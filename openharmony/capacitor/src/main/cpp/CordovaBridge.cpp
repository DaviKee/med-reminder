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

#include "CordovaBridge.h"
#include "cJSON.h"

const unsigned int CordovaBridge::LOG_PRINT_DOMAIN = 0xFF00;

CordovaBridge::CordovaBridge(PluginManager* pluginManager, NativeToJsMessageQueue* jsMessageQueue)
    : m_pluginManager(pluginManager), m_jsMessageQueue(jsMessageQueue)
{
}

CordovaBridge::~CordovaBridge() {}

std::string CordovaBridge::jsExec(const std::string& strWebTag,
                                  const int bridgeSecret,
                                  const std::string& service,
                                  const std::string& action,
                                  const std::string& callbackId,
                                  const std::string& arguments)
{
    if (!verifySecret(strWebTag, action, bridgeSecret)) {
        return "verifySecret failed";
    }

    if (arguments.empty()) {
        return "arguments is null";
    }

    m_jsMessageQueue->setPaused(true);
    m_pluginManager->exec(strWebTag, service, action, callbackId, arguments);
    m_jsMessageQueue->setPaused(false);
    return "";
}

// 鸿蒙强制使用 EVAL_BRIDGE，因此该函数无用
void CordovaBridge::jsSetNativeToJsBridgeMode(const std::string& strWebTag, const int bridgeSecret, const int value)
{
    return;
}

// 鸿蒙强制使用EVAL_BRIDGE，不再使用ONLINE_EVENT的交接模式，因此该函数无用
std::string
CordovaBridge::jsRetrieveJsMessages(const std::string& strWebTag, const int bridgeSecret, const bool fromOnlineEvent)
{
    if (!verifySecret(strWebTag, "jsRetrieveJsMessages()", bridgeSecret)) {
        return "";
    }
    return m_jsMessageQueue->popAndEncode(fromOnlineEvent);
}

bool CordovaBridge::verifySecret(const std::string& strWebTag, const std::string& action, const int bridgeSecret)
{
    if (!m_jsMessageQueue->isBridgeEnabled(strWebTag)) {
        return false;
    }
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_mapWebTagToBridgeSecret.find(strWebTag) != m_mapWebTagToBridgeSecret.end()) {
        if (m_mapWebTagToBridgeSecret[strWebTag] != bridgeSecret) {
            OH_LOG_Print(LOG_APP,
                         LOG_WARN,
                         LOG_PRINT_DOMAIN,
                         "ArkWeb",
                         "Bridge access attempt with wrong secret token, possibly from malicious code. Disabling "
                         "exec() bridge!");
            m_mapWebTagToBridgeSecret.erase(strWebTag);
        }
    }
    return true;
}

int CordovaBridge::generateBridgeSecret(const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    srand(time(nullptr));
    int nRandom = rand();
    m_mapWebTagToBridgeSecret[strWebTag] = nRandom;
    return nRandom;
}

std::string CordovaBridge::promptOnJsPrompt(const std::string& strWebTag,
                                            const std::string& origin,
                                            const std::string& message,
                                            const std::string& defaultValue)
{
    if (!defaultValue.empty() && defaultValue.find("gap:") == 0) {
        std::string strJson = defaultValue.substr(4);
        cJSON* pJsonArray = cJSON_Parse(strJson.c_str());
        if (pJsonArray != nullptr) {
            int nCount = cJSON_GetArraySize(pJsonArray);
            if (nCount >= 4) {
                int bridgeSecret = cJSON_GetArrayItem(pJsonArray, 0)->valueint;
                std::string strService = cJSON_GetArrayItem(pJsonArray, 1)->valuestring;
                std::string strAction = cJSON_GetArrayItem(pJsonArray, 2)->valuestring;
                std::string strCallbackId = cJSON_GetArrayItem(pJsonArray, 3)->valuestring;
                jsExec(strWebTag, bridgeSecret, strService, strAction, strCallbackId, message);
            }
        }
        return "";
    } else if (!defaultValue.empty() && defaultValue.find("gap_bridge_mode:") == 0) {
        int bridgeSecret = atoi(defaultValue.substr(16).c_str());
        int nValue = atoi(message.c_str());
        jsSetNativeToJsBridgeMode(strWebTag, bridgeSecret, nValue);
        return "";
    } else if (!defaultValue.empty() && defaultValue.find("gap_poll:") == 0) {
        int bridgeSecret = atoi(defaultValue.substr(9).c_str());
        int fromOnlineEvent = message == "1" ? 1 : 0;
        std::string ret = jsRetrieveJsMessages(strWebTag, bridgeSecret, fromOnlineEvent);
        return ret;
    } else if (!defaultValue.empty() && defaultValue.find("gap_init:") == 0) {
        if (m_pluginManager->shouldAllowBridgeAccess(origin)) {
            int bridgeMode = atoi(defaultValue.substr(9).c_str());
            m_jsMessageQueue->setBridgeMode(strWebTag, bridgeMode);
            int secret = generateBridgeSecret(strWebTag);
            char szBuf[64];
            snprintf(szBuf, sizeof(szBuf), "%d", secret);
            return szBuf;
        }
    }
    return "";
}