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

#include "TsCordovaPlugin.h"
#include "cJSON.h"

const unsigned int TsCordovaPlugin::LOG_PRINT_DOMAIN = 0xFF00;
void TsCordovaPlugin::setCallBackContext(CallbackContext cbc)
{
    CGuard guard(m_cLock);
    m_mapCbc[cbc.getCallbackId()] = cbc;
}

CallbackContext TsCordovaPlugin::getCallBackContext(const std::string& strCallbackId, const bool isKeepCallBack)
{
    CGuard guard(m_cLock);
    CallbackContext cbc;
    if (m_mapCbc.find(strCallbackId) != m_mapCbc.end()) {
        cbc = m_mapCbc[strCallbackId];
        if (!isKeepCallBack) {
            m_mapCbc.erase(strCallbackId);
        }
        return cbc;
    }
    return cbc;
}

bool TsCordovaPlugin::execute(const std::string& serviceName, const std::string& action, cJSON* args, CallbackContext cbc)
{
    if (!cbc.getCallbackId().empty() && action != "onArKTsResult") {
        setCallBackContext(cbc);
    }
    
    if (action == "onArKTsResult") {
        return onArKTsResult(args);
    }
    
    std::string strAction = "/service/"+cbc.getWebTag()+"/"+action+"/"+cbc.getCallbackId();
    char* pPageArgs = cJSON_Print(args);
    executeArkTs(strAction, 0, pPageArgs, serviceName.c_str(), cbc);
    free(pPageArgs);
    return true;
}

bool TsCordovaPlugin::onArKTsResult(cJSON* args)
{
    if (args == nullptr) {
        return false;
    }
    
    cJSON* pCallbackId = cJSON_GetObjectItem(args, "callbackId");
    std::string strCallBackId = "";
    if (pCallbackId != NULL && pCallbackId->type == cJSON_String) {
        strCallBackId = pCallbackId->valuestring;
    }
    
    cJSON* pStatus = cJSON_GetObjectItem(args, "status");
    int nStatus = -1;
    if (pStatus != NULL && pStatus->type == cJSON_Number) {
        nStatus = pStatus->valueint;
    }
    
    cJSON* pMessageType = cJSON_GetObjectItem(args, "messageType");
    int nMessageType = -1;
    if (pMessageType != NULL && pMessageType->type == cJSON_Number) {
        nMessageType = pMessageType->valueint;
    }
    
    cJSON* pMessage = cJSON_GetObjectItem(args, "message");
    std::string strMessage = "";
    if (pMessage != NULL && pMessage->type == cJSON_String) {
        strMessage = pMessage->valuestring;
    }
    
    cJSON* pKeepCallBack = cJSON_GetObjectItem(args, "keepCallback");
    bool isKeepCallBack = false;
    if (pKeepCallBack != NULL && pKeepCallBack->type == cJSON_True) {
        isKeepCallBack = true;
    }
    
    CallbackContext cbc = getCallBackContext(strCallBackId, isKeepCallBack);
    if (cbc.getQueue() == NULL) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ArkWeb", "Multiple callbacks{%{public}s}，please setKeepCallback is true", strCallBackId.c_str());
        return false;
    }
    if (nStatus == PluginResult::Status::OK) {
        switch(nMessageType) {
            case PluginResult::MessageType::MESSAGE_TYPE_STRING:
            {
                PluginResult pluginResult(PluginResult::OK, strMessage);
                pluginResult.setKeepCallback(isKeepCallBack);
                cbc.sendPluginResult(pluginResult);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_JSON:
            {
                cJSON* pJson = cJSON_Parse(strMessage.c_str());
                if (pJson == NULL) {
                    PluginResult pluginResult(PluginResult::OK, strMessage);
                    pluginResult.setKeepCallback(isKeepCallBack);
                    cbc.sendPluginResult(pluginResult);
                    break;
                }
                PluginResult pluginResult(PluginResult::OK, pJson);
                pluginResult.setKeepCallback(isKeepCallBack);
                cbc.sendPluginResult(pluginResult);
                cJSON_Delete(pJson);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_NUMBER:
            {
                double dbNumber = atof(strMessage.c_str());
                PluginResult pluginResult(PluginResult::OK, dbNumber);
                pluginResult.setKeepCallback(isKeepCallBack);
                cbc.sendPluginResult(pluginResult);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_BOOLEAN:
            {
                if (strMessage == "true") {
                    PluginResult pluginResult(PluginResult::OK, true);
                    pluginResult.setKeepCallback(isKeepCallBack);
                    cbc.sendPluginResult(pluginResult);
                } else {
                    PluginResult pluginResult(PluginResult::OK, false);
                    pluginResult.setKeepCallback(isKeepCallBack);
                    cbc.sendPluginResult(pluginResult);
                }
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_NULL:
            {
                std::string strTmp;
                PluginResult pluginResult(PluginResult::OK, strTmp);
                pluginResult.setKeepCallback(isKeepCallBack);
                cbc.sendPluginResult(pluginResult);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_ARRAYBUFFER:
            {
                PluginResult pluginResult(PluginResult::Status::OK, PluginResult::MessageType::MESSAGE_TYPE_ARRAYBUFFER, strMessage);
                pluginResult.setKeepCallback(isKeepCallBack);
                cbc.sendPluginResult(pluginResult);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_BINARYSTRING:
            {
                PluginResult pluginResult(PluginResult::Status::OK, PluginResult::MessageType::MESSAGE_TYPE_BINARYSTRING, strMessage);
                pluginResult.setKeepCallback(isKeepCallBack);
                cbc.sendPluginResult(pluginResult);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_MULTIPART:
                break;
            default:
                break;
        }
    } else {
        switch(nMessageType) {
            case PluginResult::MessageType::MESSAGE_TYPE_STRING:
            {
                cbc.error(strMessage);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_JSON:
            {
                cJSON* pJson = cJSON_Parse(strMessage.c_str());
                if (pJson == NULL) {
                    cbc.error(strMessage);
                    break;
                }
                cbc.error(pJson);
                cJSON_Delete(pJson);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_NUMBER:
            {
                double dbNumber = atof(strMessage.c_str());
                cbc.error(dbNumber);
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_BOOLEAN:
            {
                if (strMessage == "true") {
                    cbc.error(true);
                } else {
                    cbc.error(false);
                }
                break;
            }
            case PluginResult::MessageType::MESSAGE_TYPE_NULL:
            case PluginResult::MessageType::MESSAGE_TYPE_ARRAYBUFFER:
            case PluginResult::MessageType::MESSAGE_TYPE_BINARYSTRING:
            case PluginResult::MessageType::MESSAGE_TYPE_MULTIPART:
            {
                cbc.error("");
                break;
            }
            default:
                break;
        }
    }
    return true;
}

void TsCordovaPlugin::onMessage(const std::string &id, const std::string& data)
{
    std::string strAction = "/service/*/onMessage/*";
    cJSON* pJson = cJSON_CreateObject();
    cJSON_AddStringToObject(pJson, "id", id.c_str());
    cJSON_AddStringToObject(pJson, "data", data.c_str());
    char* pPageArgs = cJSON_Print(pJson);
    CallbackContext cbc(Application::getFirstWebTag(), "*", nullptr);
    executeArkTsAsync(strAction, 0, pPageArgs, "", cbc);
    cJSON_Delete(pJson);
    free(pPageArgs);
}

void TsCordovaPlugin::onPause(bool multitasking)
{
    std::string strAction = "/service/*/onPause/*";
    CallbackContext cbc(Application::getFirstWebTag(), "*", nullptr);
    executeArkTsAsync(strAction, multitasking, "[]", "", cbc);
}
void TsCordovaPlugin::onResume(bool multitasking)
{
    std::string strAction = "/service/*/onResume/*";
    CallbackContext cbc(Application::getFirstWebTag(), "*", nullptr);
    executeArkTsAsync(strAction, multitasking, "[]", "", cbc);
}

void TsCordovaPlugin::onStart(const std::string& strWebTag)
{
    std::string strAction = "/service/"+strWebTag+"/onStart/*";
    CallbackContext cbc(strWebTag, "*", nullptr);
    executeArkTsAsync(strAction, 0, "[]", "", cbc);
}

void TsCordovaPlugin::onPageStart(const std::string& strWebTag)
{
    std::string strAction = "/service/"+strWebTag+"/onPageStart/*";
    CallbackContext cbc(strWebTag, "*", nullptr);
    executeArkTsAsync(strAction, 0, "[]", "", cbc);
}

void TsCordovaPlugin::onPageEnd(const std::string& strWebTag)
{
    std::string strAction = "/service/"+strWebTag+"/onPageEnd/*";
    CallbackContext cbc(strWebTag, "*", nullptr);
    executeArkTsAsync(strAction, 0, "[]", "", cbc);
}