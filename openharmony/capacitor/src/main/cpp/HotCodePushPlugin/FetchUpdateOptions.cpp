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

#include "FetchUpdateOptions.h"

FetchUpdateOptions::FetchUpdateOptions()
{
}
void FetchUpdateOptions::InitFetchUpdateOptions(cJSON* json)
{
    if (json == NULL) {
        return;
    }
    if (cJSON_GetObjectItem(json, "config-file")) {
        m_strConfigUrl = cJSON_GetObjectItem(json, "config-file")->valuestring;
    }
    
    cJSON* pHeaders = cJSON_GetObjectItem(json, "request-headers");
    ParseRequestHeaders(pHeaders);
}

void FetchUpdateOptions::ParseRequestHeaders(cJSON* pHeaders)
{
    if (!pHeaders || pHeaders->type != cJSON_Object) {
        return;
    }
    
    pHeaders = pHeaders->child;
    while (pHeaders != NULL) {
        string strName = pHeaders->string;
        if (pHeaders->type == cJSON_String) {
            string strValue = pHeaders->valuestring;
            m_requestHeaders[strName] = strValue;
        }
        pHeaders = pHeaders->next;
    }
}
void FetchUpdateOptions::InitFetchUpdateOptions(const string& strConfigUrl, const map<string, string>& requestHeaders)
{
    m_strConfigUrl = strConfigUrl;
    m_requestHeaders = requestHeaders;
}
string FetchUpdateOptions::getConfigUrl()
{
    return m_strConfigUrl;
}
map<string, string> FetchUpdateOptions::getRequestHeaders()
{
    return m_requestHeaders;
}
void FetchUpdateOptions::SetConfigUrl(const string strConfigUrl)
{
    m_strConfigUrl = strConfigUrl;
}
void FetchUpdateOptions::SetRequestHeader(const map<string, string>& requestHeaders)
{
    m_requestHeaders = requestHeaders;
}