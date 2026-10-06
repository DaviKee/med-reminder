/*
 * Copyright (c) 2025 Huawei Device, Inc. Ltd. and <马弓手>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NATIVE_TO_JS_MESSAGE_QUEUE_H
#define NATIVE_TO_JS_MESSAGE_QUEUE_H

#include "PluginResult.h"
#include "Thread.h"
#include "hilog/log.h"
#include "Application.h"

#include <mutex>
#include <sys/time.h>
#include <string>
#include <queue>
#include <thread>
#include <unistd.h>
#include <web/arkweb_type.h>

/**
 * @brief Holds the list of messages to be sent to the WebView.
 * All WebViews share a single queue.
 */
class NativeToJsMessageQueue {
    static const unsigned int LOG_PRINT_DOMAIN;
    std::mutex m_mutex;
    bool m_paused;
    static const int COMBINED_RESPONSE_CUTOFF = 16 * 1024 * 1024;

    /**
     * In OHOS, this cannot be modified to true and must remain false; otherwise, it will not work.
     * false: Native-to-JavaScript uses EVAL_BRIDGE mode, where JavaScript runs directly in the WebView (higher
     * efficiency). OHOS currently uses this mode. true: Native-to-JavaScript uses ONLINE_EVENT mode, where JavaScript
     * code is executed via eval() (lower efficiency). OHOS does not use this mode.
     */
    static const bool FORCE_ENCODE_USING_EVAL = false;

    /**
     * Stores the thread handle and thread lock for monitoring JavaScript callback execution time.
     */
    std::mutex m_mutexTimer;
    std::map<unsigned long, void*> m_threadIdToTimer;

    /**
     * Used to store the task ID of JavaScript callbacks for monitoring whether the execution time exceeds the timeout.
     */
    std::mutex m_mutexDetach;
    std::vector<unsigned long> m_vecDetachId;

public:
    class JsMessage {
        std::string m_strWebTag;
        std::string m_jsPayloadOrCallbackId;
        PluginResult m_pluginResult;

    public:
        JsMessage() {}

        JsMessage(const JsMessage& arg)
        {
            m_strWebTag = arg.m_strWebTag;
            m_jsPayloadOrCallbackId = arg.m_jsPayloadOrCallbackId;
            m_pluginResult = arg.m_pluginResult;
        }

        JsMessage& operator=(const JsMessage& arg)
        {
            if (this == &arg) {
                return *this;
            }
            m_strWebTag = arg.m_strWebTag;
            m_jsPayloadOrCallbackId = arg.m_jsPayloadOrCallbackId;
            m_pluginResult = arg.m_pluginResult;
            return *this;
        }

        JsMessage(const std::string strWebTag, const std::string& js)
        {
            m_strWebTag = strWebTag;
            m_jsPayloadOrCallbackId = js;
        }

        JsMessage(const std::string& strWebTag, PluginResult pluginResult, const std::string& callbackId)
        {
            m_strWebTag = strWebTag;
            m_jsPayloadOrCallbackId = callbackId;
            m_pluginResult = pluginResult;
        }

        std::string getWebTag() const
        {
            return m_strWebTag;
        }

        /**
         * @brief Calculates the JavaScript std::string length using ONLINE_EVENT mode (obsolete in OHOS).
         * @param pluginResult JavaScript object.
         * @return Calculated length.
         */
        static int calculateEncodedLengthHelper(const PluginResult& pluginResult)
        {
            switch (pluginResult.getMessageType()) {
                case PluginResult::MESSAGE_TYPE_BOOLEAN: // f or t
                case PluginResult::MESSAGE_TYPE_NULL:    // N
                    return 1;
                case PluginResult::MESSAGE_TYPE_NUMBER: // n
                    return 1 + pluginResult.getMessage().length();
                case PluginResult::MESSAGE_TYPE_STRING: // s
                    return 1 + pluginResult.getStrMessage().length();
                case PluginResult::MESSAGE_TYPE_BINARYSTRING:
                    return 1 + pluginResult.getMessage().length();
                case PluginResult::MESSAGE_TYPE_ARRAYBUFFER:
                    return 1 + pluginResult.getMessage().length();
                case PluginResult::MESSAGE_TYPE_MULTIPART: {
                    int ret = 1;
                    for (int i = 0; i < pluginResult.getMultipartMessagesSize(); i++) {
                        int length = calculateEncodedLengthHelper(pluginResult.getMultipartMessage(i));
                        char szBuf[32];
                        snprintf(szBuf, sizeof(szBuf), "%d", length);
                        int argLength = strlen(szBuf);
                        ret += argLength + 1 + length;
                    }
                    return ret;
                }
                case PluginResult::MESSAGE_TYPE_JSON:
                default:
                    return pluginResult.getMessage().length();
            }
        }

        /**
         * @brief Calculates the total JavaScript std::string length using ONLINE_EVENT mode (obsolete in OHOS).
         * @return Total length.
         */
        int calculateEncodedLength() const
        {
            if (m_pluginResult.getStatus() == -1) {
                return m_jsPayloadOrCallbackId.length() + 1;
            }
            char szBuf[32];
            snprintf(szBuf, sizeof(szBuf), "%d", m_pluginResult.getStatus());
            int statusLen = strlen(szBuf);
            int ret = 2 + statusLen + 1 + m_jsPayloadOrCallbackId.length() + 1;
            return ret + calculateEncodedLengthHelper(m_pluginResult);
        }

        /**
         * @brief Combines JavaScript strings using ONLINE_EVENT mode (obsolete in OHOS).
         * @param sb Output JavaScript std::string.
         * @param pluginResult avaScript object.
         */
        static void encodeAsMessageHelper(std::string& sb, const PluginResult& pluginResult)
        {
            switch (pluginResult.getMessageType()) {
                case PluginResult::MESSAGE_TYPE_BOOLEAN:
                    sb += (pluginResult.getMessage().substr(0, 1)); // t or f.
                    break;
                case PluginResult::MESSAGE_TYPE_NULL: // N
                    sb += "N";
                    break;
                case PluginResult::MESSAGE_TYPE_NUMBER: // n
                    sb += "n";
                    sb += pluginResult.getMessage();
                    break;
                case PluginResult::MESSAGE_TYPE_STRING: // s
                    sb += "s";
                    sb += pluginResult.getStrMessage();
                    break;
                case PluginResult::MESSAGE_TYPE_BINARYSTRING: // S
                    sb += "S";
                    sb += pluginResult.getMessage();
                    break;
                case PluginResult::MESSAGE_TYPE_ARRAYBUFFER: // A
                    sb += "A";
                    sb += pluginResult.getMessage();
                    break;
                case PluginResult::MESSAGE_TYPE_MULTIPART:
                    sb += "M";
                    for (int i = 0; i < pluginResult.getMultipartMessagesSize(); i++) {
                        const PluginResult& multipartMessage = pluginResult.getMultipartMessage(i);
                        char szBuf[32];
                        snprintf(szBuf, sizeof(szBuf), "%d", calculateEncodedLengthHelper(multipartMessage));
                        sb += szBuf;
                        sb += " ";
                        encodeAsMessageHelper(sb, multipartMessage);
                    }
                    break;
                case PluginResult::MESSAGE_TYPE_JSON:
                default:
                    sb.append(pluginResult.getMessage()); // [ or {
            }
        }

        /**
         * @brief Combines complete JavaScript strings using ONLINE_EVENT mode (obsolete in OHOS).
         * @param sb Output JavaScript std::string.
         */
        void encodeAsMessage(std::string& sb) const
        {
            if (m_pluginResult.getStatus() == -1) {
                sb += "J";
                sb += m_jsPayloadOrCallbackId;
                return;
            }
            int status = m_pluginResult.getStatus();
            bool noResult = status == PluginResult::Status::NO_RESULT;
            bool resultOk = status == PluginResult::Status::OK;
            bool keepCallback = m_pluginResult.getKeepCallback();

            sb += ((noResult || resultOk) ? "S:" : "F");
            sb += (keepCallback ? "1" : "0");
            char szBuf[32];
            snprintf(szBuf, sizeof(szBuf), "%d", status);
            sb += szBuf;
            sb += " ";
            sb += m_jsPayloadOrCallbackId;
            sb += " ";

            encodeAsMessageHelper(sb, m_pluginResult);
        }

        /**
         * @brief Combines JavaScript strings using EVAL_BRIDGE mode.
         * @param sb Output JavaScript std::string.
         */
        void buildJsMessage(std::string& sb) const
        {
            switch (m_pluginResult.getMessageType()) {
                case PluginResult::MESSAGE_TYPE_MULTIPART: {
                    int size = m_pluginResult.getMultipartMessagesSize();
                    // sb += "[";//TODO:所有数据应该作为完整的数组输出，但是Android是分开的，这里保持和Android一致
                    for (int i = 0; i < size; i++) {
                        const PluginResult& subresult = m_pluginResult.getMultipartMessage(i);
                        JsMessage submessage(m_strWebTag, subresult, m_jsPayloadOrCallbackId);
                        submessage.buildJsMessage(sb);
                        if (i < (size - 1)) {
                            sb += ",";
                        }
                    }
                    // sb += "]";
                    break;
                }
                case PluginResult::MESSAGE_TYPE_BINARYSTRING:
                    sb += "atob('";
                    sb += m_pluginResult.getMessage();
                    sb += "')";
                    break;
                case PluginResult::MESSAGE_TYPE_ARRAYBUFFER:
                    sb += "cordova.require('cordova/base64').toArrayBuffer('";
                    sb += m_pluginResult.getMessage();
                    sb += "')";
                    break;
                case PluginResult::MESSAGE_TYPE_NULL:
                    sb += "null";
                    break;
                case PluginResult::MESSAGE_TYPE_STRING:
                    sb += "'";
                    sb += m_pluginResult.getMessage();
                    sb += "'";
                    break;
                default:
                    sb += m_pluginResult.getMessage();
            }
        }

        /**
         * @brief Combines complete JavaScript strings using EVAL_BRIDGE mode.
         * @param sb Output JavaScript std::string.
         */
        void encodeAsJsMessage(std::string& sb) const
        {
            if (m_pluginResult.getStatus() == -1) {
                sb += m_jsPayloadOrCallbackId;
            } else {
                int status = m_pluginResult.getStatus();
                bool success = (status == PluginResult::Status::OK) || (status == PluginResult::Status::NO_RESULT);
                sb += "cordova.callbackFromNative('";
                sb += m_jsPayloadOrCallbackId;
                sb += "',";
                if (success) {
                    sb += "true";
                } else {
                    sb += "false";
                }
                sb += ",";
                char szBuf[32];
                snprintf(szBuf, sizeof(szBuf), "%d", status);
                sb += szBuf;
                sb += ",[";
                buildJsMessage(sb);
                sb += "],";
                if (m_pluginResult.getKeepCallback()) {
                    sb += "true";
                } else {
                    sb += "false";
                }
                sb += ");";
            }
        }
    };

    /**
     * @brief This is a timeout detection thread primarily used to check if JavaScript callback execution has timed out.
     * The implementation principle is as follows: （1）A timing thread is created in the constructor. After nMilSec
     * milliseconds, it checks the task thread's seqId. If detaching the task fails, it means the task thread is still
     * running, and the handler function is executed. （2）The destructor checks if the timing thread can be detached.
     *      If detachable, the timing thread is still running. The task thread's seqId is added, indicating the task
     * thread has ended and needs to be detached. If not detachable, the timing thread has already ended, meaning the
     * task thread timed out. In this case, the destructor does nothing.
     */
    class SafeTimer {
    public:
        SafeTimer(NativeToJsMessageQueue* jsQueue,
                  unsigned long seqId,
                  const std::string& jsCode,
                  const int nMilSec,
                  std::function<void(const std::string&)> handler)
            : m_jsQueue(jsQueue), m_seqId(seqId)
        {
            m_thread = std::thread([jsQueue, seqId, jsCode, nMilSec, handler]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(nMilSec));
                if (!jsQueue->delDetachId(seqId))
                    handler(jsCode);
            });
        }

        ~SafeTimer()
        {
            if (m_thread.joinable()) {
                m_jsQueue->addDetachId(m_seqId);
                m_thread.detach();
            }
        }

    private:
        std::thread m_thread;
        NativeToJsMessageQueue* m_jsQueue{nullptr};
        unsigned long m_seqId{0};
    };

    /**
     * @brief Defines the EVAL_BRIDGE mode class.
     *    OHOS enforces EVAL_BRIDGE mode and disables POLLING, LOAD_URL, and ONLINE_EVENT modes.
     */
    class BridgeMode {
    public:
        virtual void onNativeToJsMessageAvailable() = 0;

        void notifyOffPlush(NativeToJsMessageQueue* queue, bool formOnlineEvent) {}

        void reset() {}
    };

    class EvalBridgeMode : public BridgeMode, public CThread {
        /**
         * Callback object from the JavaScript side after executing JavaScript.
         */
        struct SCallBackUserData {
            unsigned long m_seqId;
            NativeToJsMessageQueue* m_queue{nullptr};
        };

        /**Queue of JavaScript statements to be executed.*/
        NativeToJsMessageQueue* m_queue;

    public:
        void execute(void*) override
        {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridgeMode", "Start Execute js");
            std::string strWebTag;
            std::string js = m_queue->popAndEncodeAsJs(strWebTag);
            if (!js.empty()) {
                unsigned long seqId = getSeqId();
                /*
                 *Adds a timer; if execution exceeds 2000ms, only logs are printed.
                 *JavaScript execution time is only a reference for developers to evaluate system efficiency.
                 *For example, if JS involves UI interaction, whether it times out depends on user operations.
                 */
                m_queue->addTimer(js, seqId);
                evaluateJavascript(strWebTag, js, seqId);
            }
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridgeMode", "End Execute js");
        }

        explicit EvalBridgeMode(NativeToJsMessageQueue* queue)
        {
            m_queue = queue;
        }

        /**
         * @brief Starts a thread to execute the task after adding it to the queue.
         * Can run multiple times concurrently.
         */
        void onNativeToJsMessageAvailable() override
        {
            start(nullptr, false);
        }

        /**
         * @brief JavaScript execution callback function.
         * @param webTag Web tag std::string.
         * @param data Data returned after JavaScript execution on the JS side.
         * @param userData Pointer to the SCallBackUserData object passed during JavaScript execution.
         */
        static void
        StaticRunJavaScriptCallback(const char* webTag, const ArkWeb_JavaScriptBridgeData* data, void* userData)
        {
            OH_LOG_Print(LOG_APP,
                         LOG_INFO,
                         LOG_PRINT_DOMAIN,
                         "EvalBridgeMode",
                         "EvalBridgeMode StaticRunJavaScriptCallback webTag:%{public}s",
                         webTag);
            if (!userData) {
                OH_LOG_Print(LOG_APP,
                             LOG_INFO,
                             LOG_PRINT_DOMAIN,
                             "EvalBridgeMode",
                             "EvalBridgeMode StaticRunJavaScriptCallback userData is nullptr");
                return;
            }
            SCallBackUserData* pUserData = (SCallBackUserData*)userData;
            pUserData->m_queue->delTimer(pUserData->m_seqId);
            std::string result((char*)data->buffer, data->size);
            pUserData->m_queue->RunJavaScriptCallback(result.c_str());
            delete pUserData;
        }

        /**
         * @brief Executes JavaScript.
         * @param strWebTag Web tag std::string.
         * @param jsCode JavaScript code to execute.
         * @param seqId Task ID for execution.
         */
        void evaluateJavascript(const std::string& strWebTag, const std::string& jsCode, unsigned long seqId)
        {
            if (strWebTag.empty()) {
                return;
            }
            OH_LOG_Print(
                LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridgeMode", "evaluateJavascript:%{public}s", jsCode.c_str());
            SCallBackUserData* pUserData = new SCallBackUserData();
            pUserData->m_queue = m_queue;
            pUserData->m_seqId = seqId;
            ArkWeb_JavaScriptObject object = {(uint8_t*)jsCode.c_str(),
                                              jsCode.size(),
                                              &EvalBridgeMode::StaticRunJavaScriptCallback,
                                              static_cast<void*>(pUserData)};
            Application::g_controller->runJavaScript(strWebTag.c_str(), &object);
        }

        /**
         * @brief Generates a task sequence based on execution time.
         * @return Task sequence.
         */
        unsigned long getSeqId()
        {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            unsigned long microseconds = tv.tv_sec * 1000000 + tv.tv_usec;
            return microseconds;
        }
    };

    EvalBridgeMode m_evalBridgeMode;

private:
    std::queue<JsMessage> m_queue;

public:
    NativeToJsMessageQueue();
    ~NativeToJsMessageQueue();
    /**
     * @brief Calculates JavaScript std::string length in ONLINE_EVENT mode (obsolete in OHOS).
     * @param message JavaScript message object.
     * @return Length value.
     */
    int calculatePackedMessageLength(const JsMessage& message);
    /**
     * @brief Generates JavaScript strings in ONLINE_EVENT mode (obsolete in OHOS).
     * @param message JavaScript message object.
     * @param sb Output JavaScript std::string.
     */
    void packMessage(const JsMessage& message, std::string& sb);
    /**
     * @brief This function is primarily used to combine JavaScript strings for ONLINE_EVENT bridge mode from Native to
     * JavaScript. Since OHOS uses EVAL_BRIDGE mode, this function is obsolete.
     * @param fromOnlineEvent Online event flag.
     * @return Returns a JavaScript std::string to be executed with eval() on the JavaScript side.
     */
    std::string popAndEncode(bool fromOnlineEvent);
    /**
     * @brief Retrieves a JavaScript message from the queue and combines it into an executable JavaScript std::string.
     * @param strWebTag Output web tag std::string, indicating in which WebView it will be executed.
     * @return Executable JavaScript std::string.
     */
    std::string popAndEncodeAsJs(std::string& strWebTag);
    /**
     * @brief Directly executes a JavaScript statement.
     * @param Web tag std::string
     * @param statement JavaScript statement. In this case, pluginResult's status is set to -1.
     */
    void addJavaScript(const std::string& strWebTag, const std::string& statement);
    /**
     * @brief Notifies the JavaScript side of the plugin execution result.
     * @param strWebTag web tag std::string
     * @param result Execution result object.
     * @param callbackId JavaScript callback ID.
     */
    void addPluginResult(const std::string& strWebTag, const PluginResult& result, std::string callbackId);
    /**
     * @brief Adds a JavaScript execution object to the queue and starts a thread to execute it.
     * @param message avaScript object.
     */
    void enqueueMessage(const JsMessage& message);
    /**
     * @brief Starts a thread to execute the JavaScript queue.
     * @param value false: starts the thread; true: does nothing.
     */
    void setPaused(const bool value);
    /**
     * @brief Processes the return result from the JavaScript side.
     * @param result Result std::string returned from JavaScript.
     */
    void RunJavaScriptCallback(const char* result);
    /**
     * @brief add bridgeMode object(the EVAL_BRIDGE mode is enforced)
     * @param pBridgeMode
     */
    void addBridgeMode(BridgeMode* pBridgeMode);
    /**
     * @brief set bridge mode(the EVAL_BRIDGE mode is enforced)
     * @param strWebTag web tag std::string
     * @param bridgeMode
     */
    void setBridgeMode(const std::string& strWebTag, const int bridgeMode);
    /**
     * @brief Bridge is Enabled
     * @param strWebTag web tag std::string
     * @return true(the EVAL_BRIDGE mode is enforced)
     */
    bool isBridgeEnabled(const std::string& strWebTag);
    /**
     * @brief Adds a task to monitor JavaScript execution time on the JS side.
     * @param jsCode JavaScript code to execute.
     * @param seqId Task ID for JavaScript execution.
     */
    void addTimer(const std::string& jsCode, const unsigned long seqId);
    /**
     * @brief Removes a task monitoring JavaScript execution time on the JS side.
     * @param seqId Task ID for JavaScript execution.
     */
    void delTimer(const unsigned long seqId);
    void addDetachId(const unsigned long seqId);
    bool delDetachId(const unsigned long seqId);
};
#endif