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

#ifndef CORDOVA_VIEW_CONTROLLER_H
#define CORDOVA_VIEW_CONTROLLER_H

#include "ConfigXmlParser.h"
#include "CordovaBridge.h"
#include "PluginEntry.h"
#include "CordovaWebViewEngine.h"

#include "getcapacitor/BridgeController.h"

/**
 * @brief defines all resource control classes for the Cordova C++ side.
 * Registers objects and creates plugin instances during WebView initialization.
 * Destroys related objects during WebView destruction.
 */
class CordovaViewController {
    static const unsigned int LOG_PRINT_DOMAIN;
    /**JavaScript execution queue object. All WebViews share a single queue.*/
    NativeToJsMessageQueue* m_queue;
    /**
     * Plugin management object
     * Manages all C++ side plugins (each C++ plugin shares one object across all WebViews).
     * Manages all ArkTS side plugins (maintains one object per plugin name; the ArkTS side distinguishes plugins when
     * called from C++). It is recommended to maintain only one TsCordovaPlugin object on the C++ side in future
     * versions.
     */
    PluginManager* m_pluginManager;
    /**
     * JavaScript-to-C++ bridge object, primarily used to invoke specific plugin methods.
     * All WebViews share this object. The actual bridging is handled by the Engine.
     * HarmonyOS uses JS_OBJECT bridge mode for JavaScript-to-C++ communication.
     */
    CordovaBridge* m_bridge;
    /**Parses config.xml. All WebViews share a single instance of this object.*/
    ConfigXmlParser* m_xmlParser;
    /**
     * Stores the JS API and Engine instances associated with a WebView.
     * These are destroyed when the WebView is destroyed.
     */
    std::map<std::string, CordovaExposedJsApi*> m_mapWebTagJsApi;
    std::map<std::string, CordovaWebViewEngine*> m_mapWebTagEngine;

    Capacitor::PluginManager* m_capacitorPluginManager;
    MessageQueue* m_capacitorMessageQueue;
    std::map<std::string, BridgeController*> m_mapCapacitorBridge;

    class CheckCertificates : public CThread {
        std::string m_strBundleName;
        std::string m_strAppId;
        bool checkCert(void* pX509);

    public:
        void execute(void*) override;
    };

    CheckCertificates m_checkCertificate;

public:
    CordovaViewController();
    ~CordovaViewController();
    /**
     * @brief Loads the config.xml configuration file.
     */
    void loadConfig();
    /**
     * @brief Cordova initialization function.
     * @param strWebTag Web tag std::string.
     */
    void init(const std::string& strWebTag);
    /**
     * @brief Function executed before the page is displayed.
     * @param strWebTag Web tag std::string.
     */
    void loadUrl(const std::string& strWebTag);
    /**
     * @brief Returns the config.xml property configuration object.
     * @return Preferences object
     */
    CordovaPreferences* getCordovaPreferences();
    /**
     * @brief Sets the WebView callback function.
     * @param engine JavaScript-to-C++ engine.
     * @param strWebTag Web tag std::string.
     */
    void SetComponentCallback(CordovaWebViewEngine* engine, const std::string& strWebTag);
    /**
     * @brief Notifies the C++ plugin of the execution result from the ArkTS side, allowing the C++ side to complete
     * subsequent tasks.
     * @param strWebTag Web tag std::string
     * @param pluginName Plugin name.
     * @param strAction Action std::string.（usually "onArKTsResult" when ArkTS notifies the C++ plugin）
     * @param strArgs Argument std::string.
     */
    void execPluginResult(const std::string& strWebTag,
                          const std::string& pluginName,
                          const std::string& strAction,
                          const std::string& strArgs);
    /**
     * @brief Triggered when WebView's onControllerAttached is called. Initializes corresponding objects on the C++
     * side.
     * @param webTag Web tag std::string.
     * @param userData User data, typically a pointer to the CordovaWebViewEngine object.
     */
    static void onWebControllerAttached(const char* webTag, void* userData);
    /**
     * @brief Triggered when the web page starts loading on the C++ side (Web lifecycle function). Typically uses the
     * corresponding function on the ArkTS side.
     * @param webTag Web tag std::string.
     * @param userData User data, typically a pointer to the CordovaWebViewEngine object.
     */
    static void onWebPageBegin(const char* webTag, void* userData);
    /**
     * @brief Triggered when the web page finishes loading on the C++ side (Web lifecycle function). Typically uses the
     * corresponding function on the ArkTS side.
     * @param webTag webTag Web tag std::string.
     * @param userData User data, typically a pointer to the CordovaWebViewEngine object.
     */
    static void onWebPageEnd(const char* webTag, void* userData);
    /**
     * @brief Triggered when the WebView is destroyed on the C++ side. Destroys the corresponding objects associated
     * with the WebView.
     * @param webTag webTag Web tag std::string.
     * @param userData User data, typically a pointer to the CordovaWebViewEngine object.
     */
    static void onWebDestroy(const char* webTag, void* userData);
    /**
     * @brief Retrieves the CordovaWebViewEngine based on the web tag.
     * @param strWebTag web tag std::string
     * @return CordovaWebViewEngine pointer
     */
    CordovaWebViewEngine* getCordovaWebViewEngine(const std::string& strWebTag);
    std::vector<PluginEntry>* getPluginEntry();
    void instantiateTsPlugin(const std::string& strWebTag, cJSON* pJson);
    void InitCapacitorTsPlugin(const std::string& strWebTag, const std::string& strJson);
    void addJavaScript(const std::string& strWebTag, const std::string& statement);
    std::vector<std::string>* getHostName();
    std::vector<std::string>* getConfigHostName();
    void setProtocolUrl(const std::vector<std::string>& vecProtocol);
    std::vector<std::string>* getProtocolUrl();
    std::string getChcpConfigUrl();
    bool getAllAllowUpdatesAutoDownload();
    bool getAllowUpdatesAutoInstall();
    int getActiveInterfaceVersion();
    void getCameraImageCompress(bool& isCameraImageCompress,
                                long& lngCompressImageSize,
                                std::string& strCameraCompressShowToast);
    PluginManager* getPluginManager();
    void setCordovaCacheDuration(const long lngCordovaCacheDuration);
    long getCordovaCacheDuration();
    std::string getIonicHostName();
    std::string getIonicScheme();
    void setClientCert(const std::string& strUrl);
    bool getClientCert(const std::string& strUrl, std::string& strP12, std::string& strPassword);
    Capacitor::PluginManager* getCapacitorPluginManager();
    Bridge* getCapacitorBridge(const std::string& strWebTag);
};
#endif