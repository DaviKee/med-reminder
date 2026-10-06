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

#ifndef CORDOVA_WEB_VIEW_ENGINE_H
#define CORDOVA_WEB_VIEW_ENGINE_H

#include "CordovaExposedJsApi.h"

/**
 * @brief Registers objects and functions for direct JavaScript-to-Native communication.
 *        Subclasses inheriting this class must override exposeJsInterface to implement specific objects and functions.
 */
class CordovaWebViewEngine {
    static const unsigned int LOG_PRINT_DOMAIN;
    /**JSAPI, passed in the constructor.*/
    CordovaExposedJsApi* m_jsAPI{nullptr};
    /**web tag std::string*/
    std::string m_strWebTag;
    /**Copying and assignment are disabled*/
    CordovaWebViewEngine(const CordovaWebViewEngine&);
    CordovaWebViewEngine& operator=(const CordovaWebViewEngine&);

public:
    CordovaWebViewEngine(CordovaExposedJsApi* jsAPI, const std::string& strWebTag);
    ~CordovaWebViewEngine();
    std::string GetWebTag();
    /**
     * @brief Gets the API object exposed to the JavaScript side.
     *      Primarily used when JavaScript enters Cordova via onPrompt and needs to retrieve the bridge object through
     * JsAPI.
     * @return Returns the JSAPI object.
     */
    CordovaExposedJsApi* getJsAPI();
    /**
     * @brief JavaScript to Native communication in HarmonyOS uses JS_OBJECT bridge mode (Cordova's default).
     *        Since exec has no return value in HarmonyOS, it never falls back to PROMPT mode(except when exec returns
     * "@Null arguments.").
     * @param webTag Web tag passed from JavaScript.
     * @param dataArray Array data passed from JavaScript.
     * @param arraySize Size of the array.
     * @param userData User data (pointer to m_jsAPI (CordovaExposedJsApi) passed during object registration).
     * @return No return value. After execution, the androidExec function in cordova.js receives null and does not
     * handle any error messages or JS polling.
     */
    static void
    exec(const char* webTag, const ArkWeb_JavaScriptBridgeData* dataArray, size_t arraySize, void* userData);
    /**
     * @brief Sets the bridge mode from the JavaScript side.
     *        In HarmonyOS, the EVAL_BRIDGE mode is enforced, replacing POLLING, LOAD_URL, and ONLINE_EVENT modes. This
     * function is now obsolete. EVAL_BRIDGE offers the highest efficiency and is compatible with all HarmonyOS versions
     * and Cordova implementations.
     * @param webTag web tag std::string
     * @param dataArray Array data passed from JavaScript.
     * @param arraySize Size of the array.
     * @param userData  User data (pointer to m_jsAPI (CordovaExposedJsApi) passed during object registration).
     */
    static void setNativeToJsBridgeMode(const char* webTag,
                                        const ArkWeb_JavaScriptBridgeData* dataArray,
                                        size_t arraySize,
                                        void* userData);
    /**
     * @brief Retrieves JS messages from the JavaScript side (used in ONLINE_EVENT bridge mode).
     *        Since HarmonyOS now enforces EVAL_BRIDGE mode, this function is obsolete.
     * @param webTag web tag std::string
     * @param dataArray Array data passed from JavaScript.
     * @param arraySize Size of the array.
     * @param userData User data (pointer to m_jsAPI (CordovaExposedJsApi) passed during object registration).
     */
    static void retrieveJsMessages(const char* webTag,
                                   const ArkWeb_JavaScriptBridgeData* dataArray,
                                   size_t arraySize,
                                   void* userData);
    /**
     * @brief Registers objects and functions from the C++ side into the WebView, serving as the entry point for
     * JavaScript to call C++.
     */
    virtual void exposeJsInterface();
};

#endif