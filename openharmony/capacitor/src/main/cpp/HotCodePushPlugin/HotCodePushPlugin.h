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

#ifndef CAPACITOR_HOTCODEPUSHPLUGIN_H
#define CAPACITOR_HOTCODEPUSHPLUGIN_H

#include "getcapacitor/Plugin.h"
#include "./Thread.h"
#include "HotCodePushPlugin/FetchUpdateOptions.h"
#include "ConnPool.h"
#include <map>
#include <atomic>
#include <string>
#include <vector>
#include <openssl/ssl.h>

class HotCodePushPlugin : public Plugin {
    static const int constMaxBuf = 4 * 1024 * 1024;
    static const int errorNothingToUpdate = 2;
    void *m_pBuf;
    PluginCall m_call;
    string m_strChcpConfigUrl;
    bool m_isAllowUpdatesAutoDownload;
    bool m_isAllowUpdatesAutoInstall;
    int m_nActiveInterfaceVersion;
    FetchUpdateOptions m_fetchUpdateOptions;
    cJSON *m_pChcpFile;
    cJSON *m_pCacheChcpFile;
    cJSON *m_pChcpManifestFile;
    cJSON *m_pChcpChacheManifestFile;
    class CheckUpdatePlugin : public CThread {
        string m_strBundleName;
        string m_strAppId;

    public:
        void Execute(void *);
    };
    CheckUpdatePlugin m_checkUpdatePlugin;
    int m_nUpdateFlag;

    // Member variables for ReceiveResponse methods
    ConnPool *m_currentConnPool;
    SSL *m_currentSsl;
    int m_currentSocket;
    string m_currentMethod;
    string *m_currentFileName;
    map<string, string> *m_currentHeaders;
    bool *m_currentIsConnectionClosed;
    std::atomic<bool> *m_currentIsAbort;
    bool m_isUsingSsl;

    // Member variables for UpdateFileIfNeeded method
    string m_updateFilePath;
    string m_updateStrWww;
    string m_updateStrRequest;
    string m_updateCacheFileName;
    string m_updateCacheFileHash;
    vector<string> m_updateVecLocalFileName;
    vector<string> m_updateVecLocalFileHash;

    struct URLInfo {
        string domain;
        string requestPath;
        bool isHttps;
    };
    
    bool ParseUrl(const string &url, URLInfo &urlInfo);
    string BuildHttpRequest(const URLInfo &urlInfo);
    ConnPool* GetConnectionPool(const string &domain);
    bool AttemptDownload(ConnPool *pool, const string &request, const string &targetPath, bool isHttps);
    bool DownloadHttps(ConnPool *pool, const string &request, string &fileName,
                        map<string, string> &responseHeaders, std::atomic<bool> &isAbort);
    bool DownloadHttp(ConnPool *pool, const string &request, string &fileName,
                       map<string, string> &responseHeaders, std::atomic<bool> &isAbort);
    bool SendRequest(ConnPool *pool, SSL *ssl, const string &request, std::atomic<bool> &isAbort);
    bool SendRequest(ConnPool *pool, int socket, const string &request, std::atomic<bool> &isAbort);
    bool ReceiveResponse();

    string GetInjectDirectoryPath();
    string GetCacheDirectoryPath();
    bool ProcessLocalChcpJsonFile(const string &strFilePath, const string &strChcpFile,
                                 const string &strChcpJsonFilePath, const string &strWww);
    bool ProcessLocalChcpManifestFile(const string &strFilePath, const string &strChcpManifest,
                                    const string &strChcpManifestFilePath, const string &strWww);
    bool CopyFileFromResources(const string &resourcePath, const string &targetPath);
    bool LoadLocalChcpJsonFile(const string &strChcpJsonFilePath);
    bool LoadServerChcpJsonFile(const string &strCahceChcpJsonFilePath);
    bool CheckForUpdates(string &strRequest);
    bool LoadLocalChcpManifestFile(const string &strChcpManifestFilePath);
    bool LoadServerChcpManifestFile(const string &strRequest, const string &strChcpManifest,
                                  const string &strChcpCacheManifestFilePath);
    void UpdateFiles(const string &strFilePath, const string &strWww, const string &strRequest);
    void ParseManifestFile(cJSON *pManifestFile, vector<string> &vecFileNames, vector<string> &vecFileHashes);
    void UpdateFileIfNeeded();
    void ReplaceFile(const string &strFilePath, const string &strWww, const string &strRequest,
                   const string &strFileName);
    void CopyServerFilesToCache(const string &strCahceChcpJsonFilePath, const string &strChcpJsonFilePath,
                              const string &strChcpCacheManifestFilePath, const string &strChcpManifestFilePath);

    // Action handler helper methods
    void HandleJsInitPlugin(PluginCall &call);
    void HandleJsFetchUpdate(PluginCall &call, cJSON *args);
    void HandleJsInstallUpdateActions(PluginCall &call);
    void HandleJsConfigure(PluginCall &call);
    void HandleJsGetVersionInfo(PluginCall &call);

public:
    HotCodePushPlugin()
    {
        m_nUpdateFlag = 0;
    }
    ~HotCodePushPlugin(){};

    void fetchUpdate(PluginCall &call);
    void installUpdate(PluginCall &call);
    void execute(PluginCall &call);
    void PluginInitialize();
    void ParseCordovaConfigXml();
    bool downLoadFile(const string &strSource, const string &strTarget);
    void FetchUpdate();
    void SendPluginMessage();
};

#endif // CAPACITOR_HOTCODEPUSHPLUGIN_H