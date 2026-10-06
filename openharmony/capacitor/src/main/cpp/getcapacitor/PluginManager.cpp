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

#include "getcapacitor/PluginManager.h"
#include "MemPool.h"
#include "cJSON.h"
#include "hilog/log.h"
#include "TsCapacitorPlugin.h"

const unsigned int Capacitor::PluginManager::LOG_PRINT_DOMAIN = 0xFF00;

Capacitor::PluginManager::PluginManager()
{
    m_pCapConfig = new CapConfig();
    registerAllPlugins();
}

std::string Capacitor::PluginManager::getPluginId(const std::string &strCalssName)
{
    return CCapacitorPluginFactory::getPluginName(strCalssName);
}

std::string Capacitor::PluginManager::getPluginName(const std::string &strCalssName)
{
    return CCapacitorPluginFactory::getPluginName(strCalssName);
}

void Capacitor::PluginManager::registerPlugin(const std::string &strCalssName)
{
    std::string strPluginId = getPluginId(strCalssName);
    if (strPluginId.empty()) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "PluginManager",
            "NativePlugin:%{public}s is invalid. Ensure the REGISTER_CAP_PLUGIN exists on the plugin class and",
            strCalssName.c_str());
        return;
    }

    Plugin *plugin = (Plugin *)CCapacitorPluginFactory::createObject(strCalssName);
    PluginHandle *pPluginHandle = new PluginHandle(strCalssName, plugin);
    m_mapPlugins[strPluginId] = pPluginHandle;
}

PluginHandle *Capacitor::PluginManager::getPluginHandle(const std::string &strWebTag, const std::string &strPluginId)
{
    if (m_mapWebTagToPluginToMethodToReType.find(strWebTag) != m_mapWebTagToPluginToMethodToReType.end()) {
        std::map<std::string, std::map<std::string, std::string>> &mapPluginId =
            m_mapWebTagToPluginToMethodToReType[strWebTag];
        if (mapPluginId.find(strPluginId) != mapPluginId.end()) {
            return m_mapPlugins["TsCapacitorPlugin"];
        }
    }

    if (m_mapPlugins.find(strPluginId) != m_mapPlugins.end()) {
        return m_mapPlugins[strPluginId];
    }
    return nullptr;
}

std::string Capacitor::PluginManager::readConfigContent()
{
    RawFile *rawfile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, "capacitor.plugins.json");
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

void Capacitor::PluginManager::registerPluginFromJson(cJSON *pPlugin)
{
    cJSON *pPluginClass = cJSON_GetObjectItem(pPlugin, "classpath");
    if (pPluginClass != nullptr && pPluginClass->type == cJSON_String) {
        std::string strClassName = pPluginClass->valuestring;
        if (strClassName.find_last_of(".") != std::string::npos) {
            strClassName = strClassName.substr(strClassName.find_last_of(".") + 1);
        }
        Plugin *pPlugin = (Plugin *)CCapacitorPluginFactory::createObject(strClassName);
        std::string strPluginName = CCapacitorPluginFactory::getPluginName(strClassName);
        if (pPlugin != nullptr && !strPluginName.empty()) {
            PluginHandle *pPluginHandle = new PluginHandle(strClassName, pPlugin);
            m_mapPlugins[strPluginName] = pPluginHandle;
        }
    }
}

void Capacitor::PluginManager::registerAllPlugins()
{
    std::string strConfigFileContent = readConfigContent();
    if (strConfigFileContent.empty()) {
        return;
    }

    cJSON *pPluginJson = cJSON_Parse(strConfigFileContent.c_str());
    if (pPluginJson != nullptr && pPluginJson->type == cJSON_Array) {
        int nCount = cJSON_GetArraySize(pPluginJson);
        for (int i = 0; i < nCount; i++) {
            registerPluginFromJson(cJSON_GetArrayItem(pPluginJson, i));
        }
    }

    Plugin *plugin = new TsCapacitorPlugin();
    PluginHandle *pluginHandle = new PluginHandle("TsCapacitorPlugin", plugin);
    m_mapPlugins["TsCapacitorPlugin"] = pluginHandle;
}

std::map<std::string, PluginHandle *> &Capacitor::PluginManager::getMapPlugins() { return m_mapPlugins; }

CapConfig *Capacitor::PluginManager::getCapConfig() { return m_pCapConfig; }

void Capacitor::PluginManager::setTsPluginInfo(const std::string &strWebTag, const std::string &strJson)
{
    cJSON *pArrayPlugin = cJSON_Parse(strJson.c_str());
    if (pArrayPlugin == nullptr || pArrayPlugin->type != cJSON_Array) {
        cJSON_Delete(pArrayPlugin);
        return;
    }

    std::map<std::string, std::map<std::string, std::string>> mapPlugin;
    int nCount = cJSON_GetArraySize(pArrayPlugin);

    for (int i = 0; i < nCount; i++) {
        cJSON *pPluginJson = cJSON_GetArrayItem(pArrayPlugin, i);
        // 合并校验：如果节点为空或没有子节点，直接跳过
        if (pPluginJson == nullptr || pPluginJson->child == nullptr)
            continue;

        std::string strPluginName = pPluginJson->child->string;
        cJSON *pArrayMethod = pPluginJson->child;

        // 校验方法数组类型
        if (pArrayMethod == nullptr || pArrayMethod->type != cJSON_Array) {
            mapPlugin[strPluginName] = {}; // 保持原逻辑：即使没方法也记录插件名
            continue;
        }

        std::map<std::string, std::string> mapMethod;
        int nMethodCount = cJSON_GetArraySize(pArrayMethod);
        for (int k = 0; k < nMethodCount; k++) {
            cJSON *pMethodJson = cJSON_GetArrayItem(pArrayMethod, k);
            // 使用卫语句平铺：减少 if 嵌套层级
            if (pMethodJson == nullptr || pMethodJson->child == nullptr)
                continue;
            if (pMethodJson->child->string == nullptr || pMethodJson->child->valuestring == nullptr)
                continue;

            mapMethod[pMethodJson->child->string] = pMethodJson->child->valuestring;
        }
        mapPlugin[strPluginName] = mapMethod;
    }

    m_mapWebTagToPluginToMethodToReType[strWebTag] = mapPlugin;
    cJSON_Delete(pArrayPlugin); // 统一释放内存
}

bool Capacitor::PluginManager::getTsPluginInfo(
    const std::string &strWebTag, std::map<std::string, std::map<std::string, std::string>> &mapPluginToMethod)
{
    if (m_mapWebTagToPluginToMethodToReType.find(strWebTag) != m_mapWebTagToPluginToMethodToReType.end()) {
        mapPluginToMethod = m_mapWebTagToPluginToMethodToReType[strWebTag];
        return true;
    }
    return false;
}

void Capacitor::PluginManager::onStart(const std::string &strWebTag)
{
    for (auto it = m_mapPlugins.begin(); it != m_mapPlugins.end(); it++) {
        it->second->getInstance()->handleOnStart(strWebTag);
    }
}

void Capacitor::PluginManager::onPageStart(const std::string &strWebTag)
{
    for (auto it = m_mapPlugins.begin(); it != m_mapPlugins.end(); it++) {
        it->second->getInstance()->handleOnPageStart(strWebTag);
    }
}

void Capacitor::PluginManager::onEnd(const std::string &strWebTag)
{
    for (auto it = m_mapPlugins.begin(); it != m_mapPlugins.end(); it++) {
        it->second->getInstance()->handleOnEnd(strWebTag);
    }
}

void Capacitor::PluginManager::onResume(const std::string &strWebTag)
{
    for (auto it = m_mapPlugins.begin(); it != m_mapPlugins.end(); it++) {
        it->second->getInstance()->handleOnResume(strWebTag);
    }
}

void Capacitor::PluginManager::onPause(const std::string &strWebTag)
{
    for (auto it = m_mapPlugins.begin(); it != m_mapPlugins.end(); it++) {
        it->second->getInstance()->handleOnPause(strWebTag);
    }
}

void Capacitor::PluginManager::onDestroy(const std::string &strWebTag)
{
    for (auto it = m_mapPlugins.begin(); it != m_mapPlugins.end(); it++) {
        it->second->getInstance()->handleOnDestroy(strWebTag);
    }
}