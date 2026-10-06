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

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "CordovaPlugin.h"
#include "NativeToJsMessageQueue.h"
#include "PluginEntry.h"
#include "TsCordovaPlugin/TsCordovaPlugin.h"
#include "cJSON.h"

#include <string>
#include <map>
#include <vector>

/**
 * @brief Plugin management class. Only one plugin manager instance is maintained globally, initialized in
 * CordovaViewController.
 */
class PluginManager {
    /**C++ side plugin.*/
    std::map<std::string, CordovaPlugin*> m_pluginMap;
    std::map<std::string, PluginEntry> m_entryMap;
    /**ArkTS side plugin name.*/
    std::map<std::string, std::vector<std::string> > m_mapWebTagToTsPluginNames;
    /**TS plugin instance object. All WebViews share a single TS plugin instance.*/
    TsCordovaPlugin* m_pTsCordovaPlugin{nullptr};
    /**JavaScript queue. All WebViews share a single queue.*/
    NativeToJsMessageQueue* m_queue{nullptr};
    /**Copying and assignment are disabled*/
    PluginManager(const PluginManager&);
    PluginManager& operator=(const PluginManager&);

public:
    PluginManager(NativeToJsMessageQueue* queue);

    ~PluginManager() {}

    /**
     * @brief Initializes all plugins when the WebView is first opened. Plugins are initialized only once (differs from
     * Android).
     * @param pluginEntries pluginEntries Plugin list.
     */
    void init(std::vector<PluginEntry>* pluginEntries);
    void setPluginEntries(std::vector<PluginEntry>* pluginEntries);
    /**
     * @brief Entry function for plugin execution. All plugin calls from the JavaScript side are routed through this
     * method.
     * @param strWebTag Web tag std::string, indicating which WebView initiated the plugin call.
     * @param service String containing the service to run
     * @param action String containing the action that the class is supposed to perform. This is
     *        passed to the plugin execute method and it is up to the plugin developer
     *        how to deal with it.
     * @param callbackId String containing the id of the callback that is execute in JavaScript if this is an async
     * plugin call.
     * @param rawArgs An Array literal std::string containing any arguments needed in the plugin execute method.
     */
    void exec(const std::string& strWebTag,
              const std::string& service,
              const std::string& action,
              const std::string& callbackId,
              const std::string& rawArgs);
    /**
     * @brief Plugin invocation entry, typically used to notify C++ side plugins of execution results from the ArkTS
     * side. After a C++ plugin enters the ArkTS side, the ArkTS side returns results to the C++ plugin via this
     * function. Special attention: When the C++ side calls back to the JavaScript side, the CallbackContext object is
     * used. Cache the CallbackContext on the C++ side if necessary (refer to TSCordovaPlugin).
     * @param strWebTag Web tag std::string, indicating which WebView initiated the plugin call.
     * @param service String containing the service to run (typically ArkTS-to-C++ callbacks).
     * @param action Action std::string (usually "onArKTsResult" when ArkTS notifies the C++ plugin).
     * @param rawArgs JSON-formatted data.
     */
    void exec(const std::string& strWebTag,
              const std::string& service,
              const std::string& action,
              const std::string& rawArgs);
    /**
     * @brief Retrieves the plugin object pointer by plugin name. All WebViews share the same C++ side object pointers.
     * @param service Plugin name.
     * @return Plugin object pointer.
     */
    CordovaPlugin* getPlugin(const std::string& service);
    CordovaPlugin* getTSPlugin(const std::string& strWebTag, const std::string& service);
    /**
     * @brief Called when the system is about to start resuming a previous webview.
     *      Notifies all plugins from CoreHarmony.
     * @param multitasking      Flag indicating if multitasking is turned on for app
     */
    void onPause(bool multitasking);
    /**
     * @brief Called when the webview will start interacting with the user.
     *  Notifies all plugins from CoreHarmony.
     * @param multitasking      Flag indicating if multitasking is turned on for app
     */
    void onResume(bool multitasking);
    /**
     * @brief  Called when the mainPage starts to display.
     * Triggered via CoreHarmony in MainPage's aboutToAppear.
     */
    void onStart(const std::string& strWebTag);
    /**
     * @brief page load start
     * @param strWebTag
     */
    void onPageStart(const std::string& strWebTag);
    /**
     * @brief page load end
     * @param strWebTag
     */
    void onPageEnd(const std::string& strWebTag);
    /**
     * @brief  The final call you receive before your webview is destroyed.
     * ArkTS plugins are notified in MainPage's aboutToDisappear, while C++ plugins are notified in
     * CordovaViewController.
     */
    void onDestroy(const std::string& strWebTag);
    /**
     * @brief Send a message to all plugins.
     *
     * @param id                The message id
     * @param data              The message data
     * @return                  Object to stop propagation or null
     */
    void postMessage(const std::string& id, const std::string& data);
    /**
     * @brief Creates a plugin object by plugin name.
     * @param className Plugin name.
     * @return Plugin object pointer, null if failed.
     */
    CordovaPlugin* instantiatePlugin(const std::string& className);
    void instantiateTsPlugin(const std::string& strWebTag, cJSON* pJson);
    /**
     * @brief Called when the WebView is going to request an external resource.
     * @param strUrl The URL that is being requested.
     * @return true to allow the resource to load,false to block the resource.
     */
    bool shouldAllowRequest(const std::string& strUrl);
    /**
     * @brief Called when the WebView is requesting the exec() bridge be enabled.
     * @param strUrl url std::string
     * @return true (HarmonyOS enforces EVAL_BRIDGE mode).
     */
    bool shouldAllowBridgeAccess(const std::string& strUrl);
};
#endif