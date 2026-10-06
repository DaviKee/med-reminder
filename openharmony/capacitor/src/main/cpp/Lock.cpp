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

#include "Lock.h"
#include <cstdarg> // va_list
#include <cstdio>

static bool Log(CLog::Level enuLevel, const char* pFun, const char* pFormat, ...)
{
    if (CLog::s_pLog == nullptr) {
        return false;
    }

    va_list ap;
    va_start(ap, pFormat);
    if (!(CLog::s_pLog->log(enuLevel, "CLock", pFun, pFormat, ap))) {
        return false;
    }
    va_end(ap);
    return true;
}

// CLock
CLock::CLock(const LockType type) : m_blnInited(false)
{
    pthread_mutexattr_t attr;
    int locktype;
    switch (type) {
        case Normal:
            locktype = PTHREAD_MUTEX_ADAPTIVE_NP;
            break;
        case ErrorCheck:
            locktype = PTHREAD_MUTEX_ERRORCHECK_NP;
            break;
        case Recursive:
            locktype = PTHREAD_MUTEX_RECURSIVE_NP;
            break;
        default:
            locktype = PTHREAD_MUTEX_RECURSIVE_NP;
            break;
    }

    int nRet = 0;
    nRet = pthread_mutexattr_init(&attr);
    if (nRet != 0) {
        Log(CLog::Error, "CLock", "pthread_mutexattr_init(&attr) != 0{nRet=%d,&attr=%08p}", nRet, &attr);
    } else {
        nRet = pthread_mutexattr_settype(&attr, locktype);
        if (nRet != 0) { // log
            Log(CLog::Error,
                "CLock",
                "pthread_mutexattr_settype(&attr,locktype) != 0{nRet=%d,&attr=%08p,locktype=%d}",
                nRet,
                &attr,
                locktype);
        } else {
            nRet = pthread_mutex_init(&m_mutexLock, &attr);
            if (nRet != 0) { // log
                Log(CLog::Error,
                    "CLock",
                    "pthread_mutex_init(&m_mutexLock, &attr) != 0{nRet=%d,&m_mutexLock=%08p,&attr=%08p}",
                    nRet,
                    &m_mutexLock,
                    &attr);
            } else {
                m_blnInited = true;
            }
        }
    }
}

CLock::~CLock()
{
    if (m_blnInited) {
        int nRet = pthread_mutex_destroy(&m_mutexLock);
        if (nRet != 0) { // log
            Log(CLog::Error,
                "~CLock",
                "pthread_mutex_destroy(&m_mutexLock) != 0{nRet=%d,&m_mutexLock=%08p}",
                nRet,
                &m_mutexLock);
        }
        m_blnInited = false;
    }
}

bool CLock::lock() const
{
    if (m_blnInited == false) {
        Log(CLog::Error, "Lock", "m_blnInited == false{}");
        return false;
    }
    int nRet = 0;
    nRet = pthread_mutex_lock(&m_mutexLock);
    if (nRet != 0) {
        Log(CLog::Error,
            "Lock",
            "pthread_mutex_lock(&m_mutexLock) != 0{nRet=%d,&m_mutexLock=%08p}",
            nRet,
            &m_mutexLock);
        return false;
    } else {
        return true;
    }
}

bool CLock::unlock() const
{
    if (m_blnInited == false) { // log
        Log(CLog::Error, "Unlock", "m_blnInited == false{}");
        return false;
    }
    int nRet = 0;
    nRet = pthread_mutex_unlock(&m_mutexLock);
    if (nRet != 0) { // log
        Log(CLog::Error,
            "Unlock",
            "pthread_mutex_unlock(&m_mutexLock) != 0{nRet=%d,&m_mutexLock=%08p}",
            nRet,
            &m_mutexLock);
        return false;
    } else {
        return true;
    }
}
