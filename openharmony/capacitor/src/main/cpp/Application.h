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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <mutex>
#include <rawfile/raw_file_manager.h>
#include <string>
#include <map>
#include <web/arkweb_scheme_handler.h>
#include <web/arkweb_type.h>

struct SDateTime {
    int m_nDate; // time, format:HHmmSS
    int m_nTime; // date, format:YYYYMMDD

    SDateTime() : m_nDate(0), m_nTime(0) {}

    SDateTime(int nDate, int nTime)
    {
        m_nDate = nDate;
        m_nTime = nTime;
    }

    bool operator==(const SDateTime& arg)
    {
        if (m_nDate == arg.m_nDate && m_nTime == arg.m_nTime) {
            return true;
        }
        return false;
    }

    SDateTime(const SDateTime& arg)
    {
        m_nDate = arg.m_nDate;
        m_nTime = arg.m_nTime;
    }

    SDateTime& operator=(const SDateTime& arg)
    {
        if (this == &arg) {
            return *this;
        }
        m_nDate = arg.m_nDate;
        m_nTime = arg.m_nTime;
        return *this;
    }

    bool operator>=(const SDateTime& arg) const
    {
        if (m_nDate > arg.m_nDate) {
            return true;
        } else if (m_nDate == arg.m_nDate && m_nTime >= arg.m_nTime) {
            return true;
        }
        return false;
    }

    bool operator<=(const SDateTime& arg) const
    {
        if (m_nDate < arg.m_nDate) {
            return true;
        } else if (m_nDate == arg.m_nDate && m_nTime <= arg.m_nTime) {
            return true;
        }
        return false;
    }

    bool operator<(const SDateTime& arg) const
    {
        if (m_nDate < arg.m_nDate) {
            return true;
        } else if (m_nDate == arg.m_nDate && m_nTime < arg.m_nTime) {
            return true;
        }
        return false;
    }

    bool operator>(const SDateTime& arg) const
    {
        if (m_nDate > arg.m_nDate) {
            return true;
        } else if (m_nDate == arg.m_nDate && m_nTime > arg.m_nTime) {
            return true;
        }
        return false;
    }

    bool operator!=(const SDateTime& arg) const
    {
        if (m_nDate == arg.m_nDate && m_nTime == arg.m_nTime) {
            return false;
        }
        return true;
    }

    bool operator==(const SDateTime& arg) const
    {
        return (m_nDate == arg.m_nDate && m_nTime == arg.m_nTime);
    }
};

/**
 * @brief Global object for the Cordova C++ side.
 */
class Application {
    static std::mutex s_mutex;
    static std::map<std::string, std::map<std::string, std::string> > s_mapWebTagToResource;
    static std::map<uintptr_t, std::string> s_mapSchemeHandlerToWebTag;
    static std::map<std::string, std::vector<std::string> > s_mapWebTagToCustomHttpHeaders;
    static std::map<std::string, bool> s_mapWebTagToIsAllowCredentials;
    static std::map<std::string, void*> s_mapFunctionRefresh;
    static std::map<std::string, std::string> s_mapWebTagToBasePath;

public:
    /**cache directory of webview*/
    static std::string g_strCachePath;
    /**database directory*/
    static std::string g_databaseDir;
    /**Web controller, used for controller-related APIs. All frontend WebViews can use it.*/
    static ArkWeb_ControllerAPI* g_controller;
    /**web component, used for component-related APIs. All frontend WebViews can use it.*/
    static ArkWeb_ComponentAPI* g_component;
    /**Current app's resource manager object, used for handling rawfile files.*/
    static NativeResourceManager* g_resourceManager;
    /**web cookie, used for cookie-related APIs. All frontend WebViews can use it.*/
    static ArkWeb_CookieManagerAPI* g_cookieManage;
    /**Connection pool object for obtaining socket or TLS/SSL communication.*/
    static void* g_connPoolManage;
    /**Virtual domain name. This domain is used to load rawfile files and sandbox files.*/
    static std::string g_strTmpUrl;
    /**Cross-origin request thread pool object, used for handling web requests.*/
    static void* g_httpGroupRunner;
    /**Paged memory management object.*/
    static void* g_memPool;
    /**Execution environment for calling ArkTS functions from the C++ side.*/
    static void* g_env;
    /**cordova view Controller*/
    static void* g_cordovaViewController;
    /**Push messages received before Cordova is initialized.*/
    static std::string g_strPushInfoBeforeInit;
    /**Background proxy push notifications. Messages saved before Cordova is initialized (currently unused).*/
    static std::string g_strNotificationBeforeInit;
    /**custom scheme*/
    static std::vector<std::string> g_vecCustomSchemes;
    /**WebTags of all WebViews opened by the frontend.*/
    static std::vector<std::string> g_vecWebTag;
    /**All domains using Cordova proxy for HTTP protocol handling.*/
    static std::vector<std::string> g_vecProtocolUrl;
    /**Safe function pointer for asynchronous calls from C++ to ArkTS.*/
    static napi_threadsafe_function g_tsfn;
    /**
     * Hot update directory. After setting the hot update directory in the plugin,
     * the WebView will load updated resource files from the specified directory.
     */
    static std::string g_strHotCodeUpdateDirectory;
    /**true:debug,false:release*/
    static bool g_isDebugMode;
    /**Log flag on the capacitor JS side*/
    static bool g_isLoggingEnabled;
    /**
     * @brief Saves the schemeHandler and webTag to s_mapSchemeHandlerToWebTag.
     * @param pSchemeHandler Scheme handler pointer.
     * @param strWebTag Web tag std::string.
     */
    static void putSchemeHandlerWebTag(const void* pSchemeHandler, const std::string& strWebTag);
    /**
     * @brief Saves resources to be replaced during HTTP requests.
     * @param strWebTag Web tag std::string.
     * @param strSrc Source url
     * @param strObj Target url
     */
    static void pushResource(const std::string& strWebTag, const std::string& strSrc, const std::string& strObj);
    /**
     * @brief Retrieves the target resource to replace based on the source resource.
     * @param pSchemeHandler Scheme handler pointer.
     * @param strSrc Source url
     * @return Returns the target resource if a replacement exists; otherwise returns the source resource.
     */
    static std::string getResourceObj(const void* pSchemeHandler, const std::string& strSrc);
    /**
     * @brief Clears the resources marked for replacement (executed after the page finishes loading).
     * @param strWebTag Web tag std::string.
     */
    static void clearResource(const std::string& strWebTag);
    /**
     * @brief Adds custom HTTP headers (only keys, no values) to s_mapWebTagToCustomHttpHeaders.
     * @param strWebTag Web tag std::string.
     * @param strCustomerHttpHeaders
     */
    static void addCustomHttpHeaders(const std::string& strWebTag, const std::string& strCustomerHttpHeaders);
    /**
     * @brief Gets custom HTTP headers.
     * @param pSchemeHandler Scheme handler pointer.
     * @return Returns HTTP headers, with multiple headers separated by ",".
     */
    static std::string getCustomHttpHeaders(const void* pSchemeHandler);
    /**
     * @brief Sets withCredentials.
     * @param strWebTag  Web tag std::string.
     * @param isAllowCredentials true or false
     */
    static void addIsAllowCredentials(const std::string& strWebTag, const bool isAllowCredentials);
    /**
     * Gets withCredentials.
     * @param pSchemeHandler
     * @return true or false
     */
    static bool getIsAllowCredentials(const void* pSchemeHandler);
    /**
     * @brief remove scheme handler
     * @param strWebTag Web tag std::string.
     */
    static void removeSchemeHandlerWebTag(const std::string& strWebTag);
    /**
     * @brief Checks if the scheme handler is valid.
     * @param pSchemeHandler scheme handler
     * @return true if valid, false if invalid.
     */
    static bool isValidSchemeHandler(const void* pSchemeHandler);
    /**
     * @brief Checks if the web tag is valid.
     * @param strWebTag Web tag std::string
     * @return true if valid, false if invalid.
     */
    static bool isValidWebTag(const std::string& strWebTag);
    /**
     * @brief Adds the reference address of an ArkTS function.
     * @param strWebTag web tag std::string
     * @param pFunctionRefresh Function reference address.
     */
    static void addFunctionRefresh(const std::string& strWebTag, void* pFunctionRefresh);
    /**
     * @brief Retrieves the ArkTS function reference based on the web tag.
     * @param strWebTag web tag std::string
     * @return Function reference address.
     */
    static void* getFunctionRefresh(const std::string& strWebTag);
    /**
     * @brief Deletes the reference address of an ArkTS function.
     * @param strWebTag web tag std::string
     */
    static void delFunctionRefresh(const std::string& strWebTag);
    /**
     * @brief
     * @return strWebTag web tag std::string
     */
    static std::string getFirstWebTag();
    /**
     * @brief
     * @return std::vector for web tag std::string
     */
    static std::vector<std::string> getAllWebTag();
    /**
     * @brief Gets WebTag
     * @param pSchemeHandler
     * @return strWebTag web tag std::string
     */
    static std::string getWebTagBySchemeHandler(const void* pSchemeHandler);
    /**
     * @brief
     * @param strWebTag
     * @param strBasePath
     */
    static void setBathPath(const std::string& strWebTag, const std::string& strBasePath);
    /**
     * Gets BasePath
     * @param pSchemeHandler
     * @return strBasePath base path std::string
     */
    static std::string getBasePath(const void* pSchemeHandler);
};
#endif