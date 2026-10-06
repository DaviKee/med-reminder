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

#include "HotCodePushPlugin.h"
#include "getcapacitor/Plugin.h"
#include "cJSON.h"
#include <bundle/native_interface_bundle.h>
#include "Application.h"
#include "CordovaViewController.h"
#include "FileCache.h"
#include "ConnPool.h"
#include <vector>
#include <string>
#include <map>
#include <atomic>

REGISTER_CAP_PLUGIN(HotCodePushPlugin, HotCodePushPlugin)

REGISTER_PLUGIN_METHOD(HotCodePushPlugin, fetchUpdate, PluginMethod::RETURN_PROMISE)
void HotCodePushPlugin::fetchUpdate(PluginCall &call)
{
    cJSON *pJson = cJSON_GetArrayItem(call.getData(), 0);
    m_fetchUpdateOptions.InitFetchUpdateOptions(pJson);
    if (!m_fetchUpdateOptions.getConfigUrl().empty()) {
        m_strChcpConfigUrl = m_fetchUpdateOptions.getConfigUrl();
    }
    PluginInitialize();
    m_call = call;
    m_checkUpdatePlugin.start(this);
}

REGISTER_PLUGIN_METHOD(HotCodePushPlugin, installUpdate, PluginMethod::RETURN_PROMISE)
void HotCodePushPlugin::installUpdate(PluginCall &call)
{
    if (m_nUpdateFlag == 1) {
        OH_NativeBundle_ApplicationInfo application = OH_NativeBundle_GetCurrentApplicationInfo();
        string strBundleName = application.bundleName;

        executeArkTs("./PluginAction/ReStartApp/ReStartApp", 0, strBundleName.c_str(), "HotCodePushPlugin", call);
        return;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "action", "chcp_nothingToUpdate");
    cJSON_AddNumberToObject(json, "error", errorNothingToUpdate);
    cJSON_AddNullToObject(json, "data");
    call.resolve(json);
    cJSON_Delete(json);
}

REGISTER_PLUGIN_METHOD(HotCodePushPlugin, execute, PluginMethod::RETURN_PROMISE)
void HotCodePushPlugin::execute(PluginCall &call)
{
    string action = call.getString("action");
    cJSON *args = cJSON_GetObjectItem(call.getData(), "args");

    if (action == "jsInitPlugin") {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "action", "chcp_nothingToUpdate");
        cJSON_AddNumberToObject(json, "error", errorNothingToUpdate);
        cJSON_AddNullToObject(json, "data");
        call.resolve(json);
        cJSON_Delete(json);
    }

    if (action == "jsFetchUpdate") {
        cJSON *pJson = cJSON_GetArrayItem(args, 0);
        m_fetchUpdateOptions.InitFetchUpdateOptions(pJson);
        if (!m_fetchUpdateOptions.getConfigUrl().empty()) {
            m_strChcpConfigUrl = m_fetchUpdateOptions.getConfigUrl();
        }
        m_call = call;
        m_checkUpdatePlugin.start(this);
    }

    if (action == "jsInstallUpdate" || action == "jsRequestAppUpdate" ||
        action == "jsIsUpdateAvailableForInstallation") {
        if (m_nUpdateFlag == 1) {
            OH_NativeBundle_ApplicationInfo application = OH_NativeBundle_GetCurrentApplicationInfo();
            string strBundleName = application.bundleName;

            executeArkTs("./PluginAction/ReStartApp/ReStartApp", 0, strBundleName.c_str(), "HotCodePushPlugin", call);
            return;
        }

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "action", "chcp_nothingToUpdate");
        cJSON_AddNumberToObject(json, "error", errorNothingToUpdate);
        cJSON_AddNullToObject(json, "data");
        call.resolve(json);
        cJSON_Delete(json);
    }
}

void HotCodePushPlugin::PluginInitialize()
{
    m_pBuf = malloc(constMaxBuf);
    m_pChcpFile = NULL;
    m_pCacheChcpFile = NULL;
    m_pChcpManifestFile = NULL;
    m_pChcpChacheManifestFile = NULL;
    Application::g_strHotCodeUpdateDirectory =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files/chcp";
    ParseCordovaConfigXml();
}


void HotCodePushPlugin::ParseCordovaConfigXml()
{
    Capacitor::PluginManager *pPluginManager =
        ((CordovaViewController *)(Application::g_cordovaViewController))->getCapacitorPluginManager();
    CapConfig *pCapConfig = pPluginManager->getCapConfig();
    PluginConfig *pPluginConfig = pCapConfig->getPluginConfiguration("chcp");
    if (pPluginConfig != nullptr) {
        cJSON *pJson = pPluginConfig->getConfigJSON();
        if (pJson != nullptr) {
            // 从pJson直接获取配置值
            cJSON *pConfigFile = cJSON_GetObjectItem(pJson, "config-file");
            if (pConfigFile != nullptr) {
                m_strChcpConfigUrl = pConfigFile->valuestring;
            }

            cJSON *pAutoDownload = cJSON_GetObjectItem(pJson, "auto-download");
            if (pAutoDownload != nullptr) {
                m_isAllowUpdatesAutoDownload = (pAutoDownload->type == cJSON_True);
            } else {
                m_isAllowUpdatesAutoDownload = false; // 默认值
            }

            cJSON *pAutoInstall = cJSON_GetObjectItem(pJson, "auto-install");
            if (pAutoInstall != nullptr) {
                m_isAllowUpdatesAutoInstall = (pAutoInstall->type == cJSON_True);
            } else {
                m_isAllowUpdatesAutoInstall = false; // 默认值
            }

            cJSON *pNativeInterface = cJSON_GetObjectItem(pJson, "native-interface");
            if (pNativeInterface != nullptr) {
                m_nActiveInterfaceVersion = pNativeInterface->valueint;
            } else {
                m_nActiveInterfaceVersion = 0; // 默认值
            }
        }
    }
}

// 内部使用的下载方法，与txt文件中的签名一致
bool HotCodePushPlugin::downLoadFile(const string &strSource, const string &strTarget)
{
    URLInfo urlInfo;
    if (!ParseUrl(strSource, urlInfo)) {
        return false;
    }

    string httpRequest = BuildHttpRequest(urlInfo);
    ConnPool *connectionPool = GetConnectionPool(urlInfo.domain);
    if (!connectionPool) {
        return false;
    }

    return AttemptDownload(connectionPool, httpRequest, strTarget, urlInfo.isHttps);
}

bool HotCodePushPlugin::ParseUrl(const string &url, URLInfo &urlInfo)
{
    string trimmedUrl = url;
    // Remove leading and trailing spaces
    trimmedUrl.erase(0, trimmedUrl.find_first_not_of(" "));
    trimmedUrl.erase(trimmedUrl.find_last_not_of(" ") + 1);

    // Parse protocol type
    if (trimmedUrl.find("https://") == 0) {
        urlInfo.isHttps = true;
    } else if (trimmedUrl.find("http://") == 0) {
        urlInfo.isHttps = false;
    } else {
        return false;
    }

    // Extract domain and request path
    size_t protocolEnd = trimmedUrl.find("//");
    if (protocolEnd == string::npos) {
        return false;
    }

    string fullPath = trimmedUrl.substr(protocolEnd + 2);
    size_t pathStart = fullPath.find("/");
    if (pathStart != string::npos) {
        urlInfo.domain = fullPath.substr(0, pathStart);
        urlInfo.requestPath = fullPath.substr(pathStart);
    } else {
        urlInfo.domain = fullPath;
        urlInfo.requestPath = "/";
    }

    return !urlInfo.domain.empty();
}

string HotCodePushPlugin::BuildHttpRequest(const URLInfo &urlInfo)
{
    string request = "GET " + urlInfo.requestPath + " HTTP/1.1\r\n";
    request += "Host: " + urlInfo.domain + "\r\n";

    // Add custom request headers
    map<string, string> headers = m_fetchUpdateOptions.getRequestHeaders();
    for (const auto &header : headers) {
        request += header.first + ": " + header.second + "\r\n";
    }

    // Add standard headers
    request += "Connection: Keep-Alive\r\n";
    request += "Keep-Alive: timeout=5, max=1000\r\n";
    request += "\r\n";
    return request;
}

ConnPool *HotCodePushPlugin::GetConnectionPool(const string &domain)
{
    ConnPoolManage *poolManager = (ConnPoolManage *)Application::g_connPoolManage;
    return poolManager->getConnPool(domain);
}

bool HotCodePushPlugin::AttemptDownload(ConnPool *pool, const string &request, const string &targetPath, bool isHttps)
{
    static const int MAX_RETRY_ATTEMPTS = 3;
    std::atomic<bool> isAbort{false};
    string fileName = targetPath;
    map<string, string> responseHeaders;

    for (int attempt = 0; attempt < MAX_RETRY_ATTEMPTS; attempt++) {
        bool success = isHttps ? DownloadHttps(pool, request, fileName, responseHeaders, isAbort)
                               : DownloadHttp(pool, request, fileName, responseHeaders, isAbort);
        if (success && FileCache::IsFile(fileName, "")) {
            return true;
        }
    }

    return false;
}

bool HotCodePushPlugin::DownloadHttps(ConnPool *pool, const string &request, string &fileName,
                                      map<string, string> &responseHeaders, std::atomic<bool> &isAbort)
{
    bool isConnectionClosed = false;
    SSL *sslConnection = nullptr;

    if (!pool->getSSL(&sslConnection)) {
        OH_LOG_ERROR(LOG_APP, "Failed to establish SSL connection");
        return false;
    }

    if (!SendRequest(pool, sslConnection, request, isAbort)) {
        pool->freeSocket(sslConnection, true);
        return false;
    }

    // Set member variables for ReceiveResponse
    m_currentConnPool = pool;
    m_currentSsl = sslConnection;
    m_currentSocket = -1;
    m_currentMethod = "GET";
    m_currentFileName = &fileName;
    m_currentHeaders = &responseHeaders;
    m_currentIsConnectionClosed = &isConnectionClosed;
    m_currentIsAbort = &isAbort;
    m_isUsingSsl = true;

    if (!ReceiveResponse()) {
        pool->freeSocket(sslConnection, isConnectionClosed);
        return false;
    }

    pool->freeSocket(sslConnection, isConnectionClosed);
    return true;
}

bool HotCodePushPlugin::DownloadHttp(ConnPool *pool, const string &request, string &fileName,
                                     map<string, string> &responseHeaders, std::atomic<bool> &isAbort)
{
    bool isConnectionClosed = false;
    int socket = -1;

    if (!pool->getSocket(socket)) {
        OH_LOG_ERROR(LOG_APP, "Failed to establish HTTP socket connection");
        return false;
    }

    if (!SendRequest(pool, socket, request, isAbort)) {
        pool->freeSocket(socket, true);
        return false;
    }

    // Set member variables for ReceiveResponse
    m_currentConnPool = pool;
    m_currentSsl = nullptr;
    m_currentSocket = socket;
    m_currentMethod = "GET";
    m_currentFileName = &fileName;
    m_currentHeaders = &responseHeaders;
    m_currentIsConnectionClosed = &isConnectionClosed;
    m_currentIsAbort = &isAbort;
    m_isUsingSsl = false;

    if (!ReceiveResponse()) {
        pool->freeSocket(socket, true);
        return false;
    }

    pool->freeSocket(socket, true);
    return true;
}

bool HotCodePushPlugin::SendRequest(ConnPool *pool, SSL *ssl, const string &request, std::atomic<bool> &isAbort)
{
    if (!pool->sendWithTimeOut(ssl, request, std::ref(isAbort))) {
        OH_LOG_ERROR(LOG_APP, "Failed to send HTTPS request");
        return false;
    }
    return true;
}

bool HotCodePushPlugin::SendRequest(ConnPool *pool, int socket, const string &request, std::atomic<bool> &isAbort)
{
    if (!pool->sendWithTimeOut(socket, request, std::ref(isAbort))) {
        OH_LOG_ERROR(LOG_APP, "Failed to send HTTP request");
        return false;
    }
    return true;
}

bool HotCodePushPlugin::ReceiveResponse()
{
    if (m_isUsingSsl) {
        if (!m_currentConnPool->recvHttpWithTimeOut(m_currentSsl, m_currentMethod, *m_currentFileName,
                                                    *m_currentHeaders, "", m_pBuf, constMaxBuf,
                                                    *m_currentIsConnectionClosed, std::ref(*m_currentIsAbort), NULL)) {
            OH_LOG_ERROR(LOG_APP, "Failed to receive HTTPS response");
            return false;
        }
    } else {
        if (!m_currentConnPool->recvHttpWithTimeOut(m_currentSocket, m_currentMethod, *m_currentFileName,
                                                    *m_currentHeaders, "", m_pBuf, constMaxBuf,
                                                    *m_currentIsConnectionClosed, std::ref(*m_currentIsAbort), NULL)) {
            OH_LOG_ERROR(LOG_APP, "Failed to receive HTTP response");
            return false;
        }
    }
    return true;
}

void HotCodePushPlugin::FetchUpdate()
{
    string strFilePath = GetInjectDirectoryPath();
    string strChcpFile = "chcp.json";
    string strChcpJsonFilePath = strFilePath + strChcpFile;
    string strWww = "www/";

    if (!ProcessLocalChcpJsonFile(strFilePath, strChcpFile, strChcpJsonFilePath, strWww)) {
        return;
    }

    string strChcpManifest = "chcp.manifest";
    string strChcpManifestFilePath = strFilePath + strChcpManifest;
    if (!ProcessLocalChcpManifestFile(strFilePath, strChcpManifest, strChcpManifestFilePath, strWww)) {
        return;
    }

    if (!LoadLocalChcpJsonFile(strChcpJsonFilePath)) {
        return;
    }

    string strCachePath = GetCacheDirectoryPath();
    string strCahceChcpJsonFilePath = strCachePath + strChcpFile;
    if (!LoadServerChcpJsonFile(strCahceChcpJsonFilePath)) {
        return;
    }

    string strRequest;
    if (!CheckForUpdates(strRequest)) {
        return;
    }

    if (!LoadLocalChcpManifestFile(strChcpManifestFilePath)) {
        return;
    }

    string strChcpCacheManifestFilePath = strCachePath + strChcpManifest;
    if (!LoadServerChcpManifestFile(strRequest, strChcpManifest, strChcpCacheManifestFilePath)) {
        return;
    }

    UpdateFiles(strFilePath, strWww, strRequest);

    Bridge::AutoInjectCordovaJs(strWww + "index.html", m_call.getWebTag());

    CopyServerFilesToCache(strCahceChcpJsonFilePath, strChcpJsonFilePath, strChcpCacheManifestFilePath,
                           strChcpManifestFilePath);
}

string HotCodePushPlugin::GetInjectDirectoryPath()
{
    return Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) +
           "/base/files/inject/";
}

string HotCodePushPlugin::GetCacheDirectoryPath()
{
    return Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/cache/";
}

bool HotCodePushPlugin::ProcessLocalChcpJsonFile(const string &strFilePath, const string &strChcpFile,
                                                 const string &strChcpJsonFilePath, const string &strWww)
{
    if (FileCache::IsFile(strChcpFile, strFilePath)) {
        return true;
    }

    string strTempChcpFile = strWww + strChcpFile;
    return CopyFileFromResources(strTempChcpFile, strChcpJsonFilePath);
}

bool HotCodePushPlugin::ProcessLocalChcpManifestFile(const string &strFilePath, const string &strChcpManifest,
                                                     const string &strChcpManifestFilePath, const string &strWww)
{
    if (FileCache::IsFile(strChcpManifest, strFilePath)) {
        return true;
    }

    string strTempChcpManifest = strWww + strChcpManifest;
    return CopyFileFromResources(strTempChcpManifest, strChcpManifestFilePath);
}

bool HotCodePushPlugin::CopyFileFromResources(const string &resourcePath, const string &targetPath)
{
    RawFile *rawfile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, resourcePath.c_str());
    if (!rawfile) {
        return false;
    }

    long consumed = 0;
    unsigned char *buffer = (unsigned char *)m_pBuf;
    const int MAX_ITERATIONS = 1000; // Safety limit
    int iterations = 0;

    while (iterations < MAX_ITERATIONS) {
        int ret = OH_ResourceManager_ReadRawFile(rawfile, buffer + consumed, constMaxBuf);
        if (ret == 0) {
            break;
        }
        consumed += ret;
        OH_ResourceManager_SeekRawFile(rawfile, consumed, 0);
        iterations++;
    }

    // Additional safety check
    if (iterations >= MAX_ITERATIONS) {
        OH_LOG_ERROR(LOG_APP, "Maximum iterations reached in CopyFileFromResources");
        OH_ResourceManager_CloseRawFile(rawfile);
        return false;
    }
    OH_ResourceManager_CloseRawFile(rawfile);
    buffer[consumed] = 0;

    return FileCache::writeFile(targetPath, (const char *)buffer, consumed);
}

bool HotCodePushPlugin::LoadLocalChcpJsonFile(const string &strChcpJsonFilePath)
{
    long consumed = 0;
    unsigned char *buffer = (unsigned char *)m_pBuf;
    std::fill_n(buffer, constMaxBuf, 0);

    if (!FileCache::readFile(strChcpJsonFilePath, buffer, constMaxBuf)) {
        return false;
    }

    if (m_pChcpFile != NULL) {
        cJSON_Delete(m_pChcpFile);
        m_pChcpFile = NULL;
    }

    m_pChcpFile = cJSON_Parse((const char *)buffer);
    return m_pChcpFile != NULL;
}

bool HotCodePushPlugin::LoadServerChcpJsonFile(const string &strCahceChcpJsonFilePath)
{
    FileCache::deleteFile(strCahceChcpJsonFilePath);
    if (!downLoadFile(m_strChcpConfigUrl, strCahceChcpJsonFilePath)) {
        return false;
    }

    long consumed = 0;
    unsigned char *buffer = (unsigned char *)m_pBuf;
    std::fill_n(buffer, constMaxBuf, 0);

    if (!FileCache::readFile(strCahceChcpJsonFilePath, buffer, constMaxBuf)) {
        return false;
    }

    if (m_pCacheChcpFile != NULL) {
        cJSON_Delete(m_pCacheChcpFile);
        m_pCacheChcpFile = NULL;
    }

    m_pCacheChcpFile = cJSON_Parse((const char *)buffer);
    return m_pCacheChcpFile != NULL;
}

bool HotCodePushPlugin::CheckForUpdates(string &strRequest)
{
    cJSON *pVersion = cJSON_GetObjectItem(m_pChcpFile, "release");
    if (!pVersion) {
        return false;
    }
    string strLocalVersion = pVersion->valuestring;

    cJSON *pCacheVersion = cJSON_GetObjectItem(m_pCacheChcpFile, "release");
    if (!pCacheVersion) {
        return false;
    }
    string strCacheVersion = pCacheVersion->valuestring;

    if (strLocalVersion == strCacheVersion) {
        return false;
    }

    cJSON *pContentUrl = cJSON_GetObjectItem(m_pCacheChcpFile, "content_url");
    if (!pContentUrl) {
        return false;
    }
    strRequest = pContentUrl->valuestring;
    return true;
}

bool HotCodePushPlugin::LoadLocalChcpManifestFile(const string &strChcpManifestFilePath)
{
    long consumed = 0;
    unsigned char *buffer = (unsigned char *)m_pBuf;
    std::fill_n(buffer, constMaxBuf, 0);

    if (!FileCache::readFile(strChcpManifestFilePath, buffer, constMaxBuf)) {
        return false;
    }

    if (m_pChcpManifestFile != NULL) {
        cJSON_Delete(m_pChcpManifestFile);
        m_pChcpManifestFile = NULL;
    }

    m_pChcpManifestFile = cJSON_Parse((const char *)buffer);
    return m_pChcpManifestFile != NULL;
}

bool HotCodePushPlugin::LoadServerChcpManifestFile(const string &strRequest, const string &strChcpManifest,
                                                   const string &strChcpCacheManifestFilePath)
{
    FileCache::deleteFile(strChcpCacheManifestFilePath);
    if (!downLoadFile(strRequest + "/" + strChcpManifest, strChcpCacheManifestFilePath)) {
        return false;
    }

    long consumed = 0;
    unsigned char *buffer = (unsigned char *)m_pBuf;
    std::fill_n(buffer, constMaxBuf, 0);

    if (!FileCache::readFile(strChcpCacheManifestFilePath, buffer, constMaxBuf)) {
        return false;
    }

    if (m_pChcpChacheManifestFile != NULL) {
        cJSON_Delete(m_pChcpChacheManifestFile);
        m_pChcpChacheManifestFile = NULL;
    }

    m_pChcpChacheManifestFile = cJSON_Parse((const char *)buffer);
    return m_pChcpChacheManifestFile != NULL;
}

void HotCodePushPlugin::UpdateFiles(const string &strFilePath, const string &strWww, const string &strRequest)
{
    vector<string> vecCacheFileName;
    vector<string> vecCahceFileHash;
    ParseManifestFile(m_pChcpChacheManifestFile, vecCacheFileName, vecCahceFileHash);

    vector<string> vecLocalFileName;
    vector<string> vecLocalFileHash;
    ParseManifestFile(m_pChcpManifestFile, vecLocalFileName, vecLocalFileHash);

    for (size_t i = 0; i < vecCacheFileName.size(); i++) {
        // Set member variables for UpdateFileIfNeeded
        m_updateFilePath = strFilePath;
        m_updateStrWww = strWww;
        m_updateStrRequest = strRequest;
        m_updateCacheFileName = vecCacheFileName[i];
        m_updateCacheFileHash = vecCahceFileHash[i];
        m_updateVecLocalFileName = vecLocalFileName;
        m_updateVecLocalFileHash = vecLocalFileHash;

        UpdateFileIfNeeded();
    }
}

void HotCodePushPlugin::ParseManifestFile(cJSON *pManifestFile, vector<string> &vecFileNames,
                                          vector<string> &vecFileHashes)
{
    int nFileCount = cJSON_GetArraySize(pManifestFile);
    for (int i = 0; i < nFileCount; i++) {
        cJSON *pFileNode = cJSON_GetArrayItem(pManifestFile, i);
        cJSON *pFileName = cJSON_GetObjectItem(pFileNode, "file");
        string strFileName = pFileName ? pFileName->valuestring : "";

        cJSON *pFileHash = cJSON_GetObjectItem(pFileNode, "hash");
        string strFileHash = pFileHash ? pFileHash->valuestring : "";

        vecFileNames.push_back(strFileName);
        vecFileHashes.push_back(strFileHash);
    }
}

void HotCodePushPlugin::UpdateFileIfNeeded()
{
    size_t j;
    for (j = 0; j < m_updateVecLocalFileName.size(); j++) {
        if (m_updateCacheFileName == m_updateVecLocalFileName[j]) {
            if (m_updateCacheFileHash != m_updateVecLocalFileHash[j]) {
                ReplaceFile(m_updateFilePath, m_updateStrWww, m_updateStrRequest, m_updateCacheFileName);
                m_nUpdateFlag = 1;
            }
            break;
        }
    }

    if (j >= m_updateVecLocalFileName.size()) {
        ReplaceFile(m_updateFilePath, m_updateStrWww, m_updateStrRequest, m_updateCacheFileName);
        m_nUpdateFlag = 1;
    }
}

void HotCodePushPlugin::ReplaceFile(const string &strFilePath, const string &strWww, const string &strRequest,
                                    const string &strFileName)
{
    string filePath = strFilePath + strWww + strFileName;
    FileCache::deleteFile(filePath);
    if (downLoadFile(strRequest + "/" + strFileName, filePath)) {
        m_nUpdateFlag = 1;
    }
}

void HotCodePushPlugin::CopyServerFilesToCache(const string &strCahceChcpJsonFilePath,
                                               const string &strChcpJsonFilePath,
                                               const string &strChcpCacheManifestFilePath,
                                               const string &strChcpManifestFilePath)
{
    FileCache::moveFile(strCahceChcpJsonFilePath, strChcpJsonFilePath);
    FileCache::moveFile(strChcpCacheManifestFilePath, strChcpManifestFilePath);
}

void HotCodePushPlugin::SendPluginMessage()
{
    if (m_nUpdateFlag == 0) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "action", "chcp_nothingToUpdate");
        cJSON_AddNumberToObject(json, "error", errorNothingToUpdate);
        cJSON_AddNullToObject(json, "data");
        m_call.resolve(json);
        cJSON_Delete(json);
    }

    if (m_nUpdateFlag == 1) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "action", "chcp_updateIsReadyToInstall");
        cJSON_AddNullToObject(json, "error");
        cJSON_AddNullToObject(json, "data");
        m_call.resolve(json);
        cJSON_Delete(json);
    }
}

void HotCodePushPlugin::CheckUpdatePlugin::Execute(void *arg)
{
    HotCodePushPlugin *pHotCodePushPlugin = (HotCodePushPlugin *)arg;
    pHotCodePushPlugin->FetchUpdate();
    pHotCodePushPlugin->SendPluginMessage();
}
