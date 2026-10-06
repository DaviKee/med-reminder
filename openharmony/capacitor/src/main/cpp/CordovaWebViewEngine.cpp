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

#include "CordovaWebViewEngine.h"
#include "CordovaExposedJsApi.h"
#include "web/arkweb_type.h"
#include "hilog/log.h"
#include "Application.h"

const unsigned int CordovaWebViewEngine::LOG_PRINT_DOMAIN = 0xFF00;

CordovaWebViewEngine::CordovaWebViewEngine(CordovaExposedJsApi* jsAPI, const std::string& strWebTag)
{
    m_jsAPI = jsAPI;
    m_strWebTag = strWebTag;
}

CordovaWebViewEngine::~CordovaWebViewEngine() {}

CordovaExposedJsApi* CordovaWebViewEngine::getJsAPI()
{
    return m_jsAPI;
}

void CordovaWebViewEngine::exec(const char* webTag,
                                const ArkWeb_JavaScriptBridgeData* dataArray,
                                size_t arraySize,
                                void* userData)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk exec webTag:%{public}s", webTag);
    if (!userData) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk exec userData is nullptr");
        return;
    }
    CordovaExposedJsApi* jsApi = static_cast<CordovaExposedJsApi*>(userData);
    jsApi->exec(dataArray, arraySize);
}

void CordovaWebViewEngine::setNativeToJsBridgeMode(const char* webTag,
                                                   const ArkWeb_JavaScriptBridgeData* dataArray,
                                                   size_t arraySize,
                                                   void* userData)
{
    OH_LOG_Print(
        LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk setNativeToJsBridgeMode webTag:%{public}s", webTag);
    if (!userData) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk setNativeToJsBridgeMode userData is nullptr");
        return;
    }
    CordovaExposedJsApi* jsApi = static_cast<CordovaExposedJsApi*>(userData);
    jsApi->setNativeToJsBridgeMode(dataArray, arraySize);
}

void CordovaWebViewEngine::retrieveJsMessages(const char* webTag,
                                              const ArkWeb_JavaScriptBridgeData* dataArray,
                                              size_t arraySize,
                                              void* userData)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk retrieveJsMessages webTag:%{public}s", webTag);
    if (!userData) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk retrieveJsMessages userData is nullptr");
        return;
    }
    CordovaExposedJsApi* jsApi = static_cast<CordovaExposedJsApi*>(userData);
    jsApi->retrieveJsMessages(dataArray, arraySize);
}

/*
 *当前注册对象和函数使用的API12版本，注册函数无返回值，在cordova.js文件的androidExec函数中，返回null，没有错误信息或者js轮询消息处理
 *Api18和API20版本，提供了OH_NativeArkWeb_RegisterAsyncThreadJavaScriptProxy方法和ArkWeb_ProxyObjectWithResult对象，可以注册带有返回值的对象和函数，更好的兼容androidExec函数
 */
void CordovaWebViewEngine::exposeJsInterface()
{
    ArkWeb_ProxyMethod function1 = {"exec", CordovaWebViewEngine::exec, static_cast<void*>(m_jsAPI)};
    ArkWeb_ProxyMethod function2 = {
        "setNativeToJsBridgeMode", CordovaWebViewEngine::setNativeToJsBridgeMode, static_cast<void*>(m_jsAPI)};
    ArkWeb_ProxyMethod function3 = {
        "retrieveJsMessages", CordovaWebViewEngine::retrieveJsMessages, static_cast<void*>(m_jsAPI)};
    ArkWeb_ProxyMethod methodList[3] = {function1, function2, function3};
    ArkWeb_ProxyObject proxyObject = {"_cordovaNative", methodList, 3};
    Application::g_controller->registerJavaScriptProxy(m_strWebTag.c_str(), &proxyObject);
}
