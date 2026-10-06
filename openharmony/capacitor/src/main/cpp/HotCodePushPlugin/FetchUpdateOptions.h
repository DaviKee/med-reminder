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

#ifndef CAPACITOR_FETCHUPDATEOPTIONS_H
#define CAPACITOR_FETCHUPDATEOPTIONS_H

#include "cJSON.h"
#include <string>
#include <map>
using namespace std;

class FetchUpdateOptions {
    string m_strConfigUrl;
    map<string, string> m_requestHeaders;
    void ParseRequestHeaders(cJSON* pHeaders);
public:
    FetchUpdateOptions();
    void InitFetchUpdateOptions(cJSON* json);
    void InitFetchUpdateOptions(const string& strConfigUrl, const map<string, string>& requestHeaders);
    string getConfigUrl();
    map<string, string> getRequestHeaders();
    void SetConfigUrl(const string strConfigUrl);
    void SetRequestHeader(const map<string, string>& requestHeaders);
};

#endif //CAPACITOR_FETCHUPDATEOPTIONS_H
