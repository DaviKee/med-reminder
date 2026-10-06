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

#ifndef CAPACITORPLUGIN_APP_H
#define CAPACITORPLUGIN_APP_H

#include "getcapacitor/Plugin.h"

class App : public Plugin {
    PluginCall m_call;
    std::string urlStr;
    bool appStateChanged = false;
    bool appUrlOpen = false;
    bool backButton = false;
public:
    App() {}
    ~App() {
    };
    
    void exitApp(PluginCall& call);
    void getInfo(PluginCall& call);
    void getState(PluginCall& call);
    void getLaunchUrl(PluginCall& call);
    void minimizeApp(PluginCall& call);
    void toggleBackButtonHandler(PluginCall& call);
    void addListener(PluginCall& call) override;
    void removeListener(PluginCall& call)override;
    void removeAllListeners(PluginCall& call)override;
    void handleOnStart(const std::string& strWebTag)override;
    void handleOnEnd(const std::string& strWebTag)override;
    void handleOnResume(const std::string& strWebTag)override;
    void handleOnPause(const std::string& strWebTag)override;
    void handleOnDestroy(const std::string& strWebTag)override;
    void onArKTsResult(PluginCall& call);
};

#endif //CAPACITORPLUGIN_APP_H
