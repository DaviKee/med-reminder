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

#include "PluginCall.h"
#include "MessageHandler.h"
#include "Application.h"

const std::string PluginCall::CALLBACK_ID_DANGLING = "-1";

PluginCall::~PluginCall()
{
    if (m_pJsonData != nullptr) {
        cJSON_Delete(m_pJsonData);
    }
}

PluginCall::PluginCall(void *pMessageHandler, const std::string &strPluginId, const std::string &strMethodName,
                       const std::string &strMethodData)
{
    m_pMessageHandler = pMessageHandler;
    m_strPluginId = strPluginId;
    m_strCallbackId = "";
    m_strMethodName = strMethodName;
    m_pJsonData = cJSON_Parse(strMethodData.c_str());
}

PluginCall::PluginCall(void *pMessageHandler, const std::string &strPluginId, const std::string &strCallbackId,
                       const std::string &strMethodName, cJSON *pJsonData)
{
    m_pMessageHandler = pMessageHandler;
    m_strPluginId = strPluginId;
    m_strCallbackId = strCallbackId;
    m_strMethodName = strMethodName;
    if (pJsonData != nullptr) {
        m_pJsonData = cJSON_Duplicate(pJsonData, 1);
    }
}

std::string PluginCall::getWebTag()
{
    if (m_pMessageHandler != nullptr) {
        MessageHandler *pMessageHandler = (MessageHandler *)m_pMessageHandler;
        return pMessageHandler->getWebTag();
    }
    // 没有初始化的call,选择第一个webTag，主要用于系统初始化C++到ArkTS侧，不局限于某一个webview
    if (!Application::g_vecWebTag.empty()) {
        return Application::g_vecWebTag[0];
    }
    return "";
}
void PluginCall::successCallback(const Capacitor::PluginResult &pPluginResult)
{
    MessageHandler *pMessageHandler = (MessageHandler *)m_pMessageHandler;
    pMessageHandler->sendResponseMessage(this, &pPluginResult, nullptr);
}

void PluginCall::resolve(cJSON *pData)
{
    Capacitor::PluginResult result(pData);
    successCallback(result);
}

void PluginCall::resolve()
{
    MessageHandler *pMessageHandler = (MessageHandler *)m_pMessageHandler;
    pMessageHandler->sendResponseMessage(this, nullptr, nullptr);
}

void PluginCall::errorCallback(const std::string &strMsg) const
{
    Capacitor::PluginResult errorResult;
    errorResult.put("message", strMsg);
    MessageHandler *pMessageHandler = (MessageHandler *)m_pMessageHandler;
    pMessageHandler->sendResponseMessage(this, nullptr, &errorResult);
}

void PluginCall::reject(const std::string &strMsg, const std::string &strCode, const std::exception *ex,
                        cJSON *pJsonData)
{
    Capacitor::PluginResult errorResult;
    errorResult.put("message", strMsg);
    errorResult.put("code", strCode);
    if (pJsonData != nullptr) {
        errorResult.put("data", pJsonData);
    }

    if (ex != nullptr) {
        throw *ex;
    }

    MessageHandler *pMessageHandler = (MessageHandler *)m_pMessageHandler;
    pMessageHandler->sendResponseMessage(this, nullptr, &errorResult);
}

void PluginCall::reject(const std::string &strMsg, const std::exception *ex, cJSON *pJsonData)
{
    reject(strMsg, "", ex, pJsonData);
}

void PluginCall::reject(const std::string &strMsg, const std::string &strCode, cJSON *pJsonData)
{
    reject(strMsg, strCode, nullptr, pJsonData);
}

void PluginCall::reject(const std::string &strMsg, const std::string &strCode, const std::exception *ex)
{
    reject(strMsg, strCode, ex, nullptr);
}

void PluginCall::reject(const std::string &strMsg, cJSON *pJsonData) { reject(strMsg, "", nullptr, pJsonData); }

void PluginCall::reject(const std::string &strMsg, const std::exception *ex) { reject(strMsg, "", ex, nullptr); }

void PluginCall::reject(const std::string &strMsg, const std::string &strCode)
{
    reject(strMsg, strCode, nullptr, nullptr);
}

void PluginCall::reject(const std::string &strMsg) { reject(strMsg, "", nullptr, nullptr); }

void PluginCall::unimplemented() { unimplemented("not implemented"); }

void PluginCall::unimplemented(const std::string &strMsg) { reject(strMsg, "UNIMPLEMENTED", nullptr, nullptr); }

void PluginCall::unavailable() { unavailable("not available"); }

void PluginCall::unavailable(const std::string &strMsg) { reject(strMsg, "UNAVAILABLE", nullptr, nullptr); }

std::string PluginCall::getString(const std::string &strName) { return getString(strName, ""); }

std::string PluginCall::getString(const std::string &strName, const std::string &strDefaultValue)
{
    cJSON *pStr = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pStr != nullptr && pStr->type == cJSON_String) {
        return pStr->valuestring;
    }
    return strDefaultValue;
}

int PluginCall::getInt(const std::string &strName) { return getInt(strName, 0); }

int PluginCall::getInt(const std::string &strName, const int nDefaultValue)
{
    cJSON *pStr = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pStr != nullptr && pStr->type == cJSON_Number) {
        return pStr->valueint;
    }
    return nDefaultValue;
}

long PluginCall::getLong(const std::string &strName) { return getLong(strName, 0); }

long PluginCall::getLong(const std::string &strName, const long lngDefaultValue)
{
    cJSON *pStr = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pStr != nullptr && pStr->type == cJSON_Number) {
        return static_cast<long>(pStr->valuedouble);
    }
    return lngDefaultValue;
}

float PluginCall::getFloat(const std::string &strName) { return getFloat(strName, 0); }

float PluginCall::getFloat(const std::string &strName, const float fDefaultValue)
{
    cJSON *pStr = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pStr != nullptr && pStr->type == cJSON_Number) {
        return static_cast<float>(pStr->valuedouble);
    }
    return fDefaultValue;
}

double PluginCall::getDouble(const std::string &strName) { return getDouble(strName, 0); }

double PluginCall::getDouble(const std::string &strName, const double dbDefaultValue)
{
    cJSON *pStr = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pStr != nullptr && pStr->type == cJSON_Number) {
        return pStr->valuedouble;
    }
    return dbDefaultValue;
}

bool PluginCall::getBoolean(const std::string &strName) { return getBoolean(strName, false); }

bool PluginCall::getBoolean(const std::string &strName, const bool bDefaultValue)
{
    cJSON *pStr = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pStr != nullptr && pStr->type == cJSON_True) {
        return true;
    }

    if (pStr != nullptr && pStr->type == cJSON_False) {
        return false;
    }
    return bDefaultValue;
}

cJSON *PluginCall::getObject(const std::string &strName) { return getObject(strName, nullptr); }

cJSON *PluginCall::getObject(const std::string &strName, cJSON *pJsonDefaultValue)
{
    cJSON *pJson = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pJson != nullptr && pJson->type == cJSON_Object) {
        return pJson;
    }
    return pJsonDefaultValue;
}

cJSON *PluginCall::getArray(const std::string &strName) { return getArray(strName, nullptr); }

cJSON *PluginCall::getArray(const std::string &strName, cJSON *pJsonDefaultValue)
{
    cJSON *pJson = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pJson != nullptr && pJson->type == cJSON_Array) {
        return pJson;
    }
    return pJsonDefaultValue;
}

bool PluginCall::hasOption(const std::string &strName)
{
    cJSON *pJson = cJSON_GetObjectItem(m_pJsonData, strName.c_str());
    if (pJson != nullptr) {
        return true;
    }
    return false;
}

void PluginCall::save() { setKeepAlive(true); }

void PluginCall::setKeepAlive(const bool isKeepAlive) { m_isKeepAlive = isKeepAlive; }

void PluginCall::release() { m_isKeepAlive = false; }
