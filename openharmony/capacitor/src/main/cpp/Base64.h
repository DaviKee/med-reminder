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

#ifndef BASE64_H
#define BASE64_H

#include <string>

/**
 * @brief define base64 encode and decode
 */
class CBase64 {
public:
    /**
     * @brief Base64 encode.
     * @param in Input buffer.
     * @param nLength Buffer length.
     * @return Encoded std::string (without line breaks).
     */
    static std::string encode(const char* in, int nLength = 0);
    /**
     * @brief Base64 decode.
     * @param strIn Input std::string.
     * @param nRetLen Output decoded length.
     * @return Pointer to the decoded buffer (must be freed after use).
     */
    static char* decode(const std::string& strIn, int& nRetLen);
    /**
     * @brief Base64 decode.
     * @param strIn Input std::string.
     * @return Decoded std::string (must be known in advance to be a valid std::string after decoding).
     */
    static std::string decode(const std::string& strIn);
};

#endif
