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

#include "Application.h"
#include "HttpUrl.h"

std::string Application::g_databaseDir = "";
std::string Application::g_strCachePath = "";
ArkWeb_ControllerAPI* Application::g_controller = nullptr;
ArkWeb_ComponentAPI* Application::g_component = nullptr;
NativeResourceManager* Application::g_resourceManager = nullptr;
ArkWeb_CookieManagerAPI* Application::g_cookieManage = nullptr;
std::string Application::g_strTmpUrl = "";
void* Application::g_connPoolManage = nullptr;
void* Application::g_httpGroupRunner = nullptr;
void* Application::g_memPool = nullptr;
void* Application::g_env = nullptr;
void* Application::g_cordovaViewController = nullptr;
std::string Application::g_strPushInfoBeforeInit = "";
std::string Application::g_strNotificationBeforeInit = "";
std::vector<std::string> Application::g_vecCustomSchemes;
std::vector<std::string> Application::g_vecWebTag;
std::vector<std::string> Application::g_vecProtocolUrl;
std::map<std::string, std::vector<std::string> > Application::s_mapWebTagToCustomHttpHeaders;
std::map<std::string, bool> Application::s_mapWebTagToIsAllowCredentials;
std::mutex Application::s_mutex;
std::map<std::string, std::map<std::string, std::string> > Application::s_mapWebTagToResource;
std::map<uintptr_t, std::string> Application::s_mapSchemeHandlerToWebTag;
std::map<std::string, void*> Application::s_mapFunctionRefresh;
napi_threadsafe_function Application::g_tsfn = nullptr;
std::string Application::g_strHotCodeUpdateDirectory = "";
bool Application::g_isDebugMode = false;
bool Application::g_isLoggingEnabled = false;
std::map<std::string, std::string> Application::s_mapWebTagToBasePath;

void Application::putSchemeHandlerWebTag(const void* pSchemeHandler, const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    uintptr_t upSchemeHandler = reinterpret_cast<uintptr_t>(pSchemeHandler);
    s_mapSchemeHandlerToWebTag[upSchemeHandler] = strWebTag;
}

void Application::pushResource(const std::string& strWebTag, const std::string& strSrc, const std::string& strObj)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    if (s_mapWebTagToResource.find(strWebTag) != s_mapWebTagToResource.end()) {
        std::map<std::string, std::string>& mapSrcObj = s_mapWebTagToResource[strWebTag];
        mapSrcObj[strSrc] = strObj;
    } else {
        std::map<std::string, std::string> mapSrcObj;
        mapSrcObj[strSrc] = strObj;
        s_mapWebTagToResource[strWebTag] = mapSrcObj;
    }
}

std::string Application::getResourceObj(const void* pSchemeHandler, const std::string& strSrc)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    uintptr_t upSchemeHandler = reinterpret_cast<uintptr_t>(pSchemeHandler);
    if (s_mapSchemeHandlerToWebTag.find(upSchemeHandler) == s_mapSchemeHandlerToWebTag.end()) {
        return strSrc;
    }
    std::string strWebTag = s_mapSchemeHandlerToWebTag[upSchemeHandler];
    if (s_mapWebTagToResource.find(strWebTag) == s_mapWebTagToResource.end()) {
        return strSrc;
    }
    std::map<std::string, std::string>& mapSrcObj = s_mapWebTagToResource[strWebTag];
    if (mapSrcObj.find(strSrc) == mapSrcObj.end()) {
        return strSrc;
    }
    return mapSrcObj[strSrc];
}

void Application::clearResource(const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    s_mapWebTagToResource.erase(strWebTag);
}

void Application::addCustomHttpHeaders(const std::string& strWebTag, const std::string& strCustomerHttpHeaders)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    std::vector<std::string> vecCustomerHttpHeaders;
    HttpUrl::splitString(strCustomerHttpHeaders, ",", vecCustomerHttpHeaders);
    vecCustomerHttpHeaders.push_back("Content-Type"); // 预添加
    vecCustomerHttpHeaders.push_back("Authorization");
    HttpUrl::diffContData<std::string>(vecCustomerHttpHeaders);
    s_mapWebTagToCustomHttpHeaders[strWebTag] = vecCustomerHttpHeaders;
}

std::string Application::getCustomHttpHeaders(const void* pSchemeHandler)
{
    std::string strRet = "Content-Type";
    std::lock_guard<std::mutex> guard(s_mutex);
    uintptr_t upSchemeHandler = reinterpret_cast<uintptr_t>(pSchemeHandler);
    if (s_mapSchemeHandlerToWebTag.find(upSchemeHandler) == s_mapSchemeHandlerToWebTag.end()) {
        return strRet;
    }
    std::string strWebTag = s_mapSchemeHandlerToWebTag[upSchemeHandler];
    if (s_mapWebTagToCustomHttpHeaders.find(strWebTag) == s_mapWebTagToCustomHttpHeaders.end()) {
        return strRet;
    }
    std::vector<std::string>& vecHeader = s_mapWebTagToCustomHttpHeaders[strWebTag];
    for (int i = 0; i < vecHeader.size(); i++) {
        strRet += ",";
        strRet += vecHeader[i];
    }

    return strRet;
}

void Application::addIsAllowCredentials(const std::string& strWebTag, const bool isAllowCredentials)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    s_mapWebTagToIsAllowCredentials[strWebTag] = isAllowCredentials;
}

bool Application::getIsAllowCredentials(const void* pSchemeHandler)
{
    bool isRet = false;
    std::lock_guard<std::mutex> guard(s_mutex);
    uintptr_t upSchemeHandler = reinterpret_cast<uintptr_t>(pSchemeHandler);
    if (s_mapSchemeHandlerToWebTag.find(upSchemeHandler) == s_mapSchemeHandlerToWebTag.end()) {
        return isRet;
    }
    std::string strWebTag = s_mapSchemeHandlerToWebTag[upSchemeHandler];
    if (s_mapWebTagToIsAllowCredentials.find(strWebTag) == s_mapWebTagToIsAllowCredentials.end()) {
        return isRet;
    }

    return s_mapWebTagToIsAllowCredentials[strWebTag];
}

void Application::removeSchemeHandlerWebTag(const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    for (auto it = s_mapSchemeHandlerToWebTag.begin(); it != s_mapSchemeHandlerToWebTag.end();) {
        if (it->second == strWebTag) {
            it = s_mapSchemeHandlerToWebTag.erase(it);
            break;
        } else {
            ++it;
        }
    }

    for (int i = 0; i < g_vecWebTag.size(); i++) {
        if (g_vecWebTag[i] == strWebTag) {
            g_vecWebTag.erase(g_vecWebTag.begin() + i);
            break;
        }
    }

    s_mapWebTagToCustomHttpHeaders.erase(strWebTag);
    s_mapWebTagToIsAllowCredentials.erase(strWebTag);
}

bool Application::isValidSchemeHandler(const void* pSchemeHandler)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    uintptr_t upSchemeHandler = reinterpret_cast<uintptr_t>(pSchemeHandler);
    if (s_mapSchemeHandlerToWebTag.find(upSchemeHandler) != s_mapSchemeHandlerToWebTag.end()) {
        return true;
    }
    return false;
}

bool Application::isValidWebTag(const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    if (s_mapFunctionRefresh.find(strWebTag) != s_mapFunctionRefresh.end()) {
        return true;
    }
    return false;
}

void Application::addFunctionRefresh(const std::string& strWebTag, void* pFunctionRefresh)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    s_mapFunctionRefresh[strWebTag] = pFunctionRefresh;
}

void* Application::getFunctionRefresh(const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    if (s_mapFunctionRefresh.find(strWebTag) != s_mapFunctionRefresh.end()) {
        return s_mapFunctionRefresh[strWebTag];
    }
    return nullptr;
}

void Application::delFunctionRefresh(const std::string& strWebTag)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    if (s_mapFunctionRefresh.find(strWebTag) != s_mapFunctionRefresh.end()) {
        s_mapFunctionRefresh.erase(strWebTag);
    }
}

std::string Application::getFirstWebTag()
{
    std::lock_guard<std::mutex> guard(s_mutex);
    if (!s_mapFunctionRefresh.empty()) {
        return s_mapFunctionRefresh.begin()->first;
    }
    return "";
}

std::vector<std::string> Application::getAllWebTag()
{
    std::lock_guard<std::mutex> guard(s_mutex);
    std::vector<std::string> vecWebTag;
    for (auto it = s_mapFunctionRefresh.begin(); it != s_mapFunctionRefresh.end(); it++) {
        vecWebTag.push_back(it->first);
    }
    return vecWebTag;
}

std::string Application::getWebTagBySchemeHandler(const void* pSchemeHandler)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    uintptr_t upSchemeHandler = reinterpret_cast<uintptr_t>(pSchemeHandler);
    if (s_mapSchemeHandlerToWebTag.find(upSchemeHandler) != s_mapSchemeHandlerToWebTag.end()) {
        return s_mapSchemeHandlerToWebTag[upSchemeHandler];
    }
    return "";
}

void Application::setBathPath(const std::string& strWebTag, const std::string& strBasePath)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    if (!strWebTag.empty() && !strBasePath.empty()) {
        s_mapWebTagToBasePath[strWebTag] = strBasePath;
    }
}

std::string Application::getBasePath(const void* pSchemeHandler)
{
    std::lock_guard<std::mutex> guard(s_mutex);
    uintptr_t upSchemeHandler = reinterpret_cast<uintptr_t>(pSchemeHandler);
    if (s_mapSchemeHandlerToWebTag.find(upSchemeHandler) != s_mapSchemeHandlerToWebTag.end()) {
        const std::string& strWebTag = s_mapSchemeHandlerToWebTag[upSchemeHandler];
        if (s_mapWebTagToBasePath.find(strWebTag) != s_mapWebTagToBasePath.end()) {
            return s_mapWebTagToBasePath[strWebTag];
        }
    }
    return "";
}