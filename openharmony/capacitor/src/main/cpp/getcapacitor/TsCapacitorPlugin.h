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

#ifndef MyApplication_TSCAPACITORPLUGIN_H
#define MyApplication_TSCAPACITORPLUGIN_H
#include "Plugin.h"

class TsCapacitorPlugin : public Plugin {
public:
    TsCapacitorPlugin() = default;
    ~TsCapacitorPlugin() = default;
    void callMethod(PluginCall &call);

    void onArKTsResult(PluginCall &call);
    void addListener(PluginCall &call) override;
    void removeListener(PluginCall &call) override;
    void removeAllListeners(PluginCall &call) override;
    void checkPermissions(PluginCall &call) override;
    void requestPermissions(PluginCall &call) override;

    void handleOnStart(const std::string &strWebTag) override;
    void handleOnPageStart(const std::string &strWebTag) override;
    void handleOnEnd(const std::string &strWebTag) override;
    void handleOnResume(const std::string &strWebTag) override;
    void handleOnPause(const std::string &strWebTag) override;
};

#endif // MyApplication_TSCAPACITORPLUGIN_H
