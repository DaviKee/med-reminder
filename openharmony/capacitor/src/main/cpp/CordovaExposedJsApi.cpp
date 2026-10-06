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

#include "CordovaExposedJsApi.h"
#include "hilog/log.h"

const unsigned int CordovaExposedJsApi::LOG_PRINT_DOMAIN = 0xFF00;

CordovaExposedJsApi::CordovaExposedJsApi(CordovaBridge* bridge, const std::string& strWebTag)
{
    m_bridge = bridge;
    m_strWebTag = strWebTag;
}

CordovaExposedJsApi::~CordovaExposedJsApi() {}

std::string CordovaExposedJsApi::GetWebTag()
{
    return m_strWebTag;
}

std::string CordovaExposedJsApi::exec(const ArkWeb_JavaScriptBridgeData* dataArray, const int32_t arraySize)
{
    if (arraySize != 5) {
        OH_LOG_Print(LOG_APP,
                     LOG_INFO,
                     LOG_PRINT_DOMAIN,
                     "CordovaExposedJsApi",
                     "CordovaExposedJsApi::exec(const ArkWeb_JavaScriptBridgeData *dataArray, int32_t "
                     "arraySize(%{public}d)) error",
                     arraySize);
        return "";
    }
    std::string bridgeSecret(reinterpret_cast<const char*>(dataArray[0].buffer), dataArray[0].size);
    std::string service(reinterpret_cast<const char*>(dataArray[1].buffer), dataArray[1].size);
    std::string action(reinterpret_cast<const char*>(dataArray[2].buffer), dataArray[2].size);
    std::string callbackId(reinterpret_cast<const char*>(dataArray[3].buffer), dataArray[3].size);
    std::string arguments(reinterpret_cast<const char*>(dataArray[4].buffer), dataArray[4].size);
    int nBridgeSecret = atoi(bridgeSecret.c_str());
    OH_LOG_Print(LOG_APP,
                 LOG_INFO,
                 LOG_PRINT_DOMAIN,
                 "CordovaExposedJsApi",
                 "exec jsExec(%{public}d, %{public}s, %{public}s, %{public}s, %{public}s)",
                 nBridgeSecret,
                 service.c_str(),
                 action.c_str(),
                 callbackId.c_str(),
                 arguments.c_str());
    return m_bridge->jsExec(m_strWebTag, nBridgeSecret, service, action, callbackId, arguments);
}

void CordovaExposedJsApi::setNativeToJsBridgeMode(const ArkWeb_JavaScriptBridgeData* dataArray, const int32_t arraySize)
{
    if (arraySize != 2) {
        OH_LOG_Print(LOG_APP,
                     LOG_INFO,
                     LOG_PRINT_DOMAIN,
                     "CordovaExposedJsApi",
                     "CordovaExposedJsApi::setNativeToJsBridgeMode(const ArkWeb_JavaScriptBridgeData *dataArray, "
                     "int32_t arraySize(%{public}d)) error",
                     arraySize);
        return;
    }
    std::string bridgeSecret((char*)dataArray[0].buffer, dataArray[0].size);
    std::string value((char*)dataArray[1].buffer, dataArray[1].size);
    int nBridgeSecret = atoi(bridgeSecret.c_str());
    int nValue = atoi(value.c_str());
    m_bridge->jsSetNativeToJsBridgeMode(m_strWebTag, nBridgeSecret, nValue);
}

void CordovaExposedJsApi::retrieveJsMessages(const ArkWeb_JavaScriptBridgeData* dataArray, const int32_t arraySize)
{
    if (arraySize != 2) {
        OH_LOG_Print(LOG_APP,
                     LOG_INFO,
                     LOG_PRINT_DOMAIN,
                     "CordovaExposedJsApi",
                     "CordovaExposedJsApi::retrieveJsMessages(const ArkWeb_JavaScriptBridgeData *dataArray, int32_t "
                     "arraySize(%{public}d)) error",
                     arraySize);
        return;
    }
    std::string bridgeSecret((char*)dataArray[0].buffer, dataArray[0].size);
    std::string value((char*)dataArray[1].buffer, dataArray[1].size);
    int nBridgeSecret = atoi(bridgeSecret.c_str());
    int nValue = atoi(value.c_str());
    m_bridge->jsRetrieveJsMessages(m_strWebTag, nBridgeSecret, nValue);
}
