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

#include "cJSON.h"
#include "napi/native_api.h"
#include "hilog/log.h"
#include "napi/native_api.h"
#include "rawfile_request.h"
#include "rawfile/raw_file_manager.h"
#include "web/arkweb_scheme_handler.h"
#include "web/arkweb_interface.h"
#include "CordovaViewController.h"
#include "Application.h"
#include "ConnPool.h"
#include "FileCache.h"
#include "HttpUrl.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <multimedia/image_framework/image_mdk.h>
#include <multimedia/image_framework/image_receiver_mdk.h>

#undef LOG_TAG
#define LOG_TAG "ss-handler"

CordovaViewController* g_cordovaViewController = nullptr;

#ifndef NDEBUG

#endif

/**
 * @brief Custom scheme registration
 *        Must be called before initializing the web kernel
 * @param env info A std::string of multiple custom schemes, separated by ','
 * @return null
 */
static napi_value RegisterCustomSchemes(napi_env env, napi_callback_info info)
{
    OH_LOG_INFO(LOG_APP, "register cdvfile schemes");
    std::vector<std::string>::iterator it =
        find(Application::g_vecCustomSchemes.begin(), Application::g_vecCustomSchemes.end(), "cdvfile");
    if (it == Application::g_vecCustomSchemes.end()) {
        // 注册“custom“ scheme到Web组件，并指定该scheme需要遵循标准的scheme规则，允许该scheme发出跨域请求。
        OH_ArkWeb_RegisterCustomSchemes("cdvfile", ARKWEB_SCHEME_OPTION_STANDARD | ARKWEB_SCHEME_OPTION_CORS_ENABLED);
        // 注册“custom-local” scheme到Web组件，并指定该scheme需要遵循与“file” scheme一样的规则。
        OH_ArkWeb_RegisterCustomSchemes("cdvfile-local", ARKWEB_SCHEME_OPTION_LOCAL);
        // 注册“custom-csp-bypassing”到Web组件，并指定该scheme需要遵循标准的scheme规则，允许忽略CSP检查。
        OH_ArkWeb_RegisterCustomSchemes("cdvfile-csp-bypassing",
                                        ARKWEB_SCHEME_OPTION_CSP_BYPASSING | ARKWEB_SCHEME_OPTION_STANDARD);
        // 注册“custom-isolated”到Web组件，并指定该scheme的请求必须从相同scheme加载的网页中发起。
        OH_ArkWeb_RegisterCustomSchemes("cdvfile-isolated", ARKWEB_SCHEME_OPTION_DISPLAY_ISOLATED);
        Application::g_vecCustomSchemes.push_back("cdvfile");
    }

    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc == 1) {
        // 获取第一个参数 resultValue
        size_t resultSize = 0;
        napi_get_value_string_utf8(env, args[0], nullptr, 0, &resultSize);
        char resultValue[resultSize + 1];
        std::fill(resultValue, resultValue + sizeof(resultValue), 0);
        size_t resultLength = 0;
        napi_get_value_string_utf8(env, args[0], resultValue, resultSize + 1, &resultLength);

        std::vector<std::string> vecCustomSchemes;
        HttpUrl::splitString(resultValue, ",", vecCustomSchemes);
        HttpUrl::diffContData<std::string>(vecCustomSchemes);
        for (int i = 0; i < vecCustomSchemes.size(); i++) {
            std::string strCustomScheme = HttpUrl::trim(vecCustomSchemes[i]);
            if (vecCustomSchemes.empty() || vecCustomSchemes[i] == "https" || vecCustomSchemes[i] == "http" ||
                vecCustomSchemes[i] == "ftp" || vecCustomSchemes[i] == "file") {
                continue;
            }
            std::vector<std::string>::iterator it = find(
                Application::g_vecCustomSchemes.begin(), Application::g_vecCustomSchemes.end(), vecCustomSchemes[i]);
            if (it != Application::g_vecCustomSchemes.end()) {
                continue;
            }
            Application::g_vecCustomSchemes.push_back(vecCustomSchemes[i]);
            char szCustomSchemes[128];
            OH_ArkWeb_RegisterCustomSchemes(vecCustomSchemes[i].c_str(),
                                            ARKWEB_SCHEME_OPTION_STANDARD | ARKWEB_SCHEME_OPTION_CORS_ENABLED);
            // 注册“custom-local” scheme到Web组件，并指定该scheme需要遵循与“file” scheme一样的规则。
            sprintf(szCustomSchemes, "%s-local", vecCustomSchemes[i].c_str());
            OH_ArkWeb_RegisterCustomSchemes(szCustomSchemes, ARKWEB_SCHEME_OPTION_LOCAL);
            // 注册“custom-csp-bypassing”到Web组件，并指定该scheme需要遵循标准的scheme规则，允许忽略CSP检查。
            sprintf(szCustomSchemes, "%s-csp-bypassing", vecCustomSchemes[i].c_str());
            OH_ArkWeb_RegisterCustomSchemes(szCustomSchemes,
                                            ARKWEB_SCHEME_OPTION_CSP_BYPASSING | ARKWEB_SCHEME_OPTION_STANDARD);
            // 注册“custom-isolated”到Web组件，并指定该scheme的请求必须从相同scheme加载的网页中发起。
            sprintf(szCustomSchemes, "%s-isolated", vecCustomSchemes[i].c_str());
            OH_ArkWeb_RegisterCustomSchemes(szCustomSchemes, ARKWEB_SCHEME_OPTION_DISPLAY_ISOLATED);
        }
    }
    return nullptr;
}

/**
 *  @brief Callback for request initiation. Determines whether to intercept the request.
 * If intercepted, the request is handled by Cordova; if allowed, it is processed by the WebView.
 * Interception occurs under the following conditions：
 * 1，Resource replacement is set on the ArkTS side
 * 2，The domain name to intercept is configured in config.xml
 * 3，The domain name to intercept is dynamically set on the ArkTS side
 * 4，Interception of localhost domains.
 * 5，Interception of cdvfile schemes
 * 6，Interception of dynamic cross-origin resource requests
 * 7，Interception of iframe cross-origin requests
 * 8，Interception of OPTIONS requests.
 * @param schemeHandler Pointer to the scheme object
 * @param resourceRequest Pointer to the request object
 * @param resourceHandler Pointer to the response handler object
 * @param intercept true: intercept; false: allow
 */
void OnURLRequestStart(const ArkWeb_SchemeHandler* schemeHandler,
                       ArkWeb_ResourceRequest* resourceRequest,
                       const ArkWeb_ResourceHandler* resourceHandler,
                       bool* intercept)
{
    char* url;
    OH_ArkWebResourceRequest_GetUrl(resourceRequest, &url);
    std::string urlPath = url;
    OH_ArkWeb_ReleaseString(url);

    std::string strSchemeDomainPort;
    std::string strProtocol;
    std::string strDomain;
    if (urlPath.find("//") != std::string::npos) {
        strSchemeDomainPort = urlPath;
        strProtocol = urlPath.substr(0, urlPath.find("//") - 1);
        strDomain = urlPath.substr(urlPath.find("//") + 2);
        if (strDomain.find("/") != std::string::npos) {
            strDomain = strDomain.substr(0, strDomain.find("/"));
        }
        strSchemeDomainPort = strSchemeDomainPort.substr(0, strSchemeDomainPort.find(strDomain) + strDomain.length());
    } else {
        return;
    }

    std::string strUrl = Application::getResourceObj(schemeHandler, urlPath);
    if (strUrl != urlPath) {
        *intercept = true;
        RawfileRequest* request = new RawfileRequest(
            resourceRequest, resourceHandler, Application::g_resourceManager, schemeHandler, strSchemeDomainPort);
        OH_ArkWebResourceRequest_SetUserData(resourceRequest, request);
        request->start();
        return;
    }

    std::string strIonicScheme = g_cordovaViewController->getIonicScheme();
    if (!strIonicScheme.empty() && strProtocol == strIonicScheme && strIonicScheme != "https" &&
        strIonicScheme != "http" && strIonicScheme != "cdvfile") {
        *intercept = false;
        return;
    }

    char* method;
    OH_ArkWebResourceRequest_GetMethod(resourceRequest, &method);
    std::string strMethod = method;
    OH_ArkWeb_ReleaseString(method);

    int32_t resourceType = OH_ArkWebResourceRequest_GetResourceType(resourceRequest);

    if (urlPath.find("https://") == 0 && strDomain != Application::g_strTmpUrl && strDomain != "localhost") {
        if (resourceType == STYLE_SHEET || resourceType == SCRIPT || resourceType == IMAGE || resourceType == FAVICON ||
            resourceType == PING) {
            *intercept = false;
            return;
        }
    }

    const std::vector<std::string>& vecProtocolUrl =
        *((CordovaViewController*)Application::g_cordovaViewController)->getProtocolUrl();
    auto it = find(vecProtocolUrl.begin(), vecProtocolUrl.end(), strDomain);
    if (it != vecProtocolUrl.end()) {
        *intercept = true;
        RawfileRequest* request = new RawfileRequest(
            resourceRequest, resourceHandler, Application::g_resourceManager, schemeHandler, strSchemeDomainPort);
        OH_ArkWebResourceRequest_SetUserData(resourceRequest, request);
        request->start();
        return;
    }

    const std::vector<std::string>& vecProtocolUrl2 = Application::g_vecProtocolUrl;
    auto it2 = find(vecProtocolUrl2.begin(), vecProtocolUrl2.end(), strDomain);
    if (it2 != vecProtocolUrl2.end()) {
        *intercept = true;
        RawfileRequest* request = new RawfileRequest(
            resourceRequest, resourceHandler, Application::g_resourceManager, schemeHandler, strSchemeDomainPort);
        OH_ArkWebResourceRequest_SetUserData(resourceRequest, request);
        request->start();
        return;
    }

    std::map<std::string, std::string> mapHeader;
    ArkWeb_RequestHeaderList* headerList = nullptr;
    OH_ArkWebResourceRequest_GetRequestHeaders(resourceRequest, &headerList);
    if (headerList != nullptr) {
        int headerListSize = OH_ArkWebRequestHeaderList_GetSize(headerList);
        for (int i = 0; i < headerListSize; i++) {
            char* key;
            char* value;
            OH_ArkWebRequestHeaderList_GetHeader(headerList, i, &key, &value);
            mapHeader[key] = value;
            OH_ArkWeb_ReleaseString(key);
            OH_ArkWeb_ReleaseString(value);
        }
        OH_ArkWebRequestHeaderList_Destroy(headerList);
    }

    urlPath = urlPath.erase(0, urlPath.find_first_not_of(" "));
    urlPath = urlPath.erase(urlPath.find_last_not_of(" ") + 1);
    if ((strDomain == Application::g_strTmpUrl || strDomain == "localhost") ||
        (strProtocol != "http" && strProtocol != "https")) {
        *intercept = true;
        RawfileRequest* request = new RawfileRequest(
            resourceRequest, resourceHandler, Application::g_resourceManager, schemeHandler, strSchemeDomainPort);
        OH_ArkWebResourceRequest_SetUserData(resourceRequest, request);
        request->start();
    } else if (mapHeader.find("Origin") != mapHeader.end()) {
        std::string strOrigin = mapHeader["Origin"];
        if (resourceType == STYLE_SHEET || resourceType == SCRIPT || resourceType == IMAGE || resourceType == FAVICON ||
            resourceType == PING) {
            *intercept = false;
            return;
        }

        // 跨域请求的来源是本地虚拟域名，其他来源都有webview处理跨域
        if (strOrigin.find(Application::g_strTmpUrl) != std::string::npos) {
            *intercept = true;
            RawfileRequest* request = new RawfileRequest(
                resourceRequest, resourceHandler, Application::g_resourceManager, schemeHandler, strSchemeDomainPort);
            OH_ArkWebResourceRequest_SetUserData(resourceRequest, request);
            request->start();
            return;
        }
        *intercept = false;
        return;
    } else if (mapHeader.find("Sec-Fetch-Dest") != mapHeader.end() &&
               mapHeader.find("Sec-Fetch-Site") != mapHeader.end()) {
        std::string strFetchDest = mapHeader["Sec-Fetch-Dest"];
        std::string strFetchSite = mapHeader["Sec-Fetch-Site"];
        if (strFetchDest == "iframe" && strFetchSite == "cross-site") {
            *intercept = true;
            RawfileRequest* request = new RawfileRequest(
                resourceRequest, resourceHandler, Application::g_resourceManager, schemeHandler, strSchemeDomainPort);
            OH_ArkWebResourceRequest_SetUserData(resourceRequest, request);
            request->start();
            return;
        }
        *intercept = false;
        return;
    } else {
        *intercept = false;
        return;
    }
}

/**
 * @brief 完成请求后回调
 * @param schemeHandler Pointer to the scheme object
 * @param resourceRequest Pointer to the request object
 */
void OnURLRequestStop(const ArkWeb_SchemeHandler* schemeHandler, const ArkWeb_ResourceRequest* request)
{
    if (!request) {
        OH_LOG_ERROR(LOG_APP, "on request stop request is nullptr.");
        return;
    }

    RawfileRequest* rawfileRequest = static_cast<RawfileRequest*>(OH_ArkWebResourceRequest_GetUserData(request));
    if (rawfileRequest) {
        rawfileRequest->stop();
        if (rawfileRequest->isTaskFinished()) {
            delete rawfileRequest;
        }
    }
}

/**
 * @brief Sets the SchemeHandler
 * @param env info containing two parameters: the first is the custom scheme, the second is the webTag.
 * @return null
 */
static napi_value SetSchemeHandler(napi_env env, napi_callback_info info)
{
    OH_LOG_INFO(LOG_APP, "set scheme handler");
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 customScheme
    size_t customSchemeSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &customSchemeSize);
    char customScheme[customSchemeSize + 1];
    std::fill(customScheme, customScheme + sizeof(customScheme), 0);
    size_t customSchemeLength = 0;
    napi_get_value_string_utf8(env, args[0], customScheme, customSchemeSize + 1, &customSchemeLength);

    // 获取第二个参数 webTag
    size_t webTagSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &webTagSize);
    char webTag[webTagSize + 1];
    std::fill(webTag, webTag + sizeof(webTag), 0);
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[1], webTag, webTagSize + 1, &webTagLength);

    // 创建一个ArkWeb_SchemeHandler对象。
    ArkWeb_SchemeHandler* pSchemeHandler = nullptr;
    OH_ArkWeb_CreateSchemeHandler(&pSchemeHandler);
    Application::putSchemeHandlerWebTag(pSchemeHandler, webTag);

    // 为ArkWeb_SchemeHandler设置ArkWeb_OnRequestStart与ArkWeb_OnRequestStop回调。
    OH_ArkWebSchemeHandler_SetOnRequestStart(pSchemeHandler, OnURLRequestStart);
    OH_ArkWebSchemeHandler_SetOnRequestStop(pSchemeHandler, OnURLRequestStop);

    OH_ArkWeb_SetSchemeHandler("https", webTag, pSchemeHandler);
    OH_ArkWeb_SetSchemeHandler("http", webTag, pSchemeHandler);
    OH_ArkWeb_SetSchemeHandler("cdvfile", webTag, pSchemeHandler);

    OH_ArkWebServiceWorker_SetSchemeHandler("https", pSchemeHandler);
    OH_ArkWebServiceWorker_SetSchemeHandler("http", pSchemeHandler);
    OH_ArkWebServiceWorker_SetSchemeHandler("cdvfile", pSchemeHandler);

    std::vector<std::string> vecCustomSchemes;
    HttpUrl::splitString(customScheme, ",", vecCustomSchemes);
    HttpUrl::diffContData<std::string>(vecCustomSchemes);
    for (int i = 0; i < vecCustomSchemes.size(); i++) {
        std::string strCustomScheme = HttpUrl::trim(vecCustomSchemes[i]);
        if (vecCustomSchemes.empty() || vecCustomSchemes[i] == "https" || vecCustomSchemes[i] == "http" ||
            vecCustomSchemes[i] == "ftp" || vecCustomSchemes[i] == "file") {
            continue;
        }
        OH_ArkWeb_SetSchemeHandler(strCustomScheme.c_str(), webTag, pSchemeHandler);
        OH_ArkWebServiceWorker_SetSchemeHandler(strCustomScheme.c_str(), pSchemeHandler);
    }
    return nullptr;
}

/**
 * @brief Cordova initialization function
 * @param env info containing seven parameters:
 *      1，webTag：WebTag of the WebView
 *      2，tempUrl：Virtual domain
 *      3，route: Route for executing ArkTS functions from the C++ side
 *      4，databaseDir：Database sandbox directory
 *      5，resourceManager:Resource manager object for accessing rawfile directory files of the app
 *      6，customHttpHeader：Custom HTTP headers; passed when withCredentials is set to true
 *      7，AllowCredentials：Sets the `withCredentials` property of the WebView
 * @return jsonArray:Returns the preferences node configuration from config.xml for use by ArkTS custom plugins
 */
static napi_value InitCordova(napi_env env, napi_callback_info info)
{
    size_t argc = 8;
    napi_value args[8] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 webTag
    size_t webTagSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &webTagSize);
    char webTagValue[webTagSize + 1];
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[0], webTagValue, webTagSize + 1, &webTagLength);
    const std::string strWebTag = webTagValue;

    std::vector<std::string>::iterator it =
        find(Application::g_vecWebTag.begin(), Application::g_vecWebTag.end(), strWebTag);
    if (it == Application::g_vecWebTag.end()) {
        Application::g_vecWebTag.push_back(webTagValue); // 缓存所有的webTag
    }

    // 获取第二个参数 tempUrl
    size_t temUrlSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &temUrlSize);
    char tmpUrlValue[temUrlSize + 1];
    size_t tmpUrlLength = 0;
    napi_get_value_string_utf8(env, args[1], tmpUrlValue, temUrlSize + 1, &tmpUrlLength);
    Application::g_strTmpUrl = tmpUrlValue;

    Application::g_env = env; // 无论有多少个webview，此环境变量永远不变

    // 获取第三个参数 route
    napi_ref refer;
    // 创建引用之后保存，防止释放。
    napi_create_reference(env, args[2], 1, &refer);
    Application::addFunctionRefresh(webTagValue, refer);

    // 获取第四个参数
    size_t databaseDirSize = 0;
    napi_get_value_string_utf8(env, args[3], nullptr, 0, &databaseDirSize);
    char databaseDir[databaseDirSize + 1];
    size_t databaseDirLength = 0;
    napi_get_value_string_utf8(env, args[3], databaseDir, databaseDirSize + 1, &databaseDirLength);
    Application::g_databaseDir = databaseDir;
    Application::g_strCachePath =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/cache";
    // 设置一个默认热更新目录，如果使用capacitor的自动注入js，系统会自动修改本地rawfile/index.html文件注入js后存放在该目录下
    Application::g_strHotCodeUpdateDirectory =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files/inject";

    // 获取第五个参数
    Application::g_resourceManager = OH_ResourceManager_InitNativeResourceManager(env, args[4]);

    // 获取第六个参数
    size_t customHttpHeaderSize = 0;
    napi_get_value_string_utf8(env, args[5], nullptr, 0, &customHttpHeaderSize);
    char customHttpHeader[customHttpHeaderSize + 1];
    size_t customHttpHeaderLength = 0;
    napi_get_value_string_utf8(env, args[5], customHttpHeader, customHttpHeaderSize + 1, &customHttpHeaderLength);
    Application::addCustomHttpHeaders(strWebTag, customHttpHeader);

    // 获取第七个参数
    bool isAllowCredentials;
    napi_get_value_bool(env, args[6], &isAllowCredentials);
    Application::addIsAllowCredentials(strWebTag, isAllowCredentials);

    // 获取第八个参数
    size_t webBasePathSize = 0;
    napi_get_value_string_utf8(env, args[7], nullptr, 0, &webBasePathSize);
    char webBasePath[webBasePathSize + 1];
    size_t webBasePathLength = 0;
    napi_get_value_string_utf8(env, args[7], webBasePath, webBasePathSize + 1, &webBasePathLength);
    Application::setBathPath(strWebTag, webBasePath);

    if (Application::g_controller == nullptr) {
        Application::g_controller =
            reinterpret_cast<ArkWeb_ControllerAPI*>(OH_ArkWeb_GetNativeAPI(ARKWEB_NATIVE_CONTROLLER));
        Application::g_component =
            reinterpret_cast<ArkWeb_ComponentAPI*>(OH_ArkWeb_GetNativeAPI(ARKWEB_NATIVE_COMPONENT));
        Application::g_cookieManage =
            reinterpret_cast<ArkWeb_CookieManagerAPI*>(OH_ArkWeb_GetNativeAPI(ARKWEB_NATIVE_COOKIE_MANAGER));
        Application::g_connPoolManage = new ConnPoolManage();
        g_cordovaViewController = new CordovaViewController();
        Application::g_cordovaViewController = g_cordovaViewController;
    }

    g_cordovaViewController->loadUrl(webTagValue);

    std::string strIonicHostName = g_cordovaViewController->getIonicHostName();
    if (!strIonicHostName.empty()) {
        Application::g_strTmpUrl = strIonicHostName;
    }

    strIonicHostName = g_cordovaViewController->getCapacitorPluginManager()->getCapConfig()->getHostName();
    if (!strIonicHostName.empty()) {
        Application::g_strTmpUrl = strIonicHostName;
    }

    std::map<std::string, std::string>& mapPreferences = g_cordovaViewController->getCordovaPreferences()->getAll();
    cJSON* pArrayJson = cJSON_CreateArray();
    for (auto it = mapPreferences.begin(); it != mapPreferences.end(); it++) {
        cJSON* pJsonHostName = cJSON_CreateObject();
        cJSON_AddStringToObject(pJsonHostName, "name", it->first.c_str());
        cJSON_AddStringToObject(pJsonHostName, "value", it->second.c_str());
        cJSON_AddItemToArray(pArrayJson, pJsonHostName);
    }

    std::map<std::string, std::string>& mapCapacitorPreferences =
        g_cordovaViewController->getCapacitorPluginManager()->getCapConfig()->getCapacitorPreferences();
    for (auto it = mapCapacitorPreferences.begin(); it != mapCapacitorPreferences.end(); it++) {
        cJSON* pJsonHostName = cJSON_CreateObject();
        cJSON_AddStringToObject(pJsonHostName, "name", it->first.c_str());
        cJSON_AddStringToObject(pJsonHostName, "value", it->second.c_str());
        cJSON_AddItemToArray(pArrayJson, pJsonHostName);
    }

    cJSON* pRetJson = cJSON_CreateObject();
    cJSON_AddItemToObject(pRetJson, "preferences", pArrayJson);
    char* pRet = cJSON_Print(pRetJson);
    std::string strRet = pRet;
    cJSON_Delete(pRetJson);
    free(pRet);

    napi_value ret = nullptr;
    const char* pArgs = strRet.c_str();
    napi_create_string_utf8(env, pArgs, strlen(pArgs), &ret);
    return ret;
}

/**
 * @brief Notifies the C++ side of the execution result from the ArkTS side
 * @param env info：containing 2 parameters：
 *      1，resultValue：Execution result from ArkTS (JSON std::string)
 *      2，serviceValue：Name of the plugin (C++)
 * @return null
 */
static napi_value onArkTsResult(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 resultValue
    size_t resultSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &resultSize);
    char* resultValue = new char[resultSize + 1]; // 回调的数据参数有可能很大，栈上的内存不能满足要求，需堆上申请
    std::fill(resultValue, resultValue + resultSize + 1, 0);
    size_t resultLength = 0;
    napi_get_value_string_utf8(env, args[0], resultValue, resultSize + 1, &resultLength);
    std::string strResultValue = resultValue;
    delete[] resultValue;

    // 获取第二个参数 serviceValue
    size_t serviceSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &serviceSize);
    char serviceValue[serviceSize + 1];
    std::fill(serviceValue, serviceValue + sizeof(serviceValue), 0);
    size_t serviceLength = 0;
    napi_get_value_string_utf8(env, args[1], serviceValue, serviceSize + 1, &serviceLength);

    // 获取第三个参数 webTagValue
    size_t webTagSize = 0;
    napi_get_value_string_utf8(env, args[2], nullptr, 0, &webTagSize);
    char webTagValue[webTagSize + 1];
    std::fill(webTagValue, webTagValue + sizeof(webTagValue), 0);
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[2], webTagValue, webTagSize + 1, &webTagLength);

    if (g_cordovaViewController == nullptr) {
        std::string strServiceValue = serviceValue;
        if (strServiceValue == "HuaweiPushPlugin") {
            Application::g_strPushInfoBeforeInit = strResultValue;
        }
        return nullptr;
    }
    g_cordovaViewController->execPluginResult(webTagValue, serviceValue, "onArKTsResult", strResultValue);
    return nullptr;
}

/**
 * @brief Retrieves the list of C++ side plugins
 * @return jsonArray Returns the list of C++ side plugins
 */
static napi_value GetPluginEntry(napi_env env, napi_callback_info info)
{
    if (Application::g_cordovaViewController == nullptr) {
        return nullptr;
    }

    std::vector<PluginEntry>& vecPluginEntry =
        *((CordovaViewController*)Application::g_cordovaViewController)->getPluginEntry();
    cJSON* pRetJson = cJSON_CreateArray();
    for (int i = 0; i < vecPluginEntry.size(); i++) {
        cJSON* pJson = cJSON_CreateObject();
        cJSON_AddStringToObject(pJson, "serviceName", vecPluginEntry[i].m_service.c_str());
        cJSON_AddStringToObject(pJson, "className", vecPluginEntry[i].m_pluginClass.c_str());
        if (vecPluginEntry[i].m_onload) {
            cJSON_AddTrueToObject(pJson, "onload");
        } else {
            cJSON_AddFalseToObject(pJson, "onload");
        }
        cJSON_AddItemToArray(pRetJson, pJson);
    }

    char* pRet = cJSON_Print(pRetJson);
    std::string strRet = pRet;
    cJSON_Delete(pRetJson);
    free(pRet);

    napi_value ret = nullptr;
    const char* pArgs = strRet.c_str();
    napi_create_string_utf8(env, pArgs, strlen(pArgs), &ret);
    return ret;
}

/**
 * @brief Notifies the C++ side of the plugin list from the ArkTS side
 * @param env info jsonArray of ArkTS side plugin list
 * @return null
 */
static napi_value SetPluginEntry(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 webTag
    size_t webTagSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &webTagSize);
    char webTagValue[webTagSize + 1];
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[0], webTagValue, webTagSize + 1, &webTagLength);

    // 获取第二个参数 cordovaPlugins
    size_t resultSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &resultSize);
    char resultValue[resultSize + 1];
    std::fill(resultValue, resultValue + sizeof(resultValue), 0);
    size_t resultLength = 0;
    napi_get_value_string_utf8(env, args[1], resultValue, resultSize + 1, &resultLength);

    // 获取第三个参数 capacitorPlugins
    size_t pluginBufSize = 0;
    napi_get_value_string_utf8(env, args[2], nullptr, 0, &pluginBufSize);
    char pluginBufValue[pluginBufSize + 1];
    std::fill(pluginBufValue, pluginBufValue + sizeof(pluginBufValue), 0);
    size_t pluginBufLength = 0;
    napi_get_value_string_utf8(env, args[2], pluginBufValue, pluginBufSize + 1, &pluginBufLength);

    cJSON* pJson = cJSON_Parse(resultValue);
    g_cordovaViewController->instantiateTsPlugin(webTagValue, pJson);
    cJSON_Delete(pJson);

    g_cordovaViewController->InitCapacitorTsPlugin(webTagValue, pluginBufValue);
    return nullptr;
}

/**
 * @brief Sets the resource replacement function on the ArkTS side
 * @param env info containing three parameters:
 *      1，webTag:WebTag of the WebView
 *      2，resourceValue:Original resource URL
 *      3，objSrcValue：Target resource URL
 * @return null
 */
static napi_value SetResourceReplace(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 webTag
    size_t webTagSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &webTagSize);
    char webTagValue[webTagSize + 1];
    std::fill(webTagValue, webTagValue + sizeof(webTagValue), 0);
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[0], webTagValue, webTagSize + 1, &webTagLength);


    // 获取第二个参数 resource
    size_t resourceSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &resourceSize);
    char resourceValue[resourceSize + 1];
    std::fill(resourceValue, resourceValue + sizeof(resourceValue), 0);
    size_t resourceLength = 0;
    napi_get_value_string_utf8(env, args[1], resourceValue, resourceSize + 1, &resourceLength);

    // 获取第三个参数 objSrc
    size_t objSrcSize = 0;
    napi_get_value_string_utf8(env, args[2], nullptr, 0, &objSrcSize);
    char objSrcValue[objSrcSize + 1];
    std::fill(objSrcValue, objSrcValue + sizeof(objSrcValue), 0);
    size_t objSrcLength = 0;
    napi_get_value_string_utf8(env, args[2], objSrcValue, objSrcSize + 1, &objSrcLength);

    Application::pushResource(webTagValue, resourceValue, objSrcValue);
    return nullptr;
}

/**
 * @brief Sets the URLs for Cordova to handle requests on the ArkTS side. All requests originating from these URLs will
 * be processed by Cordova
 * @param env info: URLs to be handled by Cordova (multiple URLs should be comma-separated).
 * @return null
 */
static napi_value SetCordovaProtocolUrl(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    size_t urlSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &urlSize);
    char urlValue[urlSize + 1];
    std::fill(urlValue, urlValue + sizeof(urlValue), 0);
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[0], urlValue, urlSize + 1, &webTagLength);
    if (strlen(urlValue) > 0) {
        HttpUrl::splitString(urlValue, ",", Application::g_vecProtocolUrl);
        HttpUrl::diffContData<std::string>(Application::g_vecProtocolUrl);
    }
    return nullptr;
}

/**
 * @brief Cordova bridge initialization function. Sets the bridge mode and bridge secret.
 *      HarmonyOS enforces the use of EVAL_BRIDGE mode.
 * @return Returns the bridge secret.
 */
static napi_value onJsPrompt(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value args[4] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 webTag
    size_t webTagSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &webTagSize);
    char webTagValue[webTagSize + 1];
    std::fill(webTagValue, webTagValue + sizeof(webTagValue), 0);
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[0], webTagValue, webTagSize + 1, &webTagLength);

    // 获取第二个参数 origin
    size_t originSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &originSize);
    char originSValue[originSize + 1];
    std::fill(originSValue, originSValue + sizeof(originSValue), 0);
    size_t originLength = 0;
    napi_get_value_string_utf8(env, args[1], originSValue, originSize + 1, &originLength);

    // 获取第三个参数 message
    size_t messageSize = 0;
    napi_get_value_string_utf8(env, args[2], nullptr, 0, &messageSize);
    char messageValue[messageSize + 1];
    std::fill(messageValue, messageValue + sizeof(messageValue), 0);
    size_t messageLength = 0;
    napi_get_value_string_utf8(env, args[2], messageValue, messageSize + 1, &messageLength);

    // 获取第四个参数 defaultValue
    size_t defaultValueSize = 0;
    napi_get_value_string_utf8(env, args[3], nullptr, 0, &defaultValueSize);
    char defaultValueValue[defaultValueSize + 1];
    std::fill(defaultValueValue, defaultValueValue + sizeof(defaultValueValue), 0);
    size_t defaultValueLength = 0;
    napi_get_value_string_utf8(env, args[3], defaultValueValue, defaultValueSize + 1, &defaultValueLength);

    CordovaWebViewEngine* pEngine = g_cordovaViewController->getCordovaWebViewEngine(webTagValue);
    std::string strRet = "";
    if (pEngine != nullptr) {
        strRet = pEngine->getJsAPI()->getCordovaBridge()->promptOnJsPrompt(
            webTagValue, originSValue, messageValue, defaultValueValue);
    }
    napi_value ret = nullptr;
    if (!strRet.empty()) {
        napi_create_string_utf8(env, strRet.c_str(), strRet.length(), &ret);
    }
    return ret;
}

/**
 *
 */
static napi_value SetClientAuthP12Cert(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 url
    size_t urlSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &urlSize);
    char urlValue[urlSize + 1];
    std::fill(urlValue, urlValue + sizeof(urlValue), 0);
    size_t urlLength = 0;
    napi_get_value_string_utf8(env, args[0], urlValue, urlSize + 1, &urlLength);

    // 获取第二个参数passwd
    size_t passwdSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &passwdSize);
    char passwdValue[passwdSize + 1];
    std::fill(passwdValue, passwdValue + sizeof(passwdValue), 0);
    size_t passwdLength = 0;
    napi_get_value_string_utf8(env, args[1], passwdValue, passwdSize + 1, &passwdLength);

    // 获取第三个参数 cert base64
    size_t certSize = 0;
    napi_get_value_string_utf8(env, args[2], nullptr, 0, &certSize);
    char* certValue = new char[certSize + 1];
    std::fill(certValue, certValue + certSize + 1, 0);
    size_t certLength = 0;
    napi_get_value_string_utf8(env, args[2], certValue, certSize + 1, &certLength);
    std::string strCert = certValue;
    free(certValue);

    std::string strTempUrl = urlValue;
    if (strTempUrl.find(":") != std::string::npos) {
        strTempUrl = strTempUrl.replace(strTempUrl.find(":"), 1, "_");
    }

    std::string strTempCertPath = Application::g_strCachePath + "/client_cert/" + strTempUrl + "/client_cert.cer";
    std::string strTempKeyPath = Application::g_strCachePath + "/client_cert/" + strTempUrl + "/client_key.key";
    if (!FileCache::IsFile(strTempCertPath, "") && !FileCache::IsFile(strTempKeyPath, "")) {
        int nSize = 0;
        char* p = CBase64::decode(strCert, nSize);
        if (!FileCache::createDirectory(Application::g_strCachePath + "/client_cert/" + strTempUrl)) {
            OH_LOG_ERROR(LOG_APP,
                         "FileCache::createDirectory(Application::g_strCachePath{%{public}s}/strTempUrl{%{public}s})",
                         Application::g_strCachePath.c_str(),
                         strTempUrl.c_str());
        } else if (CSSLSocket::parsePKCS12(p, nSize, passwdValue, strTempKeyPath.c_str(), strTempCertPath.c_str()) &&
                   FileCache::IsFile(strTempKeyPath, "") && FileCache::IsFile(strTempCertPath, "")) {
            OH_LOG_INFO(LOG_APP,
                        "P12 Parse success:cert:%{public}s,key:%{public}s",
                        strTempCertPath.c_str(),
                        strTempKeyPath.c_str());
            if (g_cordovaViewController) {
                g_cordovaViewController->setClientCert(urlValue);
            }
        } else {
            OH_LOG_ERROR(LOG_APP,
                         "P12 Parse failed:cert:%{public}s,key:%{public}s",
                         strTempCertPath.c_str(),
                         strTempKeyPath.c_str());
        }
    } else {
        OH_LOG_INFO(
            LOG_APP, "P12 Cache:cert:%{public}s,key:%{public}s", strTempCertPath.c_str(), strTempKeyPath.c_str());
        if (g_cordovaViewController) {
            g_cordovaViewController->setClientCert(urlValue);
        }
    }
    return nullptr;
}


static napi_value getCapacitorInjectJs(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    // 获取第一个参数 webTag
    size_t webTagSize = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &webTagSize);
    char webTagValue[webTagSize + 1];
    std::fill(webTagValue, webTagValue + sizeof(webTagValue), 0);
    size_t webTagLength = 0;
    napi_get_value_string_utf8(env, args[0], webTagValue, webTagSize + 1, &webTagLength);

    // 获取第二个参数basePath
    size_t basePathSize = 0;
    napi_get_value_string_utf8(env, args[1], nullptr, 0, &basePathSize);
    char basePathValue[basePathSize + 1];
    std::fill(basePathValue, basePathValue + sizeof(basePathValue), 0);
    size_t basePathLength = 0;
    napi_get_value_string_utf8(env, args[1], basePathValue, basePathSize + 1, &basePathLength);

    const std::string strCapacitorJs = Bridge::getInjectCapacitorJs(basePathValue, webTagValue);

    napi_value ret = nullptr;
    const char* pArgs = strCapacitorJs.c_str();
    napi_create_string_utf8(env, pArgs, strlen(pArgs), &ret);
    return ret;
}


EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"RegisterCustomSchemes", nullptr, RegisterCustomSchemes, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"SetSchemeHandler", nullptr, SetSchemeHandler, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"InitCordova", nullptr, InitCordova, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"GetPluginEntry", nullptr, GetPluginEntry, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"SetPluginEntry", nullptr, SetPluginEntry, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"SetResourceReplace", nullptr, SetResourceReplace, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"SetCordovaProtocolUrl", nullptr, SetCordovaProtocolUrl, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onJsPrompt", nullptr, onJsPrompt, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"SetClientAuthP12Cert", nullptr, SetClientAuthP12Cert, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onArkTsResult", nullptr, onArkTsResult, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getCapacitorInjectJs", nullptr, getCapacitorInjectJs, nullptr, nullptr, nullptr, napi_default, nullptr}};
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "capacitor",
    .nm_priv = ((void*)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
