/*
 * The MIT License (MIT)
 * Copyright (C) 2026 Huawei Device Co., Ltd and iSoftStone Information Technology(Group)Co.,Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#include "MessageHandler.h"
#include "Application.h"
#include "Bridge.h"
#include "PluginManager.h"
#include "CordovaViewController.h"
#include "hilog/log.h"

MessageHandler::MessageHandler(void *pBridge, const std::string &strWebTag) : m_bridge(pBridge)
{
    m_strWebTag = strWebTag;
}

MessageHandler::~MessageHandler() {}

std::string MessageHandler::getWebTag() const { return m_strWebTag; }

void MessageHandler::exposeJsInterface()
{
    ArkWeb_ProxyMethod function1 = {"postMessage", MessageHandler::postMessage, static_cast<void *>(this)};
    ArkWeb_ProxyMethod methodList[1] = {function1};
    ArkWeb_ProxyObject proxyObject = {"androidBridge", methodList, 1};
    // ArkWeb不存在原框架的addWebMessageListener的方法，仍使用注册对象的方法,如后续ArkWeb升级后，此处可升级优化
    Application::g_controller->registerJavaScriptProxy(m_strWebTag.c_str(), &proxyObject);
}

void MessageHandler::HandleCordovaMessage(MessageHandler *pMessageHandler, std::string strCallbackId, cJSON *pJson)
{
    std::string strService;
    cJSON *pService = cJSON_GetObjectItem(pJson, "service");
    if (pService != nullptr && pService->type == cJSON_String) {
        strService = pService->valuestring;
    }

    std::string strAction;
    cJSON *pAction = cJSON_GetObjectItem(pJson, "action");
    if (pAction != nullptr && pAction->type == cJSON_String) {
        strAction = pAction->valuestring;
    }

    std::string strActionArgs = "[]";
    cJSON *pActionArgs = cJSON_GetObjectItem(pJson, "actionArgs");
    if (pActionArgs != nullptr && pActionArgs->type == cJSON_Array) {
        char *pStr = cJSON_Print(pActionArgs);
        if (pStr != nullptr) {
            strActionArgs = pStr;
            free(pStr);
        }
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "MessageHandler",
                 "To native (Cordova plugin): callbackId:%{public}s, "
                 "service:%{public}s,action:%{public}s,actionArgs:%{public}s",
                 strCallbackId.c_str(), strService.c_str(), strAction.c_str(), strActionArgs.c_str());
    pMessageHandler->callCordovaPluginMethod(strCallbackId, strService, strAction, strActionArgs);
}

void MessageHandler::HandleCapacitorMessage(MessageHandler *pMessageHandler, std::string strCallbackId, cJSON *pJson)
{
    std::string strPluginId;
    cJSON *pPluginId = cJSON_GetObjectItem(pJson, "pluginId");
    if (pPluginId != nullptr && pPluginId->type == cJSON_String) {
        strPluginId = pPluginId->valuestring;
    }

    std::string strMethodName;
    cJSON *pMethodName = cJSON_GetObjectItem(pJson, "methodName");
    if (pMethodName != nullptr && pMethodName->type == cJSON_String) {
        strMethodName = pMethodName->valuestring;
    }

    cJSON *pMethodData = cJSON_GetObjectItem(pJson, "options");
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "MessageHandler",
                 "To native (Capacitor plugin): callbackId::%{public}s, pluginId:%{public}s,methodName:%{public}s",
                 strCallbackId.c_str(), strPluginId.c_str(), strMethodName.c_str());
    pMessageHandler->callPluginMethod(strCallbackId, strPluginId, strMethodName, pMethodData);
}
void MessageHandler::postMessage(const char *webTag, const ArkWeb_JavaScriptBridgeData *dataArray, size_t arraySize,
                                 void *userData)
{
    MessageHandler *pMessageHandler = static_cast<MessageHandler *>(userData);
    if (arraySize != 1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "MessageHandler",
                     "MessageHandler::postMessage(const ArkWeb_JavaScriptBridgeData *dataArray, int32_t "
                     "arraySize(%{public}d)) error",
                     arraySize);
        return;
    }
    std::string jsonStr((char *)dataArray[0].buffer, dataArray[0].size);
    cJSON *pJson = cJSON_Parse(jsonStr.c_str());
    if (pJson == nullptr) {
        return;
    }

    cJSON *pType = cJSON_GetObjectItem(pJson, "type");
    bool typeIsNotNull = pType != nullptr;
    bool isCordovaPlugin = typeIsNotNull && pType->type == cJSON_String && strcmp(pType->valuestring, "cordova") == 0;
    bool isJavaScriptError =
        typeIsNotNull && pType->type == cJSON_String && strcmp(pType->valuestring, "js.error") == 0;

    std::string strCallbackId;
    cJSON *pCallbackId = cJSON_GetObjectItem(pJson, "callbackId");
    if (pCallbackId != nullptr && pCallbackId->type == cJSON_String) {
        strCallbackId = pCallbackId->valuestring;
    }
    if (isCordovaPlugin) {
        HandleCordovaMessage(pMessageHandler, strCallbackId, pJson);
    } else if (isJavaScriptError) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "MessageHandler", "JavaScript Error: %{public}s",
                     pJson->valuestring);
    } else {
        HandleCapacitorMessage(pMessageHandler, strCallbackId, pJson);
    }
    cJSON_Delete(pJson);
}

void MessageHandler::sendResponseMessage(const PluginCall *pCall, const Capacitor::PluginResult *pSuccessResult,
                                         const Capacitor::PluginResult *pErrorResult)
{
    Capacitor::PluginResult resultData;
    resultData.put("save", pCall->isKeepAlive());
    resultData.put("callbackId", pCall->getCallbackId());
    resultData.put("pluginId", pCall->getPluginId());
    resultData.put("methodName", pCall->getMethodName());
    if (pErrorResult != nullptr) {
        resultData.put("success", false);
        resultData.put("error", pErrorResult);
    } else {
        resultData.put("success", true);
        if (pSuccessResult != nullptr) {
            resultData.put("data", pSuccessResult);
        }
    }

    if (pCall->getCallbackId() != PluginCall::CALLBACK_ID_DANGLING) {
        legacySendResponseMessage(resultData);
    }
}

void MessageHandler::legacySendResponseMessage(const Capacitor::PluginResult &data)
{
    std::string runScript = "window.Capacitor.fromNative(" + data.toString() + ")";
    Bridge *pBridge = (Bridge *)m_bridge;
    pBridge->SendResponseMessage(m_strWebTag, runScript);
}

void MessageHandler::callPluginMethod(const std::string &callbackId, const std::string &pluginId,
                                      const std::string &methodName, const std::string &strMethod)
{
    cJSON *pJsonData = cJSON_Parse(strMethod.c_str());
    callPluginMethod(callbackId, pluginId, methodName, pJsonData);
    cJSON_Delete(pJsonData);
}
void MessageHandler::callPluginMethod(const std::string &callbackId, const std::string &pluginId,
                                      const std::string &methodName, cJSON *pMethodData)
{
    PluginCall pluginCall(this, pluginId, callbackId, methodName, pMethodData);
    Bridge *pBridge = (Bridge *)m_bridge;
    pBridge->callPluginMethod(pluginId, methodName, pluginCall);
}

void MessageHandler::callCordovaPluginMethod(const std::string &callbackId, const std::string &service,
                                             const std::string &action, const std::string &actionArgs) const
{
    PluginManager *pPluginManager = ((CordovaViewController *)Application::g_cordovaViewController)->getPluginManager();
    pPluginManager->exec(m_strWebTag, service, action, callbackId, actionArgs);
}
