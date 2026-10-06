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

#ifndef CORDOVA_ARGS_H
#define CORDOVA_ARGS_H

#include "cJSON.h"
#include "Base64.h"
#include <string>

class CordovaArgs {
    cJSON* m_baseArgs;

public:
    explicit CordovaArgs(cJSON* args)
    {
        m_baseArgs = cJSON_Duplicate(args, 1);
    }

    ~CordovaArgs()
    {
        cJSON_Delete(m_baseArgs);
    }

    // Pass through the basics to the base args.
    cJSON* get(int index)
    {
        return cJSON_GetArrayItem(m_baseArgs, index);
    }

    int getSize()
    {
        return cJSON_GetArraySize(m_baseArgs);
    }

    bool getBoolean(int index)
    {
        return static_cast<bool>(cJSON_GetArrayItem(m_baseArgs, index)->valueint);
    }

    double getDouble(int index)
    {
        return static_cast<double>(cJSON_GetArrayItem(m_baseArgs, index)->valuedouble);
    }

    int getInt(int index)
    {
        return static_cast<int>(cJSON_GetArrayItem(m_baseArgs, index)->valueint);
    }

    cJSON* getJSONArray(int index)
    {
        return cJSON_GetArrayItem(m_baseArgs, index);
    }

    cJSON* getJSONObject(int index)
    {
        return cJSON_GetArrayItem(m_baseArgs, index);
    }

    long getLong(int index)
    {
        return static_cast<long>(cJSON_GetArrayItem(m_baseArgs, index)->valuedouble);
    }

    std::string getString(int index)
    {
        cJSON* pJson = cJSON_GetArrayItem(m_baseArgs, index);
        if (pJson != nullptr && pJson->type == cJSON_String) {
            return pJson->valuestring;
        }
        return "";
    }

    cJSON* opt(int index)
    {
        return cJSON_GetArrayItem(m_baseArgs, index);
    }

    bool optBoolean(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            switch (json->type) {
                case cJSON_False:
                    return false;
                case cJSON_True:
                    return true;
                default:
                    return false;
            }
        }
        return false;
    }

    double optDouble(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            switch (json->type) {
                case cJSON_Number:
                    return json->valuedouble;
                default:
                    return 0;
            }
        }
        return 0;
    }

    int optInt(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            switch (json->type) {
                case cJSON_Number:
                    return json->valueint;
                default:
                    return 0;
            }
        }
        return 0;
    }

    cJSON* optJSONArray(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            switch (json->type) {
                case cJSON_Array:
                    return json->child;
                default:
                    return nullptr;
            }
        }
        return nullptr;
    }

    cJSON* optJSONObject(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            switch (json->type) {
                case cJSON_Object:
                    return json->child;
                default:
                    return nullptr;
            }
        }
        return nullptr;
    }

    long optLong(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            switch (json->type) {
                case cJSON_Number:
                    return static_cast<long>(json->valuedouble);
                default:
                    return 0;
            }
        }
        return 0;
    }

    std::string optString(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            switch (json->type) {
                case cJSON_String:
                    return json->valuestring;
                default:
                    return "";
            }
        }
        return "";
    }

    bool isNull(int index)
    {
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            return true;
        }
        return false;
    }

    // The interesting custom helpers.
    std::vector<char> getArrayBuffer(int index)
    {
        std::vector<char> vecRet;
        std::string strRet;
        cJSON* json = cJSON_GetArrayItem(m_baseArgs, index);
        if (json != nullptr) {
            if (json->type == cJSON_String) {
                std::string str = json->valuestring;
                strRet = CBase64::decode(str);
            }
        }
        if (strRet.empty()) {
            return vecRet;
        }
        vecRet.insert(vecRet.begin(), strRet.c_str(), strRet.c_str() + strRet.length());
        return vecRet;
    }
};
#endif