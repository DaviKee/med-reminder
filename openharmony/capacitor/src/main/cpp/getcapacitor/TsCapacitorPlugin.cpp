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

#include "TsCapacitorPlugin.h"

void TsCapacitorPlugin::callMethod(PluginCall &call)
{
    if (call.getMethodName() == "onArKTsResult") {
        return onArKTsResult(call);
    }
    std::string strAction = "/capacitor/" + call.getWebTag() + "/" + call.getMethodName() + "/" + call.getCallbackId();
    char *pPageArgs = cJSON_Print(call.getData());
    executeArkTsAsync(strAction, 0, pPageArgs, call.getPluginId().c_str(), call);
    free(pPageArgs);
}

void TsCapacitorPlugin::onArKTsResult(PluginCall &call)
{
    cJSON *pCall = call.getData();
    if (pCall == nullptr) {
        return;
    }

    // 1. 解析基础 ID 字段
    cJSON *pCallBackId = cJSON_GetObjectItem(pCall, "callbackId");
    std::string strCallbackId = (pCallBackId && pCallBackId->type == cJSON_String) ? pCallBackId->valuestring : "";

    cJSON *pMethodName = cJSON_GetObjectItem(pCall, "methodName");
    std::string strMethodName = (pMethodName && pMethodName->type == cJSON_String) ? pMethodName->valuestring : "";

    cJSON *pPluginId = cJSON_GetObjectItem(pCall, "pluginId");
    std::string strPluginId = (pPluginId && pPluginId->type == cJSON_String) ? pPluginId->valuestring : "";

    // 2. 解析状态与结果字段
    cJSON *pResponse = cJSON_GetObjectItem(pCall, "response");
    cJSON *pSuccess = cJSON_GetObjectItem(pCall, "success");
    bool isSuccess = (pSuccess && pSuccess->type == cJSON_True);

    cJSON *pCode = cJSON_GetObjectItem(pCall, "code");
    std::string strCode = (pCode && pCode->type == cJSON_String) ? pCode->valuestring : "";

    cJSON *pMessage = cJSON_GetObjectItem(pCall, "message");
    std::string strMessage = (pMessage && pMessage->type == cJSON_String) ? pMessage->valuestring : "";

    cJSON *pKeepAlive = cJSON_GetObjectItem(pCall, "keepAlive");
    bool isKeepAlive = (pKeepAlive && pKeepAlive->type == cJSON_True);

    // 3. 设置 Call 属性
    call.setKeepAlive(isKeepAlive);
    call.setPluginId(strPluginId);
    call.setCallbackId(strCallbackId);

    // 4. 处理返回结果
    if (isSuccess) {
        pResponse == nullptr ? call.resolve() : call.resolve(pResponse);
    } else if (!strCode.empty() && !strMessage.empty()) {
        pResponse == nullptr ? call.reject(strMessage, strCode) : call.reject(strMessage, strCode, pResponse);
    } else if (!strMessage.empty()) {
        pResponse == nullptr ? call.reject(strMessage) : call.reject(strMessage, pResponse);
    } else {
        pResponse == nullptr ? call.reject("") : call.reject("", pResponse);
    }
}
void TsCapacitorPlugin::addListener(PluginCall &call) { callMethod(call); }

void TsCapacitorPlugin::removeListener(PluginCall &call) { callMethod(call); }

void TsCapacitorPlugin::removeAllListeners(PluginCall &call) { callMethod(call); }
void TsCapacitorPlugin::checkPermissions(PluginCall &call) { callMethod(call); }
void TsCapacitorPlugin::requestPermissions(PluginCall &call) { callMethod(call); }

void TsCapacitorPlugin::handleOnStart(const std::string &strWebTag)
{
    std::string strAction = "/capacitor/" + strWebTag + "/handleOnStart/*";
    PluginCall call(nullptr, "", "handleOnStart", "");
    executeArkTsAsync(strAction, 0, "[]", "", call);
}

void TsCapacitorPlugin::handleOnPageStart(const std::string &strWebTag)
{
    std::string strAction = "/capacitor/" + strWebTag + "/handleOnPageStart/*";
    PluginCall call(nullptr, "", "handleOnPageStart", "");
    executeArkTsAsync(strAction, 0, "[]", "", call);
}

void TsCapacitorPlugin::handleOnEnd(const std::string &strWebTag)
{
    std::string strAction = "/capacitor/" + strWebTag + "/handleOnEnd/*";
    PluginCall call(nullptr, "", "handleOnEnd", "");
    executeArkTsAsync(strAction, 0, "[]", "", call);
}

void TsCapacitorPlugin::handleOnResume(const std::string &strWebTag)
{
    std::string strAction = "/capacitor/*/handleOnResume/*";
    PluginCall call(nullptr, "", "handleOnResume", "");
    executeArkTsAsync(strAction, 0, "[]", "", call);
}

void TsCapacitorPlugin::handleOnPause(const std::string &strWebTag)
{
    std::string strAction = "/capacitor/*/handleOnPause/*";
    PluginCall call(nullptr, "", "handleOnPause", "");
    executeArkTsAsync(strAction, 0, "[]", "", call);
}