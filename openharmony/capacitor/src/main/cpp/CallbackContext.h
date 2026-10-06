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

#ifndef CALLBACK_CONTEXT_H
#define CALLBACK_CONTEXT_H

#include "PluginResult.h"
#include "NativeToJsMessageQueue.h"
#include <cstddef>
#include <string>

/**
 * @brief Defines the C++ side callback to JavaScript side class.
 */
class CallbackContext {
    /**Web tag used to distinguish frontend WebViews.*/
    std::string m_strWebTag;
    /**
     * Callback ID from the JavaScript side.
     * If the communication is from Native to JavaScript (initiated by Native),
     * the callbackId is a JavaScript statement. In this case, pluginResult's status is set to -1.
     * Refer to jsMessage's encodeAsJsMessage, which combines the JavaScript std::string.
     */
    std::string m_callbackId;
    /**is finished*/
    bool m_finished;
    int m_changingThreads;
    /**Pointer to the execution queue. All plugins share a single JavaScript queue object.*/
    NativeToJsMessageQueue* m_queue{nullptr};

public:
    CallbackContext(const std::string& strWebTag, const std::string& callbackId, NativeToJsMessageQueue* queue);
    ~CallbackContext();

    CallbackContext()
    {
        m_finished = false;
        m_changingThreads = false;
        m_queue = nullptr;
        m_changingThreads = 0;
    }

    CallbackContext(const CallbackContext& arg)
    {
        m_strWebTag = arg.m_strWebTag;
        m_callbackId = arg.m_callbackId;
        m_finished = arg.m_finished;
        m_changingThreads = arg.m_changingThreads;
        m_queue = arg.m_queue;
    }

    const CallbackContext& operator=(const CallbackContext& arg)
    {
        if (this == &arg) {
            return *this;
        }
        m_strWebTag = arg.m_strWebTag;
        m_callbackId = arg.m_callbackId;
        m_finished = arg.m_finished;
        m_changingThreads = arg.m_changingThreads;
        m_queue = arg.m_queue;
        return *this;
    }

    std::string getWebTag() const;
    std::string getCallbackId() const;
    NativeToJsMessageQueue* getQueue() const;
    /**
     * @brief Sends the plugin execution result.
     * @param pluginResult Plugin result object.
     */
    void sendPluginResult(const PluginResult& pluginResult);
    void success(cJSON* message);
    void success(std::string message);
    void success(char message[], int length);
    void success(int message);
    void success();
    void error(cJSON* message);
    void error(std::string message);
    void error(int message);
};
#endif