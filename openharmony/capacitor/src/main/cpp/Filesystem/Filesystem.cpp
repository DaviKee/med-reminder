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

#include "Filesystem.h"
#include "CordovaViewController.h"
#include "cJSON.h"
#include "getcapacitor/PluginManager.h"
#include "FileCache.h"

REGISTER_CAP_PLUGIN(Filesystem, Filesystem)

std::string getErrorMsg(const std::string &content) {
    if (content == "stat")
        return "Get file/directory stat error";
    if (content == "getUri")
        return "Get file uri error";
    if (content == "writeFile")
        return "Write file error";
    if (content == "copy")
        return "Copy file error";
    if (content == "readFile")
        return "Read file error";
    if (content == "deleteFile")
        return "Delete file error";
    return "Unknown error";
}

bool isArrayResultValid(cJSON *resultJson) { return resultJson != NULL && resultJson->type == cJSON_Array; }

REGISTER_PLUGIN_METHOD(Filesystem, onArKTsResult, PluginMethod::RETURN_PROMISE)
void Filesystem::onArKTsResult(PluginCall &call) {
    cJSON* contentObj = cJSON_GetObjectItem(call.getData(), "content");
    if (contentObj == nullptr && contentObj->valuestring == nullptr) {
        return;
    }
    std::string content = contentObj->valuestring;
    cJSON *errorMsgObj = cJSON_GetObjectItem(call.getData(), "errorMsg");
    
    if (errorMsgObj != nullptr && errorMsgObj->valuestring != nullptr && strlen(errorMsgObj->valuestring) > 0) {
        (content == "readFileInChunks" ? chunk_call : m_call).reject(errorMsgObj->valuestring);
        return;
    }
    
    cJSON *resultJson = cJSON_GetObjectItem(call.getData(), "result");
    cJSON *json = cJSON_CreateObject();
    if (content == "appendFile" || content == "rename" || content == "mkdir" || content == "rmdir") {
        m_call.resolve();
    }
    
    if (content == "stat") {
        handleStatResult(resultJson);
    }
    
    if (content == "getUri" || content == "writeFile" || content == "copy" || content == "readFile") {
        handleFileOperationResult(content, resultJson, json);
    }
    
    if (content == "readdir") {
        handleReaddirResult(call);
    }
    
    if (content == "deleteFile") {
        handleDeleteFileResult(resultJson);
    }
    
    if (content == "readFileInChunks") {
        handleReadFileInChunksResult(resultJson, json);
    }
    
    cJSON_Delete(json);
}

void Filesystem::handleStatResult(cJSON *resultJson)
{
    if (isArrayResultValid(resultJson) && cJSON_GetArraySize(resultJson) > 0) {
        m_call.resolve(cJSON_GetArrayItem(resultJson, 0));
    } else {
        m_call.reject("Get file/directory stat error");
    }
}

void Filesystem::handleFileOperationResult(const std::string &content, cJSON *resultJson, cJSON *json)
{
    if (isArrayResultValid(resultJson) && cJSON_GetArraySize(resultJson) > 0) {
        const char *key = content == "readFile" ? "data" : "uri";
        cJSON_AddStringToObject(json, key, cJSON_GetArrayItem(resultJson, 0)->valuestring);
        m_call.resolve(json);
    } else {
        m_call.reject(getErrorMsg(content));
    }
}

void Filesystem::handleReaddirResult(PluginCall &call)
{
    cJSON *readFiles = cJSON_GetObjectItem(call.getData(), "readFiles");
    if (readFiles != nullptr) {
        m_call.resolve(readFiles);
    } else {
        m_call.reject("Read directory error");
    }
}

void Filesystem::handleDeleteFileResult(cJSON *resultJson)
{
    if (isArrayResultValid(resultJson) && cJSON_GetArraySize(resultJson) == 0) {
        m_call.resolve();
    } else {
        m_call.reject("Delete file error");
    }
}

void Filesystem::handleReadFileInChunksResult(cJSON *resultJson, cJSON *json)
{
    if (isArrayResultValid(resultJson) && cJSON_GetArraySize(resultJson) > 0) {
        cJSON* dataItem = cJSON_GetArrayItem(resultJson, 0);
        if (dataItem != nullptr && dataItem->valuestring != nullptr) {
            std::string data = dataItem->valuestring;
            cJSON_AddStringToObject(json, "data", data.c_str());
            chunk_call.setKeepAlive(!data.empty());
            chunk_call.resolve(json);
        }
    } else {
        chunk_call.reject("Read file in chunks error");
    }
}

REGISTER_PLUGIN_METHOD(Filesystem, mkdir, PluginMethod::RETURN_PROMISE)
void Filesystem::mkdir(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHMkdir", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, rmdir, PluginMethod::RETURN_PROMISE)
void Filesystem::rmdir(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHRmdir", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, rename, PluginMethod::RETURN_PROMISE)
void Filesystem::rename(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHRename", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, writeFile, PluginMethod::RETURN_PROMISE)
void Filesystem::writeFile(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHWriteFile", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, appendFile, PluginMethod::RETURN_PROMISE)
void Filesystem::appendFile(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHWriteFile", 1, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, deleteFile, PluginMethod::RETURN_PROMISE)
void Filesystem::deleteFile(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHDeleteFile", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, copy, PluginMethod::RETURN_PROMISE)
void Filesystem::copy(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHCopyFile", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, readFile, PluginMethod::RETURN_PROMISE)
void Filesystem::readFile(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHReadFile", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, readdir, PluginMethod::RETURN_PROMISE)
void Filesystem::readdir(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHReaddir", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, readFileInChunks, PluginMethod::RETURN_CALLBACK)
void Filesystem::readFileInChunks(PluginCall &call) {
    chunk_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHReadFileInChunks", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, stat, PluginMethod::RETURN_PROMISE)
void Filesystem::stat(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHStat", 0, pStr, "Filesystem", call);
}

REGISTER_PLUGIN_METHOD(Filesystem, getUri, PluginMethod::RETURN_PROMISE)
void Filesystem::getUri(PluginCall &call) {
    m_call = call;
    char *pStr = cJSON_Print(call.getData());
    executeArkTs("./Filesystem/FilesystemAction/OHGetUri", 0, pStr, "Filesystem", call);
}

void Filesystem::checkPermissions(PluginCall &call)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "publicStorage", "granted");
    call.resolve(json);
    cJSON_Delete(json);
}

void Filesystem::requestPermissions(PluginCall &call)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "publicStorage", "granted");
    call.resolve(json);
    cJSON_Delete(json);
}