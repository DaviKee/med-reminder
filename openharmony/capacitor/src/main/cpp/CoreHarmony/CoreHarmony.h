/*
 * Copyright (c) 2025 Huawei Device, Inc. Ltd. and <马弓手>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORE_ANDROID_H
#define CORE_ANDROID_H

#include "CordovaPlugin.h"
#include "multimodalinput/oh_input_manager.h"

class CoreHarmony : public CordovaPlugin {
    std::mutex m_mutex;
    std::map<std::string, CallbackContext> m_mapWebTagToChanel;
    PluginResult m_pendingPause;
    PluginResult m_pendingResume;
    bool m_keepRunning{true};
    void sendEventMessage(PluginResult payload, const std::string& webTag);
public:
    CoreHarmony() {}
    ~CoreHarmony() {
    }
    void fireJavascriptEvent(std::string action);
    bool execute(const std::string&  action, cJSON* args, CallbackContext cbc) override;
    void sendEventMessage(const std::string& action, const std::string& webTag);
    void onDestroy(const std::string& strWebTag) override;
    bool onArKTsResult(cJSON* args);
};
#endif