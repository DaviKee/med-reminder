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

#ifndef __LOG_H
#define __LOG_H

#include "hilog/log.h"
#include <cstdio>

/**
 * @brief Writes logs, primarily used for socket communication.
 */
class CLog {
public:
    CLog() {}

    ~CLog() {}

    enum Level { Info, Warn, Error };

    /**
     * @brief Writes a log entry.
     * @param enuLevel Log level.
     * @param pFile Source file name.
     * @param pFunc Function name.
     * @param pFormat Format string.
     * @param arglist Variable argument list.
     * @return true
     */
    bool log(const Level enuLevel, const char* pFile, const char* pFunc, const char* pFormat, va_list arglist)
    {
        const int const_buf_size = 8192;
        char buffer[const_buf_size];
        vsnprintf(buffer, sizeof(buffer), pFormat, arglist);
        buffer[const_buf_size - 1] = 0;
        if (enuLevel == Info) {
            OH_LOG_Print(LOG_APP, LOG_INFO, 0xff00, pFile, "Info: %{public}s %{public}s", pFunc, buffer);
        }
        if (enuLevel == Warn) {
            OH_LOG_Print(LOG_APP, LOG_WARN, 0xff00, pFile, "Warn: %{public}s %{public}s", pFunc, buffer);
        }
        if (enuLevel == Error) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, 0xff00, pFile, "Error: %{public}s %{public}s", pFunc, buffer);
        }
        return true;
    }

    /**Static pointer for logging, initialized in socket.cpp.*/
    static CLog* s_pLog;
};
#endif