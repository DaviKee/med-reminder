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

#include "PluginConfig.h"

PluginConfig::PluginConfig(cJSON *pJson)
{
    if (pJson != nullptr) {
        m_config = cJSON_Duplicate(pJson, 1);
    }
}

PluginConfig *PluginConfig::createInstance(cJSON *pJson) { return new PluginConfig(pJson); }

std::string PluginConfig::getString(const std::string &strConfKey) { return getString(strConfKey, ""); }

std::string PluginConfig::getString(const std::string &strConfKey, const std::string &strDefaultValue)
{
    cJSON *pJson = cJSON_GetObjectItem(m_config, strConfKey.c_str());
    if (pJson != nullptr && pJson->type == cJSON_String) {
        return pJson->valuestring;
    }
    return strDefaultValue;
}

bool PluginConfig::getBoolean(const std::string &strConfKey, const bool bDefaultValue)
{
    cJSON *pJson = cJSON_GetObjectItem(m_config, strConfKey.c_str());
    if (pJson != nullptr) {
        if (pJson->type == cJSON_True) {
            return true;
        }
        if (pJson->type == cJSON_False) {
            return false;
        }
    }
    return bDefaultValue;
}

int PluginConfig::getInt(const std::string &strConfKey, const int nDefaultValue)
{
    cJSON *pJson = cJSON_GetObjectItem(m_config, strConfKey.c_str());
    if (pJson != nullptr && pJson->type == cJSON_Number) {
        return pJson->valueint;
    }
    return nDefaultValue;
}

std::vector<std::string> PluginConfig::getArray(const std::string &strConfKey)
{
    std::vector<std::string> vecRet;
    return getArray(strConfKey, vecRet);
}

std::vector<std::string> PluginConfig::getArray(const std::string &strConfKey,
                                                const std::vector<std::string> &vecDefaultValue)
{
    std::vector<std::string> vecRet;
    cJSON *pJson = cJSON_GetObjectItem(m_config, strConfKey.c_str());
    if (pJson != nullptr && pJson->type == cJSON_Array) {
        int nCount = cJSON_GetArraySize(pJson);
        for (int i = 0; i < nCount; i++) {
            cJSON *pItem = cJSON_GetArrayItem(pJson, i);
            if (pItem != nullptr && pItem->type == cJSON_String) {
                vecRet.push_back(pItem->valuestring);
            }
        }
        return vecRet;
    }
    return vecDefaultValue;
}

cJSON *PluginConfig::getObject(const std::string &strConfKey)
{
    cJSON *pJson = cJSON_GetObjectItem(m_config, strConfKey.c_str());
    return pJson;
}

bool PluginConfig::isEmpty() const { return m_config == nullptr; }

cJSON *PluginConfig::getConfigJSON() { return m_config; }