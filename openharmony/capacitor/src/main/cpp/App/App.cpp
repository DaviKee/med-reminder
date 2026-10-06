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

#include "App.h"
#include "PluginManager.h"
#include <bundle/native_interface_bundle.h>

REGISTER_CAP_PLUGIN(App, App)

void App::addListener(PluginCall &call) {
    Plugin::addListener(call);
    if (call.getString("eventName") == "appStateChange" && !appStateChanged) {
        OH_NativeBundle_ApplicationInfo application = OH_NativeBundle_GetCurrentApplicationInfo();
        executeArkTs("./App/App/appStateChange", 0, application.bundleName, "App", call);
        appStateChanged = true;
    } else if (call.getString("eventName") == "appUrlOpen" && !appUrlOpen) {
        m_call = call;
        executeArkTs("./App/App/setLaunchUrl", 0, "", "App", call);
        appUrlOpen = true;
    } else if (call.getString("eventName") == "backButton" && !backButton) {
        executeArkTs("./App/App/RegisterBackButton", 0, "", "App", call);
        backButton = true;
    }
}

void App::removeListener(PluginCall &call)
{
    Plugin::removeListener(call);
    std::string eventName = call.getString("eventName");
    if (eventName == "appStateChange" && appStateChanged) {
        executeArkTs("./App/App/removeAllListeners", 0, "", "App", call);
        appStateChanged = false;
    } else if (eventName == "appUrlOpen" && appUrlOpen) {
        appUrlOpen = false;
    } else if (eventName == "backButton" && backButton) {
        backButton = false;
    }
}


void App::removeAllListeners(PluginCall &call)
{
    Plugin::removeAllListeners(call);
    executeArkTs("./App/App/removeAllListeners", 0, "", "App", call);
    appStateChanged = false;
    appUrlOpen = false;
    backButton = false;
}

void App::handleOnStart(const std::string &strWebTag)
{
    Plugin::handleOnStart(strWebTag);
    PluginCall call(nullptr, "App", "load", "");
    executeArkTs("./App/App/load", 0, "", "App", call);
}

void App::handleOnEnd(const std::string &strWebTag) { Plugin::handleOnEnd(strWebTag); }

void App::handleOnResume(const std::string &strWebTag)
{
    Plugin::handleOnResume(strWebTag);
    cJSON *pJson = cJSON_CreateObject();
    notifyListeners("resume", pJson);
}

void App::handleOnPause(const std::string &strWebTag)
{
    Plugin::handleOnPause(strWebTag);
    cJSON *pJson = cJSON_CreateObject();
    notifyListeners("pause", pJson);
}

void App::handleOnDestroy(const std::string &strWebTag) { Plugin::handleOnDestroy(strWebTag); }


REGISTER_PLUGIN_METHOD(App, exitApp, PluginMethod::RETURN_PROMISE)
void App::exitApp(PluginCall &call) {
    m_call = call;
    executeArkTs("./App/App/exitApp", 0, "", "App", call);
}

REGISTER_PLUGIN_METHOD(App, getInfo, PluginMethod::RETURN_PROMISE)
void App::getInfo(PluginCall &call) {
    m_call = call;
    executeArkTs("./App/App/getInfo", 0, "", "App", call);
}

REGISTER_PLUGIN_METHOD(App, getState, PluginMethod::RETURN_PROMISE)
void App::getState(PluginCall &call) {
    m_call = call;
    OH_NativeBundle_ApplicationInfo application = OH_NativeBundle_GetCurrentApplicationInfo();
    executeArkTs("./App/App/getState", 0, application.bundleName, "App", call);
}

REGISTER_PLUGIN_METHOD(App, getLaunchUrl, PluginMethod::RETURN_PROMISE)
void App::getLaunchUrl(PluginCall &call) {
    cJSON *pJson = cJSON_CreateObject();
    cJSON_AddStringToObject(pJson, "url", urlStr.c_str());
    call.resolve(pJson);
    cJSON_Delete(pJson);
}

REGISTER_PLUGIN_METHOD(App, minimizeApp, PluginMethod::RETURN_PROMISE)
void App::minimizeApp(PluginCall &call) {
    m_call = call;
    executeArkTs("./App/App/AppMinimize", 0, "", "App", call);
}

REGISTER_PLUGIN_METHOD(App, toggleBackButtonHandler, PluginMethod::RETURN_PROMISE)
void App::toggleBackButtonHandler(PluginCall &call) {
    m_call = call;
    bool result = call.getBoolean("enabled");
    executeArkTs("./App/App/toggleBackButtonHandler", result, "", "App", call);
}

REGISTER_PLUGIN_METHOD(App, onArKTsResult, PluginMethod::RETURN_PROMISE)
void App::onArKTsResult(PluginCall &call) {
    if (call.hasOption("notify")) {
        std::string eventName = call.getString("notify");
        notifyListeners(eventName, call.getObject("content"));
    } else if (call.hasOption("result")) {
        std::string result = call.getString("result");
        if (result == "") {
            m_call.resolve();
        } else if (result == "failed") {
            std::string msg = call.getString("errorMsg");
            m_call.reject(msg);
        } else {
            m_call.resolve(call.getObject("content"));
            if (cJSON_GetObjectItem(call.getObject("content"), "url")) {
                std::string url = cJSON_GetObjectItem(call.getObject("content"), "url")->valuestring;
                urlStr = url;
            }
        }
    }
}
