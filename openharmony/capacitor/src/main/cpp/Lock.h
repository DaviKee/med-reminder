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

#ifndef __LOCK_H
#define __LOCK_H

#include <pthread.h>
#include "Log.h"

/**
 * @brief define pthread lock
 */
class CLock {
public:
    enum MUTEX_TYPE {
        PTHREAD_MUTEX_ADAPTIVE_NP = 3,
        PTHREAD_MUTEX_ERRORCHECK_NP = 2,
        PTHREAD_MUTEX_RECURSIVE_NP = 1,
    };

    enum LockType { Normal, ErrorCheck, Recursive };

public:
    CLock(const LockType type = Recursive);
    ~CLock();

public:
    bool lock() const;
    bool unlock() const;

private:
    bool m_blnInited;
    mutable pthread_mutex_t m_mutexLock;
};

/**
 * define guard of lock
 */
class CGuard {
    const CLock* m_pLock;

public:
    explicit CGuard(const CLock& lock) : m_pLock(&lock)
    {
        m_pLock->lock();
    };

    ~CGuard()
    {
        m_pLock->unlock();
        m_pLock = nullptr;
    };
};


#endif // #ifndef __LOCK_H
