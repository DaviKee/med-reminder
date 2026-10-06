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

#include "CallbackContext.h"
#include "hilog/log.h"

CallbackContext::CallbackContext(const std::string& strWebTag,
                                 const std::string& callbackId,
                                 NativeToJsMessageQueue* queue)
{
    m_strWebTag = strWebTag;
    m_callbackId = callbackId;
    m_queue = queue;
    m_finished = false;
}

CallbackContext::~CallbackContext() {}

std::string CallbackContext::getWebTag() const
{
    return m_strWebTag;
}

std::string CallbackContext::getCallbackId() const
{
    return m_callbackId;
}

NativeToJsMessageQueue* CallbackContext::getQueue() const
{
    return m_queue;
}

void CallbackContext::sendPluginResult(const PluginResult& pluginResult)
{
    if (m_queue == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_WARN, 0xff00, "CallbackContext", "m_queue == nullptr");
        return;
    }

    if (m_finished) {
        OH_LOG_Print(LOG_APP, LOG_INFO, 0xff00, "CallbackContext", "m_finished == true");
        return;
    } else {
        m_finished = !pluginResult.getKeepCallback();
    }
    m_queue->setPaused(false);
    m_queue->addPluginResult(m_strWebTag, pluginResult, m_callbackId);
}

void CallbackContext::success(cJSON* message)
{
    PluginResult pluginResult(PluginResult::OK, message);
    sendPluginResult(pluginResult);
}

void CallbackContext::success(std::string message)
{
    PluginResult pluginResult(PluginResult::OK, message);
    sendPluginResult(pluginResult);
}

void CallbackContext::success(char message[], int length)
{
    PluginResult pluginResult(PluginResult::OK, message, length);
    sendPluginResult(pluginResult);
}

void CallbackContext::success(int message)
{
    PluginResult pluginResult(PluginResult::OK, message);
    sendPluginResult(pluginResult);
}

void CallbackContext::success()
{
    PluginResult pluginResult(PluginResult::OK);
    sendPluginResult(pluginResult);
}

void CallbackContext::error(cJSON* message)
{
    PluginResult pluginResult(PluginResult::ERROR, message);
    sendPluginResult(pluginResult);
}

void CallbackContext::error(std::string message)
{
    PluginResult pluginResult(PluginResult::ERROR, message);
    sendPluginResult(pluginResult);
}

void CallbackContext::error(int message)
{
    PluginResult pluginResult(PluginResult::ERROR, message);
    sendPluginResult(pluginResult);
}