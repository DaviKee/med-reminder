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

#ifndef CAPACITOR_PLUGIN_LOCALNOTIFICATIONS_H
#define CAPACITOR_PLUGIN_LOCALNOTIFICATIONS_H

#include "getcapacitor/Plugin.h"

class LocalNotifications : public Plugin {
    PluginCall m_call;
public:
    LocalNotifications() {}
    ~LocalNotifications() {
    };
    void addListener(PluginCall& call) override;
    void removeAllListeners(PluginCall& call)override;
    void handleOnStart(const std::string& strWebTag)override;
    void schedule(PluginCall& call);
    void getPending(PluginCall& call);
    void cancel(PluginCall& call);
    void areEnabled(PluginCall& call);
    void getDeliveredNotifications(PluginCall& call);
    void removeDeliveredNotifications(PluginCall& call);
    void removeAllDeliveredNotifications(PluginCall& call);
    void registerActionTypes(PluginCall& call);
    void createChannel(PluginCall& call);
    void deleteChannel(PluginCall& call);
    void listChannels(PluginCall& call);
    void changeExactNotificationSetting(PluginCall& call);
    void checkExactNotificationSetting(PluginCall& call);
    void checkPermissions(PluginCall& call);
    void requestPermissions(PluginCall& call);
    void onArKTsResult(PluginCall& call);
};

#endif //CAPACITOR_PLUGIN_LOCALNOTIFICATIONS_H
