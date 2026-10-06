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

#include "PluginManager.h"
#include "CordovaViewController.h"
#include "cJSON.h"
#include <cstddef>

PluginManager::PluginManager(NativeToJsMessageQueue* queue)
{
    m_queue = queue;
    m_pTsCordovaPlugin = new TsCordovaPlugin();
}

void PluginManager::init(std::vector<PluginEntry>* pPluginEnters)
{
    setPluginEntries(pPluginEnters);
}

void PluginManager::setPluginEntries(std::vector<PluginEntry>* pPluginEnters)
{
    std::vector<PluginEntry>& vecPluginEnters = *pPluginEnters;
    for (int i = 0; i < vecPluginEnters.size(); i++) {
        PluginEntry pluginEntry = vecPluginEnters[i];
        m_entryMap[pluginEntry.m_service] = pluginEntry;
        if (pluginEntry.m_onload) {
            CordovaPlugin* cordovaPlugin = instantiatePlugin(pluginEntry.m_pluginClass);
            m_pluginMap[pluginEntry.m_service] = cordovaPlugin;
        } else {
            m_pluginMap[pluginEntry.m_service] = nullptr;
        }
    }
}

CordovaPlugin* PluginManager::getPlugin(const std::string& service)
{
    CordovaPlugin* cordovaPlugin = nullptr;
    if (service == "CoreAndroid") {
        cordovaPlugin = m_pluginMap["CoreHarmony"];
        return cordovaPlugin;
    }
    if (m_pluginMap.find(service) != m_pluginMap.end()) {
        cordovaPlugin = m_pluginMap[service];
        if (cordovaPlugin == nullptr) {
            PluginEntry pluginEntry = m_entryMap[service];
            cordovaPlugin = instantiatePlugin(pluginEntry.m_pluginClass);
            m_pluginMap[service] = cordovaPlugin;
        }
        return cordovaPlugin;
    }

    if (m_entryMap.find(service) != m_entryMap.end()) {
        PluginEntry pluginEntry = m_entryMap[service];
        cordovaPlugin = instantiatePlugin(pluginEntry.m_pluginClass);
        if (cordovaPlugin != nullptr) {
            m_pluginMap[service] = cordovaPlugin;
        }
    }

    if (cordovaPlugin == nullptr) {
        cordovaPlugin = instantiatePlugin(service);
        if (cordovaPlugin != nullptr) {
            m_pluginMap[service] = cordovaPlugin;
        }
    }
    return cordovaPlugin;
}

CordovaPlugin* PluginManager::getTSPlugin(const std::string& strWebTag, const std::string& service)
{
    if (m_mapWebTagToTsPluginNames.find(strWebTag) != m_mapWebTagToTsPluginNames.end()) {
        std::vector<std::string>& vecPluginName = m_mapWebTagToTsPluginNames[strWebTag];
        auto it = find(vecPluginName.begin(), vecPluginName.end(), service);
        if (it != vecPluginName.end()) {
            return m_pTsCordovaPlugin;
        }
    }
    return nullptr;
}

void PluginManager::onPause(bool multitasking)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* cordovaPlugin = it->second;
        if (cordovaPlugin != nullptr) {
            cordovaPlugin->onPause(multitasking);
        }
    }
    m_pTsCordovaPlugin->onPause(multitasking);
}

void PluginManager::onResume(bool multitasking)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* cordovaPlugin = it->second;
        if (cordovaPlugin != nullptr) {
            cordovaPlugin->onResume(multitasking);
        }
    }
    m_pTsCordovaPlugin->onResume(multitasking);
}

void PluginManager::onStart(const std::string& strWebTag)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* cordovaPlugin = it->second;
        if (cordovaPlugin != nullptr) {
            cordovaPlugin->onStart(strWebTag);
        }
    }
    m_pTsCordovaPlugin->onStart(strWebTag);
}

void PluginManager::onPageStart(const std::string& strWebTag)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* cordovaPlugin = it->second;
        if (cordovaPlugin != nullptr) {
            cordovaPlugin->onPageStart(strWebTag);
        }
    }
    m_pTsCordovaPlugin->onPageStart(strWebTag);
}

void PluginManager::onPageEnd(const std::string& strWebTag)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* cordovaPlugin = it->second;
        if (cordovaPlugin != nullptr) {
            cordovaPlugin->onPageEnd(strWebTag);
        }
    }
    m_pTsCordovaPlugin->onPageEnd(strWebTag);
}

// onDestroy只通知C++侧插件，ArkTS侧插件有MainPage的aboutToDisappear函数中通知
// 因为在C++侧收到onDestroy通知后，这个时候web已经销毁
void PluginManager::onDestroy(const std::string& strWebTag)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* cordovaPlugin = it->second;
        if (cordovaPlugin != nullptr) {
            cordovaPlugin->onDestroy(strWebTag);
        }
    }
}

void PluginManager::postMessage(const std::string& id, const std::string& data)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* pCordovaPlugin = it->second;
        if (pCordovaPlugin != nullptr) {
            pCordovaPlugin->onMessage(id, data);
        }
    }
    m_pTsCordovaPlugin->onMessage(id, data);
    return;
}

CordovaPlugin* PluginManager::instantiatePlugin(const std::string& className)
{
    CordovaPlugin* pPlugin = (CordovaPlugin*)CObjectFactory::createObject(className);
    if (pPlugin != nullptr) {
        pPlugin->privateInitialize(
            className, this, ((CordovaViewController*)Application::g_cordovaViewController)->getCordovaPreferences());
    }
    return pPlugin;
}

void PluginManager::exec(const std::string& strWebTag,
                         const std::string& service,
                         const std::string& action,
                         const std::string& callbackId,
                         const std::string& rawArgs)
{
    CallbackContext callbackContext(strWebTag, callbackId, m_queue);
    CordovaPlugin* plugin = getTSPlugin(strWebTag, service);
    if (plugin != nullptr) {
        plugin->execute(service, action, rawArgs, callbackContext);
        return;
    }

    plugin = getPlugin(service);
    if (plugin == nullptr) {
        PluginResult cr(PluginResult::Status::CLASS_NOT_FOUND_EXCEPTION);
        m_queue->addPluginResult(strWebTag, cr, callbackId);
        return;
    }

    bool wasValidAction = plugin->execute(action, rawArgs, callbackContext);
    if (!wasValidAction) {
        PluginResult cr(PluginResult::Status::INVALID_ACTION);
        callbackContext.sendPluginResult(cr);
    }
}

void PluginManager::exec(const std::string& strWebTag,
                         const std::string& service,
                         const std::string& action,
                         const std::string& rawArgs)
{
    CallbackContext callbackContext(strWebTag, "", nullptr);
    CordovaPlugin* plugin = getTSPlugin(strWebTag, service);
    if (plugin != nullptr) {
        plugin->execute(service, action, rawArgs, callbackContext);
        return;
    }

    plugin = getPlugin(service);
    if (plugin == nullptr) {
        return;
    }
    plugin->execute(action, rawArgs, callbackContext);
}

void PluginManager::instantiateTsPlugin(const std::string& strWebTag, cJSON* pJson)
{
    if (m_mapWebTagToTsPluginNames.find(strWebTag) == m_mapWebTagToTsPluginNames.end()) {
        std::vector<std::string> vecPluginName;
        m_mapWebTagToTsPluginNames[strWebTag] = vecPluginName;
    }
    std::vector<std::string>& vecPluginName = m_mapWebTagToTsPluginNames[strWebTag];
    if (pJson->type == cJSON_Array) {
        int nCount = cJSON_GetArraySize(pJson);
        for (int i = 0; i < nCount; i++) {
            cJSON* pName = cJSON_GetArrayItem(pJson, i);
            if (pName != nullptr && pName->type == cJSON_String) {
                std::string strPluginName = pName->valuestring;
                auto it = find(vecPluginName.begin(), vecPluginName.end(), strPluginName);
                if (it == vecPluginName.end()) {
                    vecPluginName.push_back(strPluginName);
                }
            }
        }
    }
}

bool PluginManager::shouldAllowRequest(const std::string& strUrl)
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); it++) {
        CordovaPlugin* pCordovaPlugin = it->second;
        if (pCordovaPlugin != nullptr) {
            if (!pCordovaPlugin->shouldAllowRequest(strUrl)) {
                return false;
            }
        }
    }
    return true;
}

// 鸿蒙强制使用 EVAL_BRIDGE，返回true
bool PluginManager::shouldAllowBridgeAccess(const std::string& strUrl)
{
    return true;
}