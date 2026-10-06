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

#ifndef CAPACITOR_PLUGIN_FILESYSTEM_H
#define CAPACITOR_PLUGIN_FILESYSTEM_H

#include "getcapacitor/Plugin.h"

class Filesystem : public Plugin {
    PluginCall m_call;
    PluginCall chunk_call;
public:
    Filesystem() {
    }
    ~Filesystem() {
    };
    void onArKTsResult(PluginCall& call);
    void handleReadFileInChunksResult(cJSON *resultJson, cJSON *json);
    void handleDeleteFileResult(cJSON *resultJson);
    void handleReaddirResult(PluginCall &call);
    void handleFileOperationResult(const std::string &content, cJSON *resultJson, cJSON *json);
    void handleStatResult(cJSON *resultJson);
    void mkdir(PluginCall& call);
    void rmdir(PluginCall& call);
    void rename(PluginCall& call);
    void readFile(PluginCall& call);
    void readdir(PluginCall& call);
    void readFileInChunks(PluginCall& call);
    void writeFile(PluginCall& call);
    void appendFile(PluginCall& call);
    void deleteFile(PluginCall& call);
    void copy(PluginCall& call);
    void stat(PluginCall& call);
    void getUri(PluginCall& call);
    void checkPermissions(PluginCall& call) override;
    void requestPermissions(PluginCall& call) override;
};

#endif //CAPACITOR_PLUGIN_FILESYSTEM_H