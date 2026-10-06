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

#include "CapConfig.h"
#include "Application.h"
#include "CordovaViewController.h"
#include "MemPool.h"
#include "cJSON.h"
#include <rawfile/raw_file.h>
const std::string CapConfig::LOG_BEHAVIOR_NONE = "none";
const std::string CapConfig::LOG_BEHAVIOR_DEBUG = "debug";
const std::string CapConfig::LOG_BEHAVIOR_PRODUCTION = "production";

CapConfig::CapConfig()
{
    std::string strConfigFileContent = ReadConfigFile("capacitor.config.json");
    if (strConfigFileContent.empty()) {
        return;
    }
    cJSON *pConfigJson = cJSON_Parse(strConfigFileContent.c_str());

    if (pConfigJson == nullptr) {
        return;
    }
    ParseHarmonyConfig(pConfigJson);
    ParsePluginsConfig(pConfigJson);
    cJSON_Delete(pConfigJson);
}

void CapConfig::ParsePluginsConfig(cJSON *pConfigJson)
{
    cJSON *pPlugins = cJSON_GetObjectItem(pConfigJson, "plugins");
    if (pPlugins == nullptr) {
        return;
    }
    cJSON *pItem = pPlugins->child;
    while (pItem != nullptr) {
        if (pItem->type == cJSON_Object && pItem->string != nullptr) {
            std::string strPluginId = pItem->string;
            PluginConfig *pPluginConfig = PluginConfig::createInstance(pItem);
            m_mapPluginsConfiguration[strPluginId] = pPluginConfig;
        }
        pItem = pItem->next;
    }
}

void CapConfig::ParseHarmonyConfig(cJSON *pConfigJson)
{
    cJSON *pHarmony = cJSON_GetObjectItem(pConfigJson, "harmony");
    if (pHarmony == nullptr) {
        return;
    }
    cJSON *pLoggingEnabled = cJSON_GetObjectItem(pHarmony, "loggingBehavior");
    if (pLoggingEnabled != nullptr && pLoggingEnabled->type == cJSON_String &&
        LOG_BEHAVIOR_PRODUCTION == pLoggingEnabled->valuestring) {
        m_isLoggingEnabled = false;
    }

    cJSON *pHostName = cJSON_GetObjectItem(pHarmony, "Hostname");
    if (pHostName != nullptr && pHostName->type == cJSON_String) {
        m_strHostName = pHostName->valuestring;
    }

    cJSON *pCacheDuration = cJSON_GetObjectItem(pHarmony, "cordova-cache-duration");
    if (pCacheDuration != nullptr && pHostName->type == cJSON_Number) {
        long lngTmp = static_cast<long>(pCacheDuration->valuedouble);
        ((CordovaViewController *)(Application::g_cordovaViewController))->setCordovaCacheDuration(lngTmp);
    }

    cJSON *pProtocolUrl = cJSON_GetObjectItem(pHarmony, "cordova-protocol-force");
    if (pProtocolUrl != nullptr && pProtocolUrl->type == cJSON_Array) {
        int nCount = cJSON_GetArraySize(pProtocolUrl);
        std::vector<std::string> vecProtocol;
        for (int i = 0; i < nCount; i++) {
            cJSON *pStrUrl = cJSON_GetArrayItem(pProtocolUrl, i);
            if (pStrUrl != nullptr && pStrUrl->type == cJSON_String) {
                vecProtocol.push_back(pStrUrl->valuestring);
            }
        }
        ((CordovaViewController *)(Application::g_cordovaViewController))->setProtocolUrl(vecProtocol);
    }

    cJSON *pHarmonyNext = pHarmony->child;
    while (pHarmonyNext != nullptr) {
        if (pHarmonyNext->string != nullptr) {
            std::string strName = pHarmonyNext->string;
            if (pHarmonyNext->type == cJSON_String) {
                std::string strValue = pHarmonyNext->valuestring;
                m_mapPreferences[strName] = strValue;
            } else if (pHarmonyNext->type == cJSON_True) {
                m_mapPreferences[strName] = "true";
            } else if (pHarmonyNext->type == cJSON_False) {
                m_mapPreferences[strName] = "false";
            } else if (pHarmonyNext->type == cJSON_Number) {
                int nNumber = pHarmonyNext->valueint;
                m_mapPreferences[strName] = std::to_string(nNumber);
            }
        }
        pHarmonyNext = pHarmonyNext->next;
    }
}

std::string CapConfig::ReadConfigFile(const char *fileName)
{
    RawFile *rawfile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, fileName);
    if (!rawfile) {
        return "";
    }
    const long fileSize = OH_ResourceManager_GetRawFileSize(rawfile);
    std::vector<SMemPage> vecMemPage;
    const int blockSize = ((CMemPool *)Application::g_memPool)->GetPageSize() - 1;
    long consumed = 0;
    ((CMemPool *)Application::g_memPool)->MallocPage(vecMemPage, 1);
    if (vecMemPage.size() < 1) {
        return "";
    }

    std::string strConfigFileContent;
    char *buffer = vecMemPage[0].m_point;
    while (true) {
        OH_ResourceManager_SeekRawFile(rawfile, consumed, 0);
        int ret = OH_ResourceManager_ReadRawFile(rawfile, buffer, blockSize);
        if (ret == 0) {
            break;
        }
        buffer[ret] = 0;
        strConfigFileContent += buffer;
        if (consumed + ret >= fileSize) {
            break;
        }
        consumed += ret;
        std::fill_n(buffer, blockSize, 0);
    }
    ((CMemPool *)Application::g_memPool)->FreePage(vecMemPage);
    OH_ResourceManager_CloseRawFile(rawfile);
    return strConfigFileContent;
}

void CapConfig::setIsLoggingEnabled(const bool isLoggingEnabled) { m_isLoggingEnabled = isLoggingEnabled; }

bool CapConfig::getIsLoggingEnabled() const { return m_isLoggingEnabled; }

PluginConfig *CapConfig::getPluginConfiguration(const std::string &strPluginId)
{
    if (m_mapPluginsConfiguration.find(strPluginId) != m_mapPluginsConfiguration.end()) {
        return m_mapPluginsConfiguration[strPluginId];
    }
    return nullptr;
}

std::string CapConfig::getHostName() { return m_strHostName; }

std::map<std::string, std::string> &CapConfig::getCapacitorPreferences() { return m_mapPreferences; }