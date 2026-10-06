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

#ifndef CORDOVA_EXPOSED_JS_API_H
#define CORDOVA_EXPOSED_JS_API_H


#include "CordovaBridge.h"
#include <string>
#include "web/arkweb_type.h"

/**
 * @brief Native object exposed to the JavaScript side, accessed via Engine's static functions to enter Cordova classes.
 */
class CordovaExposedJsApi {
    static const unsigned int LOG_PRINT_DOMAIN;
    /**
     * Bridge object between JavaScript and Native sides.
     * This object is passed in the constructor and shared by all WebViews.
     */
    CordovaBridge* m_bridge{nullptr};
    std::string m_strWebTag;
    /**Copying and assignment are disabled*/
    CordovaExposedJsApi(const CordovaExposedJsApi&);
    CordovaExposedJsApi& operator=(const CordovaExposedJsApi&);

public:
    CordovaExposedJsApi(CordovaBridge* bridge, const std::string& strWebTag);
    ~CordovaExposedJsApi();
    std::string GetWebTag();

    CordovaBridge* getCordovaBridge()
    {
        return m_bridge;
    }

    /**
     * @brief exec function called from JavaScript to C++, entering C++ object functions from Engine's static function.
     * @param dataArray Array data passed from JavaScript.
     * @param arraySize Size of the array.
     * @return Return value is unused (updated in API levels 18 and 20).
     */
    std::string exec(const ArkWeb_JavaScriptBridgeData* dataArray, const int32_t arraySize);
    /**
     * @brief Sets the bridge mode，setNativeToJsBridgeMode function called from JavaScript to C++, entering C++ object
     * functions from Engine's static function. In HarmonyOS, the EVAL_BRIDGE mode is enforced, replacing POLLING,
     * LOAD_URL, and ONLINE_EVENT modes. This function is now obsolete. EVAL_BRIDGE offers the highest efficiency and is
     * compatible with all HarmonyOS versions and Cordova implementations.
     * @param dataArray Array data passed from JavaScript.
     * @param arraySize Size of the array.
     */
    void setNativeToJsBridgeMode(const ArkWeb_JavaScriptBridgeData* dataArray, const int32_t arraySize);
    /**
     * @brief Retrieves JS messages, retrieveJsMessages function called from JavaScript to C++, entering C++ object
     * functions from Engine's static function. Since HarmonyOS now enforces EVAL_BRIDGE mode, this function is
     * obsolete.
     * @param dataArray Array data passed from JavaScript.
     * @param arraySize Size of the array.
     */
    void retrieveJsMessages(const ArkWeb_JavaScriptBridgeData* dataArray, const int32_t arraySize);
};

#endif