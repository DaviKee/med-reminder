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

#ifndef MyApplication_PLUGINRESULT_H
#define MyApplication_PLUGINRESULT_H
#include <string>
#include "cJSON.h"
namespace Capacitor {

class PluginResult {
    cJSON *m_pJson{nullptr};
    PluginResult(const PluginResult &);
    PluginResult &operator=(const PluginResult &);

public:
    PluginResult() { m_pJson = cJSON_CreateObject(); }
    explicit PluginResult(cJSON *pJson) { m_pJson = cJSON_Duplicate(pJson, 1); }
    ~PluginResult() { cJSON_Delete(m_pJson); }
    cJSON *getJSON() const { return m_pJson; }

    PluginResult *put(const std::string &strName, const bool bValue)
    {
        cJSON_AddBoolToObject(m_pJson, strName.c_str(), bValue);
        return this;
    }

    PluginResult *put(const std::string &strName, const double dbValue)
    {
        cJSON_AddNumberToObject(m_pJson, strName.c_str(), dbValue);
        return this;
    }

    PluginResult *put(const std::string &strName, const int nValue)
    {
        cJSON_AddNumberToObject(m_pJson, strName.c_str(), nValue);
        return this;
    }

    PluginResult *put(const std::string &strName, const long lngValue)
    {
        cJSON_AddNumberToObject(m_pJson, strName.c_str(), lngValue);
        return this;
    }

    PluginResult *put(const std::string &strName, const std::string &strValue)
    {
        cJSON_AddStringToObject(m_pJson, strName.c_str(), strValue.c_str());
        return this;
    }

    PluginResult *put(const std::string &strName, cJSON *pJson)
    {
        cJSON *pDupJson = cJSON_Duplicate(pJson, 1);
        cJSON_AddItemToObject(m_pJson, strName.c_str(), pDupJson);
        return this;
    }

    PluginResult *put(const std::string &strName, const PluginResult *pPluginResult)
    {
        cJSON *pDupJson = cJSON_Duplicate(pPluginResult->getJSON(), 1);
        cJSON_AddItemToObject(m_pJson, strName.c_str(), pDupJson);
        return this;
    }

    PluginResult *jsonPut(const std::string &strName, cJSON *pJson)
    {
        cJSON *pDupJson = cJSON_Duplicate(pJson, 1);
        cJSON_AddItemToObject(m_pJson, strName.c_str(), pDupJson);
        return this;
    }

    std::string toString() const
    {
        char *pStr = cJSON_Print(m_pJson);
        std::string strRet = pStr;
        free(pStr);
        return strRet;
    }

    cJSON *getWrappedResult()
    {
        std::string strPluginId;
        cJSON *pPluginId = cJSON_GetObjectItem(m_pJson, "pluginId");
        if (pPluginId != nullptr && pPluginId->type == cJSON_String) {
            strPluginId = pPluginId->valuestring;
        }

        std::string strMethodName;
        cJSON *pMethodName = cJSON_GetObjectItem(m_pJson, "methodName");
        if (pMethodName != nullptr && pMethodName->type == cJSON_String) {
            strMethodName = pMethodName->valuestring;
        }

        bool isSuccess = false;
        cJSON *pSuccess = cJSON_GetObjectItem(m_pJson, "success");
        if (pSuccess != nullptr && pSuccess->type == cJSON_True) {
            isSuccess = true;
        }

        cJSON *pData = cJSON_GetObjectItem(m_pJson, "data");
        cJSON *pError = cJSON_GetObjectItem(m_pJson, "error");

        cJSON *pJson = cJSON_CreateObject();
        cJSON_AddStringToObject(pJson, "pluginId", strPluginId.c_str());
        cJSON_AddStringToObject(pJson, "methodName", strMethodName.c_str());
        cJSON_AddBoolToObject(pJson, "success", isSuccess);
        cJSON_AddItemToObject(pJson, "data", pData);
        cJSON_AddItemToObject(pJson, "error", pError);
        return pJson;
    }
};

} // namespace Capacitor
#endif // MyApplication_PLUGINRESULT_H
