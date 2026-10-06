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

#ifndef MYAPPLICATION_BRIDGE_H
#define MYAPPLICATION_BRIDGE_H

#include "CapConfig.h"
#include "ServerPath.h"
#include "MessageHandler.h"
#include "PluginManager.h"
#include "MessageQueue.h"

class Bridge {
    static const unsigned int LOG_PRINT_DOMAIN = 0xFF00;
    std::string m_strWebTag;
    MessageHandler *m_pMessageHandler{nullptr};
    Capacitor::PluginManager *m_pPluginManager;
    MessageQueue *m_pMessageQueue{nullptr};
    ServerPath m_serverPath;
    CapConfig *m_pCapConfig;
    Bridge() = default;

public:
    static const std::string DEFAULT_WEB_ASSET_DIR;
    Bridge(const std::string &strWebTag, Capacitor::PluginManager *pPluginManager, MessageQueue *pMessageQueue,
           ServerPath serverPath, CapConfig *pConfig);
    ~Bridge() = default;
    class Builder {
        ServerPath m_serverPath;
        CapConfig *m_pCapConfig;

    public:
        Builder() = default;
        ~Builder() = default;
        Bridge *create(const std::string &strWebTag, Capacitor::PluginManager *pPluginManager,
                       MessageQueue *pMessageQueue);
        Bridge::Builder *setConfig(CapConfig *pConfig);
    };

    PluginHandle *getPlugin(const std::string &strPluginId);
    void callPluginMethod(const std::string &pluginId, const std::string &methodName, const std::string &strMethodData);
    void callPluginMethod(const std::string &strPluginId, const std::string &strMethodName, PluginCall &call);
    void SendResponseMessage(const std::string &strWebTag, const std::string &strJsStatement) const;
    void eval(const std::string &strWebTag, const std::string &js) const;
    void logToJs(const std::string &strWebTag, const std::string &message, const std::string &level);
    void logToJs(const std::string &strWebTag, const std::string &message);
    void triggerJSEvent(const std::string &strWebTag, const std::string &eventName, const std::string &target);
    void triggerJSEvent(const std::string &strWebTag, const std::string &eventName, const std::string &target,
                        const std::string &data);
    void triggerWindowJSEvent(const std::string &strWebTag, const std::string &eventName);
    void triggerWindowJSEvent(const std::string &strWebTag, const std::string &eventName, const std::string &data);
    void triggerDocumentJSEvent(const std::string &strWebTag, const std::string &eventName);
    void triggerDocumentJSEvent(const std::string &strWebTag, const std::string eventName, const std::string &data);
    MessageHandler *getMessageHandler();
    /**
     * @brief Determine whether the rawfile contains H5 resources for the capacitor SDK
     * contain native-bridge.js
     */
    static bool isCapacitorRawfile(const std::string &strDir);
    static bool isCapacitorSandbox(const std::string &strDir);
    /** Capacitor Automatically inject cordova.js, native-bridge.js, and cordova_plugin.js into index.html*/
    static void AutoInjectCordovaJs(const std::string &strRawfilePath, const std::string &strWebTag);
    static void ReadRawFileContent(const std::string &path, std::string &content);
    static void ProcessBaseScripts(std::string &strIndexFileContent);
    static void ProcessCordovaFiles(const std::string &strDir, std::string &strIndexFileContent);
    static void ProcessCapacitorPlugins(const std::string &strWebTag, std::string &strIndexFileContent);
    static void InsertAtHtmlHead(std::string &strIndexFileContent, const std::string &strInsertContent);
    static void AddMethodToJson(cJSON *pArray, const std::string &name, const std::string &rtype);
    static void AppendMethodToString(std::string &strLines, const std::string &pName, const std::string &mName,
                                     const std::string &rType);
    static void AppendTsPluginToJs(const std::string &pluginName, std::map<std::string, std::string> &mapMethods,
                                   std::string &strLines, cJSON *pPluginHeaders);
    static void AppendPluginToJs(const std::string &pluginName, PluginHandle *pluginHandle, std::string &strLines,
                                 cJSON *pPluginHeaders);

    static std::string getInjectCapacitorJs(const std::string &strRawfilePath, const std::string &strWebTag);
    static void AppendCordovaScripts(const std::string &strDir, bool isRawfile, std::string &strContent);
    static std::string GenerateCapacitorPluginsJs(const std::string &strWebTag);

private:
    static void InjectCordovaJs(const std::string &strSandboxPath, const std::string &strWebTag);
    static void InjectSandboxCordovaFiles(const std::string &strDir, std::string &strIndexFileContent);
    static void InjectSandboxPlugins(const std::string &strWebTag, std::string &strIndexFileContent);
    static void ReadCacheFileContent(const std::string &path, std::string &content);

    static std::string getRawfileFileContent(const std::string &strSandboxPath);
    static std::string getSandboxFileContent(const std::string &strSandboxPath);
};

#endif // MYAPPLICATION_BRIDGE_H
