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

#ifndef MYAPPLICATION_HTTPURL_H
#define MYAPPLICATION_HTTPURL_H

#include <map>
#include <string>

/**
 * @brief url处理相关函数
 */
class HttpUrl {
    static const unsigned int LOG_PRINT_DOMAIN = 0xFF00;
    static std::map<std::string, std::string> s_mapMimeType;

public:
    /**
     * @brief Gets mime
     * @param strFilePath url path
     * @return mime
     */
    static std::string getMimeType(const std::string& strFilePath);
    /**
     * @brief Gets mime
     * @param resourceType
     * @param strUrlPath
     * @return mime
     */
    static std::string getMimeType(const int resourceType, const std::string& strUrlPath);
    /**
     * @brief Removes leading and trailing whitespace, including '\r' and '\n'.
     * @param strValue Input std::string.
     * @return Trimmed std::string.
     */
    static std::string trim(const std::string& strValue);
    /**
     * @brief Splits a std::string.
     * @param strString Input std::string.
     * @param split split Delimiter.
     * @param vecString Output vector of split strings.
     */
    static void
    splitString(const std::string& strString, const std::string& split, std::vector<std::string>& vecString);
    /**
     * @brief Generates an MD5 hash.
     * @param strIn Input std::string.
     * @return MD5 hash value.
     */
    static std::string generateMD5(const std::string& strIn);

    /**
     * Determine the end of the std::string
     * @param str
     * @param suffix
     * @return
     */
    static bool endsWithEqual(const std::string& str, const std::string& suffix);

    /**
     * @brief Removes duplicate items from a vector.
     * @tparam T Data type.
     * @param vecData Input vector.
     */
    template <class T> static void diffContData(std::vector<T>& vecData)
    {
        int nSize = vecData.size();
        for (int i = 0, nPos = 0; nPos < nSize;) {
            T tValue = vecData[i];
            auto currIt = vecData.begin() + i;
            auto it = find(vecData.begin() + i + 1, vecData.end(), tValue);
            if (it != vecData.end())
                vecData.erase(currIt);
            else
                i++;
            nPos++;
        }
        return;
    }

    /**
     * @brief 转小写
     * @param str
     * @return
     */
    static std::string toLower(const std::string& str);
};

#endif // MYAPPLICATION_HTTPURL_H
