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

#ifndef MyApplication_PLUGINMANAGER_H
#define MyApplication_PLUGINMANAGER_H
#include "PluginHandle.h"
#include "getcapacitor/CapConfig.h"
#include <string>
#include <map>

namespace Capacitor {
class PluginManager {
    static const unsigned int LOG_PRINT_DOMAIN;
    std::map<std::string, PluginHandle *> m_mapPlugins;
    std::map<std::string, std::map<std::string, std::map<std::string, std::string> > >
        m_mapWebTagToPluginToMethodToReType;
    PluginManager(const PluginManager &);
    PluginManager &operator=(const PluginManager &);
    CapConfig *m_pCapConfig{nullptr};

public:
    PluginManager();
    ~PluginManager() = default;
    std::string getPluginId(const std::string &strCalssName);
    std::string getPluginName(const std::string &strClassName);
    void registerPlugin(const std::string &strClassName);
    PluginHandle *getPluginHandle(const std::string &strWebTag, const std::string &strPluginId);
    std::map<std::string, PluginHandle *> &getMapPlugins();
    void registerAllPlugins();
    CapConfig *getCapConfig();
    void setTsPluginInfo(const std::string &strWebTag, const std::string &strJson);
    bool getTsPluginInfo(const std::string &strWebTag,
                         std::map<std::string, std::map<std::string, std::string> > &mapPluginToMethod);
    std::string readConfigContent();
    void registerPluginFromJson(cJSON *pPlugin);
    void onStart(const std::string &strWebTag);
    void onPageStart(const std::string &strWebTag);
    void onEnd(const std::string &strWebTag);
    void onResume(const std::string &strWebTag);
    void onPause(const std::string &strWebTag);
    void onDestroy(const std::string &strWebTag);
};
} // namespace Capacitor
#endif // MyApplication_PLUGINMANAGER_H
