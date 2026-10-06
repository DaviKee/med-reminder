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

#ifndef CORDOVA_BRIDGE_H
#define CORDOVA_BRIDGE_H

#include "PluginManager.h"
#include "NativeToJsMessageQueue.h"

/**
 * @brief Implements a bidirectional cross-language bridge between JavaScript and Native sides.
 *      All WebViews in HarmonyOS share a single CordovaBridge object, instantiated during CordovaViewController
 * initialization. JavaScript-to-Native communication enforces JS_OBJECT mode, while Native-to-JavaScript communication
 * enforces EVAL_BRIDGE mode.
 */
class CordovaBridge {
    static const unsigned int LOG_PRINT_DOMAIN;
    PluginManager* m_pluginManager{nullptr};
    /**Callback JavaScript queue. All WebViews share a single queue.*/
    NativeToJsMessageQueue* m_jsMessageQueue{nullptr};
    /**Operation bridge secret lock.*/
    std::mutex m_mutex;
    /**Bridge secret.*/
    std::map<std::string, int> m_mapWebTagToBridgeSecret;
    /**Copying and assignment are disabled*/
    CordovaBridge(const CordovaBridge&);
    CordovaBridge& operator=(const CordovaBridge&);

public:
    CordovaBridge(PluginManager* pluginManager, NativeToJsMessageQueue* jsMessageQueue);
    ~CordovaBridge();
    /**
     * @brief Enters the plugin via the bridge.
     * @param strWebTag Web tag std::string.
     * @param bridgeSecret Bridge secret.
     * @param service Plugin name.
     * @param action Action.
     * @param callbackId JavaScript callback function ID.
     * @param arguments Argument std::string.
     * @return Return value is unused.
     */
    std::string jsExec(const std::string& strWebTag,
                       const int bridgeSecret,
                       const std::string& service,
                       const std::string& action,
                       const std::string& callbackId,
                       const std::string& arguments);
    /**
     * @brief Sets the bridge mode and bridge secret.
     *      HarmonyOS enforces EVAL_BRIDGE mode and disables POLLING, LOAD_URL, and ONLINE_EVENT modes.
     * @param bridgeSecret Bridge secret.
     * @param value Bridge mode value (parameter is unused).
     */
    void jsSetNativeToJsBridgeMode(const std::string& strWebTag, const int bridgeSecret, const int value);
    /**
     * @brief Retrieves JS messages for ONLINE_EVENT mode,Since HarmonyOS now enforces EVAL_BRIDGE mode, this function
     * is obsolete.
     * @param bridgeSecret Bridge secret.
     * @param fromOnlineEvent Online event flag.
     * @return Returns a JavaScript std::string.（Return value is unused）
     */
    std::string jsRetrieveJsMessages(const std::string& strWebTag, const int bridgeSecret, const bool fromOnlineEvent);
    /**
     * @brief verify bridge secret
     * @param strWebTag web tag std::string
     * @param action Action.
     * @param bridgeSecret Bridge secret.
     * @return true if successful, false if failed
     */
    bool verifySecret(const std::string& strWebTag, const std::string& action, const int bridgeSecret);
    /**
     * @brief Generates a bridge secret, called during prompt initialization.
     * @param strWebTag Web tag std::string.
     * @return int Bridge secret.
     */
    int generateBridgeSecret(const std::string& strWebTag);
    /**
     * @brief This function is critical in Cordova, primarily responsible for:
     * 1，Initializing the JavaScript-Native bridge, including setting bridge type and secret.
     * 2，Handling PROMPT mode calls from JavaScript to Native side.
     * 3，Handling POLLING and ONLINE_EVENT calls from Native to JavaScript side.
     * Summary: While this function plays a central role in Android, in HarmonyOS it only sets the bridge secret.
     *          HarmonyOS enforces JS_OBJECT mode for JavaScript-to-Native and EVAL_BRIDGE mode for Native-to-JavaScript
     * communication.
     * @param strWebTg web tag std::string
     * @param origin url
     * @param message Prompt message information.
     * @param defaultValue Prompt data.
     * @return Bridge secret std::string; empty std::string indicates no information (no longer returns JavaScript
     * strings).
     */
    std::string promptOnJsPrompt(const std::string& strWebTg,
                                 const std::string& origin,
                                 const std::string& message,
                                 const std::string& defaultValue);
};
#endif