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

#ifndef MyApplication_PLUGINCONFIG_H
#define MyApplication_PLUGINCONFIG_H
#include "cJSON.h"
#include <string>

class PluginConfig {
    cJSON *m_config{nullptr};
    PluginConfig() = default;
    explicit PluginConfig(cJSON *pJson);
    PluginConfig(const PluginConfig &);
    PluginConfig &operator=(const PluginConfig &);
    ~PluginConfig() = default;

public:
    static PluginConfig *createInstance(cJSON *pJson);
    std::string getString(const std::string &strConfKey);
    std::string getString(const std::string &strConfKey, const std::string &strDefaultValue);
    bool getBoolean(const std::string &strConfKey, const bool bDefaultValue);
    int getInt(const std::string &strConfKey, const int nDefaultValue);
    std::vector<std::string> getArray(const std::string &strConfKey);
    std::vector<std::string> getArray(const std::string &strConfKey, const std::vector<std::string> &vecDefaultValue);
    cJSON *getObject(const std::string &strConfKey);
    bool isEmpty() const;
    cJSON *getConfigJSON();
};

#endif // MyApplication_PLUGINCONFIG_H
