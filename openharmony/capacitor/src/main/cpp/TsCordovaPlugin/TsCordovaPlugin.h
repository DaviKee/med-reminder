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

#ifndef MYAPPLICATION_TSCORDOVAPLUGIN_H
#define MYAPPLICATION_TSCORDOVAPLUGIN_H

#include "CordovaPlugin.h"
#include "Lock.h"

class TsCordovaPlugin : public CordovaPlugin{
    static const unsigned int LOG_PRINT_DOMAIN;
    CLock m_cLock;
    std::map<std::string, CallbackContext> m_mapCbc;
public:
    TsCordovaPlugin() {}
    ~TsCordovaPlugin() {
    }
    bool execute(const std::string& serviceName, const std::string& action, cJSON* args, CallbackContext cbc) override;
    void onMessage(const std::string &id, const std::string& strData) override;
    void onPause(bool multitasking) override;
    void onResume(bool multitasking) override;
    void onStart(const std::string& strWebTag) override;
    void onPageStart(const std::string& strWebTag) override;
    void onPageEnd(const std::string& strWebTag) override;
    // onDestroy 在ArkTS侧调用，不用C++侧触发
    void setCallBackContext(CallbackContext cbc);
    CallbackContext getCallBackContext(const std::string& strCallbackId, const bool isKeepCallBack);
    bool onArKTsResult(cJSON* args);
    
};

#endif //MYAPPLICATION_TSCORDOVAPLUGIN_H
