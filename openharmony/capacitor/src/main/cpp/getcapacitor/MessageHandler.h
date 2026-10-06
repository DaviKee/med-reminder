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
#ifndef MyApplication_MESSAGEHANDLER_H
#define MyApplication_MESSAGEHANDLER_H

#include <web/arkweb_type.h>
#include <string>

#include "getcapacitor/PluginResult.h"
#include "cJSON.h"
#include "PluginCall.h"

class MessageHandler {
    static const unsigned int LOG_PRINT_DOMAIN = 0xFF00;
    std::string m_strWebTag;
    void *m_bridge;

public:
    MessageHandler(void *pBridge, const std::string &strWebTag);
    ~MessageHandler();
    std::string getWebTag() const;
    void exposeJsInterface();
    static void postMessage(const char *webTag, const ArkWeb_JavaScriptBridgeData *dataArray, size_t arraySize,
                            void *userData);
    static void HandleCordovaMessage(MessageHandler *pMessageHandler, std::string strCallbackId, cJSON *pJson);
    static void HandleCapacitorMessage(MessageHandler *pMessageHandler, std::string strCallbackId, cJSON *pJson);
    void sendResponseMessage(const PluginCall *call, const Capacitor::PluginResult *pSuccessResult,
                             const Capacitor::PluginResult *pErrorResult);
    // 并非过时的接口，OpenHarmony使用的就是当前模式
    void legacySendResponseMessage(const Capacitor::PluginResult &data);
    void callPluginMethod(const std::string &callbackId, const std::string &pluginId, const std::string &methodName,
                          const std::string &strMethod);
    void callPluginMethod(const std::string &callbackId, const std::string &pluginId, const std::string &methodName,
                          cJSON *pMethodData);
    void callCordovaPluginMethod(const std::string &callbackId, const std::string &service, const std::string &action,
                                 const std::string &actionArgs) const;
};

#endif // MyApplication_MESSAGEHANDLER_H
