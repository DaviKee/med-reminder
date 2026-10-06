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

#include "Plugin.h"
const std::string PluginMethod::RETURN_PROMISE = "promise";
const std::string PluginMethod::RETURN_CALLBACK = "callback";
const std::string PluginMethod::RETURN_NONE = "none";

napi_threadsafe_function Plugin::g_tsfn = nullptr;

void Plugin::addEventListener(const std::string &strEventName, const PluginCall &call)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_mapEventListeners.find(strEventName) != m_mapEventListeners.end()) {
        std::vector<PluginCall> &vecPluginCall = m_mapEventListeners[strEventName];
        auto it = find(vecPluginCall.begin(), vecPluginCall.end(), call);
        if (it == vecPluginCall.end()) {
            vecPluginCall.push_back(call);
        }
    } else {
        std::vector<PluginCall> vecPluginCall;
        vecPluginCall.push_back(call);
        m_mapEventListeners[strEventName] = vecPluginCall;
    }
}

void Plugin::removeEventListener(const std::string &strEventName, const PluginCall &call)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_mapEventListeners.find(strEventName) != m_mapEventListeners.end()) {
        std::vector<PluginCall> &vecPluginCall = m_mapEventListeners[strEventName];
        auto it = find(vecPluginCall.begin(), vecPluginCall.end(), call);
        if (it != vecPluginCall.end()) {
            vecPluginCall.erase(it);
        }
    }
}

void Plugin::notifyListeners(const std::string &strEventName, cJSON *pData, const bool retainUntilConsumed)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_mapEventListeners.find(strEventName) == m_mapEventListeners.end()) {
        if (retainUntilConsumed && pData != nullptr) {
            cJSON *pJsonData = cJSON_Duplicate(pData, 1);
            if (m_mapRetainedEventArguments.find(strEventName) == m_mapRetainedEventArguments.end()) {
                std::vector<cJSON *> vecJsonData;
                vecJsonData.push_back(pJsonData);
                m_mapRetainedEventArguments[strEventName] = vecJsonData;
            } else {
                std::vector<cJSON *> &vecJsonData = m_mapRetainedEventArguments[strEventName];
                vecJsonData.push_back(pJsonData);
            }
        }
    }

    if (m_mapEventListeners.find(strEventName) != m_mapEventListeners.end()) {
        std::vector<PluginCall> &vecPluginCall = m_mapEventListeners[strEventName];
        for (int i = 0; i < vecPluginCall.size(); i++) {
            vecPluginCall[i].resolve(pData);
        }
    }
}

void Plugin::notifyListeners(const std::string &strEventName, cJSON *pData)
{
    notifyListeners(strEventName, pData, false);
}

bool Plugin::hasListeners(const std::string &strEventName)
{
    if (m_mapEventListeners.find(strEventName) != m_mapEventListeners.end()) {
        return m_mapEventListeners[strEventName].size() != 0;
    }
    return false;
}

void Plugin::sendRetainedArgumentsForEvent(const std::string &strEventName)
{
    if (m_mapRetainedEventArguments.find(strEventName) != m_mapRetainedEventArguments.end()) {
        std::vector<cJSON *> &vecJsonData = m_mapRetainedEventArguments[strEventName];
        for (int i = 0; i < vecJsonData.size(); i++) {
            notifyListeners(strEventName, vecJsonData[i]);
            cJSON_Delete(vecJsonData[i]);
        }
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_mapRetainedEventArguments.erase(strEventName);
        }
    }
}

void Plugin::addListener(PluginCall &call)
{
    std::string eventName = call.getString("eventName");
    if (!eventName.empty()) {
        call.setKeepAlive(true);
        addEventListener(eventName, call);
    }
}

void Plugin::removeListener(PluginCall &call)
{
    std::string eventName = call.getString("eventName");
    std::string callbackId = call.getString("callbackId");
    PluginCall saveCall(nullptr, call.getPluginId(), callbackId, "addListener", nullptr);
    if (!eventName.empty()) {
        removeEventListener(eventName, saveCall);
    }
}

void Plugin::removeAllListeners(PluginCall &call)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_mapEventListeners.clear();
    call.resolve();
}

void Plugin::removeAllListeners()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_mapEventListeners.clear();
}

void Plugin::checkPermissions(PluginCall &call) { call.resolve(); }

void Plugin::requestPermissions(PluginCall &call) { call.resolve(); }