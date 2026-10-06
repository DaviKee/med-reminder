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

#include "NativeToJsMessageQueue.h"
#include <unistd.h>

const unsigned int NativeToJsMessageQueue::LOG_PRINT_DOMAIN = 0xFF00;

NativeToJsMessageQueue::NativeToJsMessageQueue() : m_evalBridgeMode(this)
{
    m_paused = false;
}

NativeToJsMessageQueue::~NativeToJsMessageQueue() {}

int NativeToJsMessageQueue::calculatePackedMessageLength(const JsMessage& message)
{
    int messageLen = message.calculateEncodedLength();
    char szBuf[32];
    sprintf(szBuf, "%d", messageLen);
    return strlen(szBuf) + messageLen + 1;
}

void NativeToJsMessageQueue::packMessage(const JsMessage& message, std::string& sb)
{
    int len = message.calculateEncodedLength();
    char szBuf[32];
    sprintf(szBuf, "%d", len);
    sb += szBuf;
    sb += " ";
    message.encodeAsMessage(sb);
}

std::string NativeToJsMessageQueue::popAndEncode(bool fromOnlineEvent)
{
    std::string sb;
    int totalPayloadLen = 0;
    int numMessagesToSend = 0;
    int size = m_queue.size();
    for (int i = 0; i < numMessagesToSend; i++) {
        const JsMessage& message = m_queue.front();
        int messageSize = calculatePackedMessageLength(message);
        if (numMessagesToSend > 0 && COMBINED_RESPONSE_CUTOFF > 0 &&
            totalPayloadLen + messageSize > COMBINED_RESPONSE_CUTOFF) {
            break;
        }
        totalPayloadLen += messageSize;
        numMessagesToSend += 1;
        packMessage(message, sb);
        m_queue.pop();
    }

    if (!m_queue.empty()) {
        // Attach a char to indicate that there are more messages pending.
        sb += "*";
    }
    return sb;
}

std::string NativeToJsMessageQueue::popAndEncodeAsJs(std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    int totalPayloadLen = 0;
    int numMessagesToSend = 0;
    int size = m_queue.size();
    if (size == 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "NativeToJsMessageQueue m_queue.size() == 0");
        return "";
    }

    std::vector<std::string> vecTmp;
    for (int i = 0; i < size; i++) {
        const JsMessage& message = m_queue.front();
        int messageSize = message.calculateEncodedLength() + 50; // overestimate.
        if (numMessagesToSend > 0 && COMBINED_RESPONSE_CUTOFF > 0 &&
            totalPayloadLen + messageSize > COMBINED_RESPONSE_CUTOFF) {
            break;
        }
        totalPayloadLen += messageSize;
        if (!strWebTag.empty() && strWebTag != message.getWebTag()) {
            break;
        }
        numMessagesToSend += 1;
        strWebTag = message.getWebTag();
        std::string strTmp;
        message.encodeAsJsMessage(strTmp);
        vecTmp.push_back(strTmp);
        m_queue.pop();
    }

    bool willSendAllMessages = m_queue.size() == 0;

    std::string sb;
    // Wrap each statement in a try/finally so that if one throws it does
    // not affect the next.
    for (int i = 0; i < numMessagesToSend; ++i) {
        if (willSendAllMessages && (i + 1 == numMessagesToSend)) {
            sb += vecTmp[i];
        } else {
            sb += "try{";
            sb += vecTmp[i];
            sb += "}finally{";
        }
    }

    if (!willSendAllMessages) {
        sb += "window.setTimeout(function(){cordova.require('cordova/plugin/android/polling').pollOnce();},0);";
    }
    for (int i = willSendAllMessages ? 1 : 0; i < numMessagesToSend; ++i) {
        sb += "}";
    }
    OH_LOG_Print(LOG_APP,
                 LOG_INFO,
                 LOG_PRINT_DOMAIN,
                 "ArkWeb",
                 "NativeToJsMessageQueue OH_NativeArkWeb_RunJavaScript result:%{public}s",
                 sb.c_str());
    return sb;
}

void NativeToJsMessageQueue::addJavaScript(const std::string& strWebTag, const std::string& statement)
{
    JsMessage message(strWebTag, statement);
    enqueueMessage(message);
}

void NativeToJsMessageQueue::addPluginResult(const std::string& strWebTag,
                                             const PluginResult& result,
                                             std::string callbackId)
{
    if (callbackId.empty()) {
        return;
    }
    // Don't send anything if there is no result and there is no need to
    // clear the callbacks.
    bool noResult = result.getStatus() == PluginResult::Status::NO_RESULT;
    bool keepCallback = result.getKeepCallback();
    if (noResult && keepCallback) {
        OH_LOG_Print(LOG_APP,
                     LOG_INFO,
                     LOG_PRINT_DOMAIN,
                     "ArkWeb",
                     "noResult:%{public}d，keepCallback%{public}d",
                     noResult,
                     keepCallback);
        return;
    }
    JsMessage message(strWebTag, result, callbackId);
    if (FORCE_ENCODE_USING_EVAL) {
        std::string sb;
        message.encodeAsJsMessage(sb);
        JsMessage tmp(strWebTag, sb);
        message = tmp;
    }
    enqueueMessage(message);
}

void NativeToJsMessageQueue::enqueueMessage(const JsMessage& message)
{
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_queue.push(message);
    }
    if (!m_paused) {
        m_evalBridgeMode.onNativeToJsMessageAvailable();
    }
}

void NativeToJsMessageQueue::setPaused(const bool value)
{
    if (m_paused && value) {
        OH_LOG_Print(
            LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "NativeToJsMessageQueue setPaused:%{public}d", value);
        return;
    }
    m_paused = value;
    if (!m_paused) {
        m_evalBridgeMode.onNativeToJsMessageAvailable();
    }
}

void NativeToJsMessageQueue::RunJavaScriptCallback(const char* result)
{
    OH_LOG_Print(LOG_APP,
                 LOG_INFO,
                 LOG_PRINT_DOMAIN,
                 "ArkWeb",
                 "NativeToJsMessageQueue OH_NativeArkWeb_RunJavaScript result:%{public}s",
                 result);
}

// OHOS enforces EVAL_BRIDGE bridge mode, so this function is obsolete.
void NativeToJsMessageQueue::addBridgeMode(BridgeMode* pBridgeMode)
{
    return;
}

// OHOS enforces EVAL_BRIDGE bridge mode, so this function is obsolete.
void NativeToJsMessageQueue::setBridgeMode(const std::string& strWebTag, const int bridgeMode)
{
    return;
}

// OHOS enforces EVAL_BRIDGE bridge mode, so this function is obsolete.
bool NativeToJsMessageQueue::isBridgeEnabled(const std::string& strWebTag)
{
    return true;
}

void NativeToJsMessageQueue::addTimer(const std::string& jsCode, const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexTimer);
    SafeTimer* pSafeTimer = new SafeTimer(this, seqId, jsCode, 2000, [](const std::string& js) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ArkWeb", "%{public}s execution exceeds 2000ms", js.c_str());
    });
    m_threadIdToTimer[seqId] = pSafeTimer;
}

void NativeToJsMessageQueue::delTimer(const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexTimer);
    if (m_threadIdToTimer.find(seqId) != m_threadIdToTimer.end()) {
        SafeTimer* pSafeTimer = (SafeTimer*)m_threadIdToTimer[seqId];
        m_threadIdToTimer.erase(seqId);
        delete pSafeTimer;
    }
}

void NativeToJsMessageQueue::addDetachId(const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexDetach);
    m_vecDetachId.push_back(seqId);
}

bool NativeToJsMessageQueue::delDetachId(const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexDetach);
    auto it = find(m_vecDetachId.begin(), m_vecDetachId.end(), seqId);
    if (it != m_vecDetachId.end()) {
        m_vecDetachId.erase(it);
        return true;
    }
    return false;
}