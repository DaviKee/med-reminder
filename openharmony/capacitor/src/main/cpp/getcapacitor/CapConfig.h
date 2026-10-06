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

#ifndef MYAPPLICATION_CAPCONFIG_H
#define MYAPPLICATION_CAPCONFIG_H

#include <map>
#include "PluginConfig.h"

class CapConfig {
    static const std::string LOG_BEHAVIOR_NONE;
    static const std::string LOG_BEHAVIOR_DEBUG;
    static const std::string LOG_BEHAVIOR_PRODUCTION;
    std::string m_strHostName;
    bool m_isLoggingEnabled{true};
    std::map<std::string, PluginConfig *> m_mapPluginsConfiguration;
    std::map<std::string, std::string> m_mapPreferences;

public:
    CapConfig();
    ~CapConfig() = default;
    void setIsLoggingEnabled(const bool isLoggingEnabled);
    bool getIsLoggingEnabled() const;
    PluginConfig *getPluginConfiguration(const std::string &strPluginId);
    std::string getHostName();
    std::map<std::string, std::string> &getCapacitorPreferences();
    std::string ReadConfigFile(const char *fileName);
    void ParseHarmonyConfig(cJSON *pConfigJson);
    void ParsePluginsConfig(cJSON *pConfigJson);
};

#endif // MYAPPLICATION_CAPCONFIG_H
