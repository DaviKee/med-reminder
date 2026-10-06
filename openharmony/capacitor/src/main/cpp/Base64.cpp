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

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Base64.h"

static char _base64_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q',
                               'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                               'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
                               'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'};
static char _base64_pad = '=';

std::string CBase64::encode(const char* str, int nLength)
{
    std::string ret;
    const unsigned char* current = reinterpret_cast<unsigned char*>(const_cast<char*>(str));
    int i = 0;
    char* result;
    if (str == nullptr || nLength < 0) {
        return ret;
    }
    if (nLength == 0) {
        nLength = strlen(str);
    }

    result = reinterpret_cast<char*>(malloc(((nLength + 3 - nLength % 3) * 4 / 3 + 1) * sizeof(char)));

    while (nLength > 2) { /* keep going until we have less than 24 bits */
        result[i++] = _base64_table[current[0] >> 2];
        result[i++] = _base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
        result[i++] = _base64_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
        result[i++] = _base64_table[current[2] & 0x3f];

        current += 3;
        nLength -= 3; /* we just handle 3 octets of data */
    }

    /* now deal with the tail end of things */
    if (nLength != 0) {
        result[i++] = _base64_table[current[0] >> 2];
        if (nLength > 1) {
            result[i++] = _base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
            result[i++] = _base64_table[(current[1] & 0x0f) << 2];
            result[i++] = _base64_pad;
        } else {
            result[i++] = _base64_table[(current[0] & 0x03) << 4];
            result[i++] = _base64_pad;
            result[i++] = _base64_pad;
        }
    }
    result[i] = '\0';
    ret = result;
    free(result);
    return ret;
}

/* as above, but backwards. :) */
char* CBase64::decode(const std::string& strIn, int& nRetLen)
{
    int length = strIn.length();
    const unsigned char* current = reinterpret_cast<const unsigned char*>(strIn.c_str());
    int ch, i = 0, j = 0, k;
    /* this sucks for threaded environments */
    short reverse_table[256];
    int table_built = 0;
    char* result;

    if (length < 0) {
        return nullptr;
    }
    if (++table_built == 1) {
        char* chp;
        for (ch = 0; ch < 256; ch++) {
            chp = strchr(_base64_table, ch);
            if (chp) {
                reverse_table[ch] = chp - _base64_table;
            } else {
                reverse_table[ch] = -1;
            }
        }
    }

    result = (char*)malloc(length + 1);
    if (result == nullptr) {
        return nullptr;
    }

    /* run through the whole std::string, converting as we go */
    while ((ch = *current++) != '\0') {
        if (ch == _base64_pad) {
            break;
        }

        /* When Base64 gets POSTed, all pluses are interpreted as spaces.
           This line changes them back.  It's not exactly the Base64 spec,
           but it is completely compatible with it (the spec says that
           spaces are invalid).  This will also save many people considerable
           headache.  - Turadg Aleahmad <turadg@wise.berkeley.edu>
        */

        if (ch == ' ') {
            ch = '+';
        }

        ch = reverse_table[ch];
        if (ch < 0) {
            continue;
        }

        switch (i % 4) {
            case 0:
                result[j] = ch << 2;
                break;
            case 1:
                result[j++] |= ch >> 4;
                result[j] = (ch & 0x0f) << 4;
                break;
            case 2:
                result[j++] |= ch >> 2;
                result[j] = (ch & 0x03) << 6;
                break;
            case 3:
                result[j++] |= ch;
                break;
            default:
                break;
        }

        i++;
    }

    k = j;
    /* mop things up if we ended on a boundary */
    if (ch == _base64_pad) {
        switch (i % 4) {
            case 0:
            case 1:
                free(result);
                return nullptr;
            case 2:
                k++;
            case 3:
                result[k++] = 0;
            default:
                break;
        }
    }
    nRetLen = j;
    result[k] = '\0';
    return result;
}

std::string CBase64::decode(const std::string& strIn)
{
    std::string ret;
    int retlen;
    char* res = decode(strIn, retlen);
    ret = res;
    if (res != nullptr) {
        free(res);
    }
    return ret;
}
