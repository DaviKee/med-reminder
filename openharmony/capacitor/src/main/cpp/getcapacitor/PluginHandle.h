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

#ifndef MyApplication_PLUGINHANDLE_H
#define MyApplication_PLUGINHANDLE_H
#include "Plugin.h"

class PluginHandle {
    std::string m_strClassName;
    Plugin *m_pPlugin{nullptr};
    PluginHandle(const PluginHandle &);
    PluginHandle &operator=(const PluginHandle &);

public:
    PluginHandle(const std::string &strClassName, Plugin *plugin);
    ~PluginHandle() = default;
    Plugin *getInstance();
    void invoke(const std::string &strMethodName, PluginCall &call);
    void addListener(PluginCall &call) const;
    void removeListener(PluginCall &call) const;
    void removeAllListeners(PluginCall &call) const;
    void checkPermissions(PluginCall &call) const;
    void requestPermissions(PluginCall &call) const;
    std::string getClassName() const { return m_strClassName; }
};

#endif // MyApplication_PLUGINHANDLE_H
