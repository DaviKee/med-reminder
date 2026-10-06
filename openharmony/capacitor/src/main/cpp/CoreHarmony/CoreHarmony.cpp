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

#include "CoreHarmony.h"
#include "CordovaViewController.h"
#include "PluginManager.h"
#include "cJSON.h"
#include "FileCache.h"

REGISTER_PLUGIN_CLASS(CoreHarmony)

void CoreHarmony::fireJavascriptEvent(std::string action)
{
    sendEventMessage(action, "");
}

bool CoreHarmony::execute(const std::string&  action, cJSON* args, CallbackContext cbc)
{
    if (action == "onArKTsResult") {
        return onArKTsResult(args);
    }
    
    PluginResult::Status status = PluginResult::Status::OK;
    std::string result = "";
    if (action == "clearCache") {
        FileCache::clearDirectory(Application::g_strCachePath.c_str());
        executeArkTs("/pages/ClearCache", 1, "", "CoreHarmony", cbc);
    } else if (action == "show") {
        std::thread([]() {
            ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->postMessage("spinner", "stop");
        }).detach();
    } else if (action == "loadUrl") {
        if (cJSON_GetArraySize(args) < 2) {
            return false;
        }
        std::string strUrl = cJSON_GetArrayItem(args, 0)->valuestring;
        cJSON* pArgs =  nullptr;
        if (cJSON_GetArraySize(args) > 1) {
            pArgs = cJSON_Duplicate(cJSON_GetArrayItem(args, 1), 1);
        } else {
            pArgs = cJSON_CreateArray();
        }
        cJSON* pJson = cJSON_CreateObject();
        cJSON_AddStringToObject(pJson, "url", strUrl.c_str());
        cJSON_AddItemToObject(pJson, "args", pArgs);
        char* pStr = cJSON_Print(pJson);
        executeArkTs("/pages/LoadUrl", 1, pStr, "CoreHarmony", cbc);
        cJSON_Delete(pJson);
        free(pStr);
    } else if (action == "cancelLoadUrl") {
        //无需实现
    } else if (action == "clearHistory") {
        executeArkTs("/pages/ClearHistory", 1, "", "CoreHarmony", cbc);
    } else if (action == "backHistory") {
       executeArkTs("/pages/BackHistory", 1, "", "CoreHarmony", cbc);
    } else if (action == "overrideButton") {
        if (cJSON_GetArraySize(args) < 2) {
            return false;
        }
        std::string strButton = cJSON_GetArrayItem(args, 0)->valuestring;
        bool isOverride = true;
        if (cJSON_GetArrayItem(args, 1)->type == cJSON_False) {
            isOverride = false;
        }
        cJSON* pJson = cJSON_CreateObject();
        cJSON_AddStringToObject(pJson, "button", strButton.c_str());
        if (isOverride) {
            cJSON_AddTrueToObject(pJson, "override");
        } else {
            cJSON_AddFalseToObject(pJson, "override");
        }
        char* pStr = cJSON_Print(pJson);
        executeArkTs("/pages/OverrideButton", 0, pStr, "CoreHarmony", cbc);
        cJSON_Delete(pJson);
        free(pStr);
    } else if (action == "overrideBackbutton") {
        if (cJSON_GetArraySize(args) < 1) {
            return false;
        }
        //鸿蒙没有返回按键，虽然有侧滑返回，但web组件捕获不到，需要在父组件onBackPress处理，如果要侧滑返回使用backHistory，或者参考MainPageOnBackPress类提供的返回操作
        int isOverride = 1;
        if (cJSON_GetArrayItem(args, 0)->type == cJSON_False) {
            isOverride = 0;
        }
        executeArkTs("/pages/OverrideBackButton", isOverride, "", "CoreHarmony", cbc);
    } else if (action == "exitApp") {
        return executeArkTs("/pages/Exit", 0, "", "CoreHarmony", cbc);
    } else if (action == "messageChannel") {
        std::string strWebTag = cbc.getWebTag();
        if (!strWebTag.empty()) {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_mapWebTagToChanel[cbc.getWebTag()] = cbc;
        }
        if (m_pendingPause.getStatus() != -1) {
            sendEventMessage(m_pendingPause, strWebTag);
            m_pendingPause.setStatus(-1);
        }
        if (m_pendingResume.getStatus() != -1) {
            sendEventMessage(m_pendingResume, strWebTag);
            m_pendingResume.setStatus(-1);
        }
        return true;
    }
    
    PluginResult pluginResult(status, result);
    cbc.sendPluginResult(pluginResult);
    return true;
}

void CoreHarmony::sendEventMessage(const std::string& action, const std::string& webTag)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "action", action.c_str());
    PluginResult result(PluginResult::Status::OK, json);
    if (m_mapWebTagToChanel.empty()) {
        if (action == "pause") {
            m_pendingPause = result;
        } else if (action == "resume") {
            //When starting normally onPause then OnResume is called
            //app启动不发送resume事件
            PluginResult tmp;
            m_pendingPause = tmp;
        }
    } else {
        sendEventMessage(result,webTag);
    }
}
void CoreHarmony::sendEventMessage(PluginResult payload, const std::string& webTag)
{
    payload.setKeepCallback(true);
    if (!webTag.empty() && m_mapWebTagToChanel.find(webTag) != m_mapWebTagToChanel.end()) {
        CallbackContext& cbc = m_mapWebTagToChanel[webTag];
        cbc.sendPluginResult(payload);
        return;
    }
    
    for (auto it = m_mapWebTagToChanel.begin(); it != m_mapWebTagToChanel.end(); ++it) {
        if (Application::isValidWebTag(it->first)) {
             CallbackContext &cbc = it->second;
            if (cbc.getQueue() != nullptr) {
                cbc.sendPluginResult(payload);
            }
        }
    }
}

void CoreHarmony::onDestroy(const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_mapWebTagToChanel.erase(strWebTag);
    return;
}

bool CoreHarmony::onArKTsResult(cJSON* args)
{
    cJSON* content = cJSON_GetObjectItem(args, "content");
    std::string action = "";
    if (content != nullptr) {
        action = content->valuestring;
    }
    
    if (action == "start") {
        cJSON* json = cJSON_GetObjectItem(args, "result");
        std::string webTag = "";    
        if (json != nullptr && json->type == cJSON_Array) {
            int count = cJSON_GetArraySize(json);
            if (count > 0) {
                webTag = cJSON_GetArrayItem(json, 0)->valuestring;
            }
        }
        if (Application::g_cordovaViewController != nullptr) { //完成初始化后通知
            ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->onStart(webTag);
            ((CordovaViewController*)Application::g_cordovaViewController)->getCapacitorPluginManager()->onStart(webTag);
        }
    } else if (action == "pendingPause") {
        cJSON* json = cJSON_GetObjectItem(args, "result");
        std::string webTag = "";
        if (json != nullptr && json->type == cJSON_Array) {
            int count = cJSON_GetArraySize(json);
            if (count > 0) {
                webTag = cJSON_GetArrayItem(json, 0)->valuestring;
            }
        }
        if (Application::g_cordovaViewController != nullptr) { //完成初始化后通知
            m_keepRunning = m_preferences->getBoolean("KeepRunning", true);
            ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->onPause(m_keepRunning);
            ((CordovaViewController*)Application::g_cordovaViewController)->getCapacitorPluginManager()->onPause(webTag);
        }
        sendEventMessage("pause", webTag);
    } else if (action == "resume") {
        cJSON* json = cJSON_GetObjectItem(args, "result");
        std::string webTag = "";
        if (json != nullptr && json->type == cJSON_Array) {
            int count = cJSON_GetArraySize(json);
            if (count > 0) {
                webTag = cJSON_GetArrayItem(json, 0)->valuestring;
            }
        }
        if (Application::g_cordovaViewController != nullptr) { //完成初始化后通知
            m_keepRunning = m_preferences->getBoolean("KeepRunning", true);
            ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->onResume(m_keepRunning);
            ((CordovaViewController*)Application::g_cordovaViewController)->getCapacitorPluginManager()->onResume(webTag);
        }
        sendEventMessage("resume",webTag);
    } else  if (action == "javascript") {
        cJSON* json = cJSON_GetObjectItem(args, "result");
        if (json != nullptr && json->type == cJSON_Array) {
            int count = cJSON_GetArraySize(json);
            std::string data;
            if (count > 0) {
                data = cJSON_GetArrayItem(json, 0)->valuestring;
            }
            std::string strWebTag;
            if (count > 1) {
                strWebTag = cJSON_GetArrayItem(json, 1)->valuestring;
            }
            (static_cast<CordovaViewController*>(Application::g_cordovaViewController))->addJavaScript(strWebTag, data);
        }
    } else if (action == "ringing") {
        ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->postMessage("telephone", "ringing");
    } else if (action == "offhook") {
        ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->postMessage("telephone", "offhook");
    } else if (action == "idle") {
        ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->postMessage("telephone", "idle");
    } else if (action == "volumedownbutton" || action == "volumeupbutton" ||action == "searchbutton" || action == "menubutton" || action == "backbutton") {
        fireJavascriptEvent(action);
    } else if (action == "postMessage") {
        cJSON* json = cJSON_GetObjectItem(args, "result");
        if (json != nullptr && json->type == cJSON_Array) {
            int count = cJSON_GetArraySize(json);
            std::string id;
            if (count > 0) {
                id = cJSON_GetArrayItem(json, 0)->valuestring;
            }
            
            std::string data;
            if (count > 1) {
                data = cJSON_GetArrayItem(json, 1)->valuestring;
            }
            ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->postMessage(id, data);
        }
    } else if (action == "overrideBackbutton") {
        sendEventMessage("backbutton", "");
    }
    return true;
}
