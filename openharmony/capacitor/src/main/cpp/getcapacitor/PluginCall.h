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

#ifndef MyApplication_PLUGINCALL_H
#define MyApplication_PLUGINCALL_H

#include "cJSON.h"
#include "getcapacitor/PluginResult.h"
#include <string>
#include <exception>

class PluginCall {
    void *m_pMessageHandler{nullptr};
    std::string m_strPluginId;
    std::string m_strCallbackId;
    std::string m_strMethodName;
    cJSON *m_pJsonData{nullptr};

    bool m_isKeepAlive{false};
    bool m_isReleased{false};

public:
    static const std::string CALLBACK_ID_DANGLING;
    PluginCall() = default;
    ~PluginCall();
    PluginCall(void *pMessageHandler, const std::string &pluginId, const std::string &strMethodName,
               const std::string &strMethodData);
    PluginCall(void *pMessageHandler, const std::string &pluginId, const std::string &strCallbackId,
               const std::string &strMethodName, cJSON *pJsonData);
    PluginCall(const PluginCall &arg)
    {
        m_pMessageHandler = arg.m_pMessageHandler;
        m_strPluginId = arg.m_strPluginId;
        m_strCallbackId = arg.m_strCallbackId;
        m_strMethodName = arg.m_strMethodName;
        if (arg.m_pJsonData != nullptr) {
            m_pJsonData = cJSON_Duplicate(arg.m_pJsonData, 1);
        }
        m_isKeepAlive = arg.m_isKeepAlive;
        m_isReleased = arg.m_isReleased;
    }
    PluginCall &operator=(const PluginCall &arg)
    {
        if (this == &arg) {
            return *this;
        }
        m_pMessageHandler = arg.m_pMessageHandler;
        m_strPluginId = arg.m_strPluginId;
        m_strCallbackId = arg.m_strCallbackId;
        m_strMethodName = arg.m_strMethodName;
        if (m_pJsonData != nullptr) {
            cJSON_Delete(m_pJsonData);
        }
        if (arg.m_pJsonData != nullptr) {
            m_pJsonData = cJSON_Duplicate(arg.m_pJsonData, 1);
        }
        m_isKeepAlive = arg.m_isKeepAlive;
        m_isReleased = arg.m_isReleased;
        return *this;
    }
    bool operator==(const PluginCall &arg) const
    {
        if (m_strCallbackId == arg.m_strCallbackId && m_strPluginId == arg.m_strPluginId &&
            m_strMethodName == arg.m_strMethodName) {
            return true;
        }
        return false;
    }
    std::string getWebTag();
    void successCallback(const Capacitor::PluginResult &pPluginResult);
    void resolve(cJSON *pData);
    void resolve();
    void errorCallback(const std::string &strMsg) const;
    void reject(const std::string &strMsg, const std::string &strCode, const std::exception *ex, cJSON *pJsonData);
    void reject(const std::string &strMsg, const std::exception *ex, cJSON *pJsonData);
    void reject(const std::string &strMsg, const std::string &strCode, cJSON *pJsonData);
    void reject(const std::string &strMsg, const std::string &strCode, const std::exception *ex);
    void reject(const std::string &strMsg, cJSON *pJsonData);
    void reject(const std::string &strMsg, const std::exception *ex);
    void reject(const std::string &strMsg, const std::string &strCode);
    void reject(const std::string &strMsg);

    void unimplemented();
    void unimplemented(const std::string &strMsg);

    void unavailable();
    void unavailable(const std::string &strMsg);

    std::string getPluginId() const { return m_strPluginId; }

    std::string getCallbackId() const { return m_strCallbackId; }

    std::string getMethodName() const { return m_strMethodName; }
    cJSON *getData() const { return m_pJsonData; }

    std::string getString(const std::string &strName);
    std::string getString(const std::string &strName, const std::string &strDefaultValue);

    int getInt(const std::string &strName);
    int getInt(const std::string &strName, const int nDefaultValue);

    long getLong(const std::string &strName);
    long getLong(const std::string &strName, const long lngDefaultValue);

    float getFloat(const std::string &strName);
    float getFloat(const std::string &strName, const float fDefaultValue);

    double getDouble(const std::string &strName);
    double getDouble(const std::string &strName, const double dbDefaultValue);

    bool getBoolean(const std::string &strName);
    bool getBoolean(const std::string &strName, const bool bDefaultValue);

    cJSON *getObject(const std::string &strName);
    cJSON *getObject(const std::string &strName, cJSON *pJsonDefaultValue);

    cJSON *getArray(const std::string &strName);
    cJSON *getArray(const std::string &strName, cJSON *pJsonDefaultValue);

    bool hasOption(const std::string &strName);

    void save();

    void setKeepAlive(const bool isKeepAlive);

    void release();

    bool isSaved() const { return isKeepAlive(); }

    bool isKeepAlive() const { return m_isKeepAlive; }

    bool isReleased() const { return m_isReleased; }

    void setPluginId(const std::string &strPluginId) { m_strPluginId = strPluginId; }

    void setCallbackId(const std::string &strCallbackId) { m_strCallbackId = strCallbackId; }
};

#endif // MyApplication_PLUGINCALL_H
