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

#include "Bridge.h"
#include "CordovaViewController.h"
#include "FileCache.h"
#include "MemPool.h"
#include "cJSON.h"
#include <regex>

const std::string Bridge::DEFAULT_WEB_ASSET_DIR = "public";

Bridge::Bridge(const std::string &strWebTag, Capacitor::PluginManager *pPluginManager, MessageQueue *pMessageQueue,
               ServerPath serverPath, CapConfig *pConfig)
{
    m_strWebTag = strWebTag;
    m_serverPath = serverPath;
    m_pPluginManager = pPluginManager;
    m_pMessageQueue = pMessageQueue;

    m_pCapConfig = pConfig;
    m_pMessageHandler = new MessageHandler(this, strWebTag);
}

Bridge *Bridge::Builder::create(const std::string &strWebTag, Capacitor::PluginManager *pPluginManager,
                                MessageQueue *pMessageQueue)
{
    Bridge *pBridge = new Bridge(strWebTag, pPluginManager, pMessageQueue, m_serverPath, m_pCapConfig);
    return pBridge;
}

Bridge::Builder *Bridge::Builder::setConfig(CapConfig *pConfig)
{
    m_pCapConfig = pConfig;
    return this;
}

PluginHandle *Bridge::getPlugin(const std::string &strPluginId)
{
    return m_pPluginManager->getPluginHandle(m_strWebTag, strPluginId);
}

void Bridge::callPluginMethod(const std::string &pluginId, const std::string &methodName,
                              const std::string &strMethodData)
{
    PluginHandle *pluginHandle = getPlugin(pluginId);
    if (pluginHandle == nullptr) {
        return;
    }

    PluginCall call(m_pMessageHandler, pluginId, methodName, strMethodData);
    pluginHandle->invoke(methodName, call);
}

void Bridge::callPluginMethod(const std::string &strPluginId, const std::string &strMethodName, PluginCall &call)
{
    PluginHandle *pluginHandle = getPlugin(strPluginId);
    if (pluginHandle == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Bridge", "pluginHandle == nullptr:strPluginId:%{public}s",
                     strPluginId.c_str());
        call.errorCallback("Unable to find plugin:" + strPluginId);
        return;
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Bridge",
                 "callback:%{public}s,pluginId:%{public}s,methodName:%{public}s", call.getCallbackId().c_str(),
                 strPluginId.c_str(), strMethodName.c_str());
    if (strMethodName == "addListener") {
        pluginHandle->addListener(call);
    } else if (strMethodName == "removeListener") {
        pluginHandle->removeListener(call);
    } else if (strMethodName == "removeAllListeners") {
        pluginHandle->removeAllListeners(call);
    } else if (strMethodName == "checkPermissions") {
        pluginHandle->checkPermissions(call);
    } else if (strMethodName == "requestPermissions") {
        pluginHandle->requestPermissions(call);
    } else {
        pluginHandle->invoke(strMethodName, call);
    }
}

void Bridge::SendResponseMessage(const std::string &strWebTag, const std::string &strJsStatement) const
{
    m_pMessageQueue->addJavaScript(strWebTag, strJsStatement);
}

void Bridge::eval(const std::string &strWebTag, const std::string &js) const
{
    m_pMessageQueue->addJavaScript(strWebTag, js);
}
void Bridge::logToJs(const std::string &strWebTag, const std::string &message, const std::string &level)
{
    eval(strWebTag, "window.Capacitor.logJs(\"" + message + "\", \"" + level + "\")");
}
void Bridge::logToJs(const std::string &strWebTag, const std::string &message) { logToJs(strWebTag, message, "log"); }
void Bridge::triggerJSEvent(const std::string &strWebTag, const std::string &eventName, const std::string &target)
{
    eval(strWebTag, "window.Capacitor.triggerEvent(\"" + eventName + "\", \"" + target + "\")");
}
void Bridge::triggerJSEvent(const std::string &strWebTag, const std::string &eventName, const std::string &target,
                            const std::string &data)
{
    eval(strWebTag, "window.Capacitor.triggerEvent(\"" + eventName + "\", \"" + target + "\", " + data + ")");
}
void Bridge::triggerWindowJSEvent(const std::string &strWebTag, const std::string &eventName)
{
    triggerJSEvent(strWebTag, eventName, "window");
}
void Bridge::triggerWindowJSEvent(const std::string &strWebTag, const std::string &eventName, const std::string &data)
{
    triggerJSEvent(strWebTag, eventName, "window", data);
}
void Bridge::triggerDocumentJSEvent(const std::string &strWebTag, const std::string &eventName)
{
    triggerJSEvent(strWebTag, eventName, "document");
}
void Bridge::triggerDocumentJSEvent(const std::string &strWebTag, const std::string eventName, const std::string &data)
{
    triggerJSEvent(strWebTag, eventName, "document", data);
}

MessageHandler *Bridge::getMessageHandler() { return m_pMessageHandler; }

bool Bridge::isCapacitorSandbox(const std::string &strDir)
{
    std::string strNativeBridge = strDir + "/native-bridge.js";
    return FileCache::IsFile(strNativeBridge, "/");
}

bool Bridge::isCapacitorRawfile(const std::string &strDir)
{
    std::string strNativeBridge = strDir + "/native-bridge.js";
    RawFile *rawFile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, strNativeBridge.c_str());
    if (rawFile == nullptr) {
        return false;
    }
    OH_ResourceManager_CloseRawFile(rawFile);
    return true;
}

// --- InjectCordovaJs辅助函数：统一 FileCache 文件读取 ---
void Bridge::ReadCacheFileContent(const std::string &path, std::string &content)
{
    FILE *file = FileCache::openFile("/" + path, "");
    if (!file) {
        return;
    }

    std::vector<SMemPage> vecMemPage;
    const int blockSize = ((CMemPool *)Application::g_memPool)->GetPageSize() - 1;
    ((CMemPool *)Application::g_memPool)->MallocPage(vecMemPage, 1);
    if (vecMemPage.empty()) {
        FileCache::closeFile(file);
        return;
    }

    char *buffer = vecMemPage[0].m_point;
    long consumed = 0;
    while (true) {
        int ret = FileCache::readFile(file, (unsigned char *)buffer, blockSize, consumed);
        if (ret <= 0) {
            break;
        }
        buffer[ret] = 0;
        content += buffer;
        consumed += ret;
        std::fill_n(buffer, blockSize, 0);
    }
    ((CMemPool *)Application::g_memPool)->FreePage(vecMemPage);
    FileCache::closeFile(file);
}

// --- InjectCordovaJs辅助函数：处理 Capacitor 插件注入 (针对 Sandbox) ---
void Bridge::InjectSandboxPlugins(const std::string &strWebTag, std::string &strIndexFileContent)
{
    if (strIndexFileContent.find("Capacitor Plugin JS") != std::string::npos) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "Bridge", "Capacitor has been injected");
        return;
    }

    Capacitor::PluginManager *pCapManger =
        ((CordovaViewController *)Application::g_cordovaViewController)->getCapacitorPluginManager();
    std::string strLines = "\n<script>\n// Begin: Capacitor Plugin JS\n";
    cJSON *pPluginHeaders = cJSON_CreateArray();

    const std::map<std::string, PluginHandle *> &mapPlugins = pCapManger->getMapPlugins();
    for (auto it = mapPlugins.begin(); it != mapPlugins.end(); it++) {
        AppendPluginToJs(it->first, it->second, strLines, pPluginHeaders);
    }

    std::map<std::string, std::map<std::string, std::string>> mapTsPluginToMethod;
    pCapManger->getTsPluginInfo(strWebTag, mapTsPluginToMethod);
    for (auto it = mapTsPluginToMethod.begin(); it != mapTsPluginToMethod.end(); it++) {
        AppendTsPluginToJs(it->first, it->second, strLines, pPluginHeaders);
    }

    char *pHeaderStr = cJSON_Print(pPluginHeaders);
    strLines.append("window.Capacitor.PluginHeaders = ").append(pHeaderStr).append(";\n</script>");
    free(pHeaderStr);
    cJSON_Delete(pPluginHeaders);
    InsertAtHtmlHead(strIndexFileContent, strLines);
}

// --- InjectCordovaJs辅助函数：处理 Sandbox 中的 Cordova 相关文件 ---
void Bridge::InjectSandboxCordovaFiles(const std::string &strDir, std::string &strIndexFileContent)
{
    std::string strPluginsContent;
    ReadCacheFileContent(strDir + "/cordova_plugins.js", strPluginsContent);
    if (!strPluginsContent.empty()) {
        std::string strJsScripts = "\n";
        std::regex pattern("\"file\":\\s*\"([^\"]+\\.js)\"");
        std::smatch match;
        std::string::const_iterator sStart(strPluginsContent.cbegin());
        std::string::const_iterator sEnd(strPluginsContent.cend());
        while (std::regex_search(sStart, sEnd, match, pattern)) {
            if (match.size() > 1)
                strJsScripts.append("<script src='").append(match[1].str()).append("'></script>");
            sStart = match.suffix().first;
        }
        InsertAtHtmlHead(strIndexFileContent, strJsScripts);
    }

    std::string strCordovaJsContent;
    ReadCacheFileContent(strDir + "/cordova.js", strCordovaJsContent);
    if (!strCordovaJsContent.empty()) {
        strCordovaJsContent = "\n<script >\n" + strCordovaJsContent + strPluginsContent + "\n</script>";
        InsertAtHtmlHead(strIndexFileContent, strCordovaJsContent);
    }
}

// --- 核心入口函数：InjectCordovaJs ---
void Bridge::InjectCordovaJs(const std::string &strSandboxPath, const std::string &strWebTag)
{
    std::string strDir = strSandboxPath.substr(0, strSandboxPath.find("index.html"));
    if (!strDir.empty() && (strDir.back() == '/' || strDir.back() == '\\')) {
        strDir.pop_back();
    }

    if (!Bridge::isCapacitorSandbox(strDir)) {
        return;
    }

    std::string strIndexFileContent;
    ReadCacheFileContent(strSandboxPath, strIndexFileContent);
    if (strIndexFileContent.empty()) {
        return;
    }

    // 注入插件逻辑
    InjectSandboxPlugins(strWebTag, strIndexFileContent);

    // 如果没有找到头部，说明 InjectSandboxPlugins 可能因为没有 <head> 返回了
    if (strIndexFileContent.find("<head>") == std::string::npos &&
        strIndexFileContent.find("<HEAD>") == std::string::npos) {
        return;
    }

    InjectSandboxCordovaFiles(strDir, strIndexFileContent);
    ProcessBaseScripts(strIndexFileContent); // 复用之前的 ProcessBaseScripts

    int nSize = FileCache::writeFile("/" + strSandboxPath, strIndexFileContent.c_str(), strIndexFileContent.size());
    if (nSize != static_cast<int>(strIndexFileContent.size())) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "Bridge", "Write file size error");
    }
}

// --- AutoInjectCordovaJs辅助函数 1: 统一的文件读取逻辑 ---
void Bridge::ReadRawFileContent(const std::string &path, std::string &content)
{
    RawFile *rawfile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, path.c_str());
    if (!rawfile) {
        return;
    }

    const long fileSize = OH_ResourceManager_GetRawFileSize(rawfile);
    std::vector<SMemPage> vecMemPage;
    const int blockSize = ((CMemPool *)Application::g_memPool)->GetPageSize() - 1;
    ((CMemPool *)Application::g_memPool)->MallocPage(vecMemPage, 1);
    if (vecMemPage.empty()) {
        OH_ResourceManager_CloseRawFile(rawfile);
        return;
    }

    char *buffer = vecMemPage[0].m_point;
    long consumed = 0;
    while (consumed < fileSize) {
        OH_ResourceManager_SeekRawFile(rawfile, consumed, 0);
        int ret = OH_ResourceManager_ReadRawFile(rawfile, buffer, blockSize);
        if (ret <= 0) {
            break;
        }
        buffer[ret] = 0;
        content += buffer;
        consumed += ret;
        std::fill_n(buffer, blockSize, 0);
    }
    ((CMemPool *)Application::g_memPool)->FreePage(vecMemPage);
    OH_ResourceManager_CloseRawFile(rawfile);
}

// --- AutoInjectCordovaJs辅助函数 2: 查找并注入 HTML 头部 ---
void Bridge::InsertAtHtmlHead(std::string &strIndexFileContent, const std::string &strInsertContent)
{
    int nPos = strIndexFileContent.find("<head>");
    if (nPos == std::string::npos) {
        nPos = strIndexFileContent.find("<HEAD>");
    }
    if (nPos != std::string::npos) {
        strIndexFileContent.insert(nPos + 6, strInsertContent);
    }
}

// --- AutoInjectCordovaJs辅助函数 3: 生成并注入 Capacitor Plugin JS ---
void Bridge::ProcessCapacitorPlugins(const std::string &strWebTag, std::string &strIndexFileContent)
{
    if (strIndexFileContent.find("Capacitor Plugin JS") != std::string::npos) {
        return;
    }

    Capacitor::PluginManager *pCapManger =
        ((CordovaViewController *)Application::g_cordovaViewController)->getCapacitorPluginManager();
    std::string strLines = "\n<script>\n// Begin: Capacitor Plugin JS\n";
    cJSON *pPluginHeaders = cJSON_CreateArray();

    // 注入 Native Plugins 逻辑 (此处建议保持原逻辑拼接)
    const std::map<std::string, PluginHandle *> &mapPlugins = pCapManger->getMapPlugins();
    for (auto it = mapPlugins.begin(); it != mapPlugins.end(); it++) {
        AppendPluginToJs(it->first, it->second, strLines, pPluginHeaders);
    }

    // 注入 TS Plugins 逻辑
    std::map<std::string, std::map<std::string, std::string>> mapTsPluginToMethod;
    pCapManger->getTsPluginInfo(strWebTag, mapTsPluginToMethod);
    for (auto it = mapTsPluginToMethod.begin(); it != mapTsPluginToMethod.end(); it++) {
        AppendTsPluginToJs(it->first, it->second, strLines, pPluginHeaders);
    }

    char *pPluginHeader = cJSON_Print(pPluginHeaders);
    strLines.append("window.Capacitor.PluginHeaders = ").append(pPluginHeader).append(";\n</script>");
    free(pPluginHeader);
    cJSON_Delete(pPluginHeaders);
    InsertAtHtmlHead(strIndexFileContent, strLines);
}

// --- AutoInjectCordovaJs辅助函数 4: 处理 Cordova JS 和 Plugins ---
void Bridge::ProcessCordovaFiles(const std::string &strDir, std::string &strIndexFileContent)
{
    std::string strPluginsContent;
    ReadRawFileContent(strDir + "/cordova_plugins.js", strPluginsContent);

    if (!strPluginsContent.empty()) {
        std::string strJsScripts = "\n";
        std::regex pattern("\"file\":\\s*\"([^\"]+\\.js)\"");
        std::smatch match;

        // 【关键修复点】：确保 start 和 end 类型一致
        std::string::const_iterator searchStart(strPluginsContent.cbegin());
        std::string::const_iterator searchEnd(strPluginsContent.cend());

        while (std::regex_search(searchStart, searchEnd, match, pattern)) {
            if (match.size() > 1) {
                strJsScripts.append("<script src='").append(match[1].str()).append("'></script>");
            }
            searchStart = match.suffix().first;
        }
        InsertAtHtmlHead(strIndexFileContent, strJsScripts);
    }

    // ... 下方的 cordova.js 注入逻辑保持不变 ...
    std::string strCordovaJsContent;
    ReadRawFileContent(strDir + "/cordova.js", strCordovaJsContent);
    if (!strCordovaJsContent.empty()) {
        strCordovaJsContent = "\n<script >\n" + strCordovaJsContent + strPluginsContent + "\n</script>";
        InsertAtHtmlHead(strIndexFileContent, strCordovaJsContent);
    }
}

// --- AutoInjectCordovaJs辅助函数 5: 注入基础/全局脚本 ---
void Bridge::ProcessBaseScripts(std::string &strIndexFileContent)
{
    std::regex patternNative(R"(<script[^>]*src=[^>]*native-bridge\.js[^>]*>)", std::regex::icase);
    if (!std::regex_search(strIndexFileContent, patternNative)) {
        InsertAtHtmlHead(strIndexFileContent, "\n<script src='native-bridge.js'></script>");
    }

    if (strIndexFileContent.find("window.Capacitor") == std::string::npos) {
        std::string strGlobalJs = "<script>\n window.Capacitor = { DEBUG: ";
        strGlobalJs += (Application::g_isDebugMode ? "true" : "false");
        strGlobalJs += ",isLoggingEnabled:" + std::string(Application::g_isLoggingEnabled ? "true" : "false");
        strGlobalJs += ", Plugins: {} };\n</script>";
        InsertAtHtmlHead(strIndexFileContent, strGlobalJs);
    }
}

/*
 *对于capacitor框架,前端框架js代码默认是在首页自动注入的，OpenHarmony默认需要在capacitor.config.json中配置injectJs:true,才会自动注入
 *自动注入native-bridge.js cordova.js cordova_plugins.js文件
 */
void Bridge::AutoInjectCordovaJs(const std::string &strRawfilePath, const std::string &strWebTag)
{
    if (strRawfilePath.find("index.html") == std::string::npos) {
        return;
    }

    size_t idx = strRawfilePath.find("index.html");
    std::string strDir = strRawfilePath.substr(0, idx);
    if (!strDir.empty() && (strDir.back() == '/' || strDir.back() == '\\')) {
        strDir.pop_back();
    }

    std::string strBaseDir = Application::g_databaseDir.substr(1, Application::g_databaseDir.find("el2/database") - 1);
    if (strRawfilePath.find(strBaseDir) != std::string::npos) {
        InjectCordovaJs(strRawfilePath, strWebTag);
        return;
    }
    if (!Bridge::isCapacitorRawfile(strDir)) {
        return;
    }

    // 检查缓存逻辑
    Capacitor::PluginManager *pPM =
        ((CordovaViewController *)(Application::g_cordovaViewController))->getCapacitorPluginManager();
    std::string strFilePath = Application::g_strHotCodeUpdateDirectory;
    if (FileCache::IsFile("/" + strRawfilePath, strFilePath)) {
        if (pPM->getCapConfig()->getIsLoggingEnabled()) {
            FileCache::deleteFile(strFilePath + "/" + strRawfilePath);
        } else {
            return;
        }
    }

    std::string strIndexFileContent;
    ReadRawFileContent(strRawfilePath, strIndexFileContent);
    if (strIndexFileContent.empty()) {
        return;
    }

    ProcessCapacitorPlugins(strWebTag, strIndexFileContent);
    ProcessCordovaFiles(strDir, strIndexFileContent);
    ProcessBaseScripts(strIndexFileContent);

    if (FileCache::isDirectory(strFilePath) || FileCache::createDirectory(strFilePath)) {
        FileCache::writeFile(strFilePath + "/" + strRawfilePath, strIndexFileContent.c_str(),
                             strIndexFileContent.size());
    }
}

// --- 辅助函数：追加 Native Plugin 到 JS 字符串 ---
void Bridge::AppendPluginToJs(const std::string &pluginName, PluginHandle *pluginHandle, std::string &strLines,
                              cJSON *pPluginHeaders)
{
    strLines.append(
        "(function(w) {\nvar a = (w.Capacitor = w.Capacitor || {});\nvar p = (a.Plugins = a.Plugins || {});\n");
    strLines.append("var t = (p['").append(pluginName).append("'] = {});\n");
    strLines.append("t.addListener = function(eventName, callback) {\n  return w.Capacitor.addListener('");
    strLines.append(pluginName).append("', eventName, callback);\n};\n");

    cJSON *pMethodArray = cJSON_CreateArray();
    std::map<std::string, std::pair<PluginMethodFun, std::string>> mapMethods;

    if (CCapacitorPluginMethod::getMethods(pluginHandle->getClassName(), mapMethods)) {
        // 补充默认方法
        std::pair<PluginMethodFun, std::string> promiseMethod(nullptr, PluginMethod::RETURN_PROMISE);
        std::pair<PluginMethodFun, std::string> noneMethod(nullptr, PluginMethod::RETURN_NONE);
        mapMethods["removeAllListeners"] = promiseMethod;
        mapMethods["removeListener"] = noneMethod;
        mapMethods["checkPermissions"] = promiseMethod;
        mapMethods["requestPermissions"] = promiseMethod;

        for (auto itMethod = mapMethods.begin(); itMethod != mapMethods.end(); itMethod++) {
            if (itMethod->first == "onArKTsResult")
                continue;

            // 处理 JS 方法字符串生成
            AppendMethodToString(strLines, pluginName, itMethod->first, itMethod->second.second);

            // 处理 JSON 结构
            if (itMethod == mapMethods.begin()) {
                AddMethodToJson(pMethodArray, "addListener", PluginMethod::RETURN_CALLBACK);
            }
            AddMethodToJson(pMethodArray, itMethod->first, itMethod->second.second);
        }
    }
    strLines.append("})(window);\n");

    cJSON *pJson = cJSON_CreateObject();
    cJSON_AddStringToObject(pJson, "name", pluginName.c_str());
    cJSON_AddItemReferenceToObject(pJson, "methods", pMethodArray);
    cJSON_AddItemToArray(pPluginHeaders, pJson);
}

// --- 辅助函数：追加 TS Plugin 到 JS 字符串 ---
void Bridge::AppendTsPluginToJs(const std::string &pluginName, std::map<std::string, std::string> &mapMethods,
                                std::string &strLines, cJSON *pPluginHeaders)
{
    strLines.append(
        "(function(w) {\nvar a = (w.Capacitor = w.Capacitor || {});\nvar p = (a.Plugins = a.Plugins || {});\n");
    strLines.append("var t = (p['").append(pluginName).append("'] = {});\n");
    strLines.append("t.addListener = function(eventName, callback) {\n  return w.Capacitor.addListener('");
    strLines.append(pluginName).append("', eventName, callback);\n};\n");

    cJSON *pMethodArray = cJSON_CreateArray();
    mapMethods["removeAllListeners"] = PluginMethod::RETURN_PROMISE;
    mapMethods["removeListener"] = PluginMethod::RETURN_NONE;
    mapMethods["checkPermissions"] = PluginMethod::RETURN_PROMISE;
    mapMethods["requestPermissions"] = PluginMethod::RETURN_PROMISE;

    for (auto itMethod = mapMethods.begin(); itMethod != mapMethods.end(); itMethod++) {
        AppendMethodToString(strLines, pluginName, itMethod->first, itMethod->second);

        if (itMethod == mapMethods.begin()) {
            AddMethodToJson(pMethodArray, "addListener", PluginMethod::RETURN_CALLBACK);
        }
        AddMethodToJson(pMethodArray, itMethod->first, itMethod->second);
    }

    strLines.append("})(window);\n");
    cJSON *pJson = cJSON_CreateObject();
    cJSON_AddStringToObject(pJson, "name", pluginName.c_str());
    cJSON_AddItemReferenceToObject(pJson, "methods", pMethodArray);
    cJSON_AddItemToArray(pPluginHeaders, pJson);
}

// --- 内部微型工具函数：处理具体的 JS 方法行生成 ---
void Bridge::AppendMethodToString(std::string &strLines, const std::string &pName, const std::string &mName,
                                  const std::string &rType)
{
    const std::string strOpt = "_options";
    const std::string strCall = "_options,_callback";

    strLines.append("t['").append(mName).append("'] = function(");
    strLines.append(rType == PluginMethod::RETURN_CALLBACK ? strCall : strOpt).append(") {\n");

    if (rType == PluginMethod::RETURN_NONE) {
        strLines.append("return w.Capacitor.nativeCallback('")
            .append(pName)
            .append("', '")
            .append(mName)
            .append("', ")
            .append(strOpt)
            .append(");");
    } else if (rType == PluginMethod::RETURN_PROMISE) {
        strLines.append("return w.Capacitor.nativePromise('")
            .append(pName)
            .append("', '")
            .append(mName)
            .append("', ")
            .append(strOpt)
            .append(");");
    } else if (rType == PluginMethod::RETURN_CALLBACK) {
        strLines.append("return w.Capacitor.nativeCallback('")
            .append(pName)
            .append("', '")
            .append(mName)
            .append("', ")
            .append(strCall)
            .append(");");
    }
    strLines.append("\n};\n");
}

// --- 内部微型工具函数：处理 JSON 节点添加 ---
void Bridge::AddMethodToJson(cJSON *pArray, const std::string &name, const std::string &rtype)
{
    cJSON *pMethod = cJSON_CreateObject();
    cJSON_AddStringToObject(pMethod, "name", name.c_str());
    if (rtype != PluginMethod::RETURN_NONE) {
        cJSON_AddStringToObject(pMethod, "rtype", rtype.c_str());
    }
    cJSON_AddItemToArray(pArray, pMethod);
}

// --- getInjectCapacitorJs 辅助函数 1: 处理 Cordova 相关内容的拼接 (已修复 this 报错) ---
void Bridge::AppendCordovaScripts(const std::string &strDir, bool isRawfile, std::string &strContent)
{
    // 注入 cordova.js
    if (isRawfile) {
        strContent += "\n" + getRawfileFileContent(strDir + "cordova.js") + "\n";
    } else {
        strContent += "\n" + getSandboxFileContent(strDir + "cordova.js") + "\n";
    }
    std::string strPluginsJs = isRawfile ? getRawfileFileContent(strDir + "cordova_plugins.js")
                                         : getSandboxFileContent(strDir + "cordova_plugins.js");
    if (!strPluginsJs.empty()) {
        std::regex pattern("\"file\":\\s*\"([^\"]+\\.js)\"");
        std::smatch match;
        std::string::const_iterator sStart = strPluginsJs.cbegin();
        while (std::regex_search(sStart, strPluginsJs.cend(), match, pattern)) {
            if (match.size() > 1) {
                std::string jsPath = strDir + match[1].str();
                strContent += "\n" + (isRawfile ? getRawfileFileContent(jsPath) : getSandboxFileContent(jsPath)) + "\n";
            }
            sStart = match.suffix().first;
        }
    }
    strContent += strPluginsJs;
}

// --- getInjectCapacitorJs 辅助函数 2: 生成 Capacitor 核心 JS 逻辑 (保持 50 行内且带花括号) ---
std::string Bridge::GenerateCapacitorPluginsJs(const std::string &strWebTag)
{
    auto *pVC = (CordovaViewController *)Application::g_cordovaViewController;
    Capacitor::PluginManager *pPM = pVC->getCapacitorPluginManager();
    std::string strLines = "\n// Begin: Capacitor Plugin JS\n";
    cJSON *pHeaders = cJSON_CreateArray();

    // 处理 Native Plugins
    const std::map<std::string, PluginHandle *> &mapPlugins = pPM->getMapPlugins();
    for (auto it = mapPlugins.begin(); it != mapPlugins.end(); it++) {
        AppendPluginToJs(it->first, it->second, strLines, pHeaders);
    }

    // 处理 TS Plugins
    std::map<std::string, std::map<std::string, std::string>> mapTs;
    pPM->getTsPluginInfo(strWebTag, mapTs);
    for (auto itTs = mapTs.begin(); itTs != mapTs.end(); itTs++) {
        AppendTsPluginToJs(itTs->first, itTs->second, strLines, pHeaders);
    }

    char *pPrint = cJSON_Print(pHeaders);
    if (pPrint != nullptr) {
        strLines.append("window.Capacitor.PluginHeaders = ").append(pPrint).append(";");
        free(pPrint);
    }
    cJSON_Delete(pHeaders);
    return strLines + "\n";
}

// --- getInjectCapacitorJs 主入口函数: getInjectCapacitorJs (严格符合门禁：嵌套<=4, 行数<=50) ---
std::string Bridge::getInjectCapacitorJs(const std::string &strRawfilePath, const std::string &strWebTag)
{
    std::string dbDir = Application::g_databaseDir;
    std::string strBase = dbDir.substr(1, dbDir.find("el2/database") - 1);
    bool isRawfile = (strRawfilePath.find(strBase) == std::string::npos);

    std::string strDir = strRawfilePath;
    if (strDir.empty() || (strDir.back() != '/' && strDir.back() != '\\')) {
        strDir += "/";
    }

    // 1. 全局基础配置
    std::string strResult = "\n window.Capacitor = { DEBUG: ";
    strResult += (Application::g_isDebugMode ? "true" : "false");
    strResult += ",isLoggingEnabled:" + std::string(Application::g_isLoggingEnabled ? "true" : "false");
    strResult += ", Plugins: {} };\n";

    // 2. 桥接脚本与 Cordova 脚本
    strResult += "\n" + getRawfileFileContent("native-bridge.js") + "\n";
    AppendCordovaScripts(strDir, isRawfile, strResult);

    // 3. Capacitor 插件逻辑
    strResult += GenerateCapacitorPluginsJs(strWebTag);

    return strResult;
}

std::string Bridge::getRawfileFileContent(const std::string &strRawfilePath)
{
    std::string strFilePath = strRawfilePath;
    if (strRawfilePath.find("/") == 0) {
        strFilePath = strRawfilePath.substr(1);
    }

    std::string strFileContent = "";
    RawFile *pCordovaPluginsFile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, strFilePath.c_str());
    if (pCordovaPluginsFile != nullptr) {
        const long fileSize = OH_ResourceManager_GetRawFileSize(pCordovaPluginsFile);
        std::vector<SMemPage> vecMemPage;
        const int blockSize = (static_cast<CMemPool *>(Application::g_memPool))->getPageSize() - 1;
        long consumed = 0;
        (static_cast<CMemPool *>(Application::g_memPool))->mallocPage(vecMemPage, 1);
        if (vecMemPage.size() < 1) {
            return "";
        }

        char *buffer = vecMemPage[0].m_point;
        while (true) {
            OH_ResourceManager_SeekRawFile(pCordovaPluginsFile, consumed, 0);
            int ret = OH_ResourceManager_ReadRawFile(pCordovaPluginsFile, buffer, blockSize);
            if (ret == 0) {
                break;
            }
            buffer[ret] = 0;
            strFileContent += buffer;
            if (consumed + ret >= fileSize) {
                break;
            }
            consumed += ret;
            std::fill(buffer, buffer + blockSize, 0);
        }
        (static_cast<CMemPool *>(Application::g_memPool))->freePage(vecMemPage);
        OH_ResourceManager_CloseRawFile(pCordovaPluginsFile);
    }
    return strFileContent;
}

std::string Bridge::getSandboxFileContent(const std::string &strSandboxPath)
{
    std::string strFilePath = strSandboxPath;
    if (strSandboxPath.find("/") != 0) {
        strFilePath = "/" + strSandboxPath;
    }

    std::string strIndexFileContent = "";
    FILE *file = FileCache::openFile(strSandboxPath, "");
    if (!file) {
        return strIndexFileContent;
    }
    std::vector<SMemPage> vecMemPage;
    const int blockSize = (static_cast<CMemPool *>(Application::g_memPool))->getPageSize() - 1;
    long consumed = 0;
    (static_cast<CMemPool *>(Application::g_memPool))->mallocPage(vecMemPage, 1);
    if (vecMemPage.size() < 1) {
        return strIndexFileContent;
    }

    char *buffer = vecMemPage[0].m_point;
    while (true) {
        int ret = FileCache::readFile(file, (unsigned char *)buffer, blockSize, consumed);
        if (ret == 0) {
            break;
        }
        buffer[ret] = 0;
        strIndexFileContent += buffer;
        consumed += ret;
        std::fill(buffer, buffer + blockSize, 0);
    }

    (static_cast<CMemPool *>(Application::g_memPool))->freePage(vecMemPage);
    FileCache::closeFile(file);
    return strIndexFileContent;
}