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

#include "LocalNotifications.h"

REGISTER_CAP_PLUGIN(LocalNotifications, LocalNotifications)

void LocalNotifications::addListener(PluginCall &call)
{
    Plugin::addListener(call);
    call.unimplemented();
}

void LocalNotifications::removeAllListeners(PluginCall &call)
{
    Plugin::removeAllListeners(call);
    call.unimplemented();
}

void LocalNotifications::handleOnStart(const std::string& strWebTag)
{
    Plugin::handleOnStart(strWebTag);
    PluginCall call(nullptr, "LocalNotifications", "load", "");
    executeArkTs("./LocalNotifications/LocalNotifications/load", 0, "", "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, schedule, PluginMethod::RETURN_PROMISE)
void LocalNotifications::schedule(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/schedule", 1, cJSON_Print(call.getData()),
                 "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, getPending, PluginMethod::RETURN_PROMISE)
void LocalNotifications::getPending(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/getPending", 1, cJSON_Print(call.getData()),
                 "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, cancel, PluginMethod::RETURN_PROMISE)
void LocalNotifications::cancel(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/cancelReminders", 1, cJSON_Print(call.getData()),
                 "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, areEnabled, PluginMethod::RETURN_PROMISE)
void LocalNotifications::areEnabled(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/areEnabled", 1, cJSON_Print(call.getData()),
                 "LocalNotifications", call);
}
REGISTER_PLUGIN_METHOD(LocalNotifications, getDeliveredNotifications, PluginMethod::RETURN_PROMISE)
void LocalNotifications::getDeliveredNotifications(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/getDeliveredNotifications", 1, cJSON_Print(call.getData()),
                 "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, removeDeliveredNotifications, PluginMethod::RETURN_PROMISE)
void LocalNotifications::removeDeliveredNotifications(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/removeDeliveredNotifications", 1, cJSON_Print(call.getData()),
                 "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, removeAllDeliveredNotifications, PluginMethod::RETURN_PROMISE)
void LocalNotifications::removeAllDeliveredNotifications(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/removeAllDeliveredNotifications", 1,
                 cJSON_Print(call.getData()), "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, registerActionTypes, PluginMethod::RETURN_PROMISE)
void LocalNotifications::registerActionTypes(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/registerActionTypes", 0,
     cJSON_Print(call.getData()), "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, createChannel, PluginMethod::RETURN_PROMISE)
void LocalNotifications::createChannel(PluginCall &call)
{
    call.unimplemented();
}

REGISTER_PLUGIN_METHOD(LocalNotifications, deleteChannel, PluginMethod::RETURN_PROMISE)
void LocalNotifications::deleteChannel(PluginCall &call)
{
    call.unimplemented();
}

REGISTER_PLUGIN_METHOD(LocalNotifications, listChannels, PluginMethod::RETURN_PROMISE)
void LocalNotifications::listChannels(PluginCall &call)
{
    call.unimplemented();
}

REGISTER_PLUGIN_METHOD(LocalNotifications, changeExactNotificationSetting, PluginMethod::RETURN_PROMISE)
void LocalNotifications::changeExactNotificationSetting(PluginCall &call)
{
    call.unimplemented();
}

REGISTER_PLUGIN_METHOD(LocalNotifications, checkExactNotificationSetting, PluginMethod::RETURN_PROMISE)
void LocalNotifications::checkExactNotificationSetting(PluginCall &call)
{
    call.unimplemented();
}

REGISTER_PLUGIN_METHOD(LocalNotifications, checkPermissions, PluginMethod::RETURN_PROMISE)
void LocalNotifications::checkPermissions(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/checkPermissions", 1, "", "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, requestPermissions, PluginMethod::RETURN_PROMISE)
void LocalNotifications::requestPermissions(PluginCall &call)
{
    m_call = call;
    executeArkTs("./LocalNotifications/LocalNotifications/requestPermissions", 1, "", "LocalNotifications", call);
}

REGISTER_PLUGIN_METHOD(LocalNotifications, onArKTsResult, PluginMethod::RETURN_PROMISE)
void LocalNotifications::onArKTsResult(PluginCall &call)
{
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
        }
    }
}
