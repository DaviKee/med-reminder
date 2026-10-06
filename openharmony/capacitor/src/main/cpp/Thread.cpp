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


#include "Thread.h"
#include <cerrno>
#include <unistd.h>
#include <algorithm>
#include <sys/time.h>
///////////////////////////////////////////////////////////////////////
// class CThread
//
pthread_mutex_t CThread::m_mutexRun = PTHREAD_MUTEX_INITIALIZER;

std::vector<void*> CThread::m_ThreadList;

CThread::CThread()
{
    m_bStop = false;
    m_thread = 0;
    m_arg = nullptr;
    pthread_mutex_init(&m_mutexStop, nullptr);
}

CThread::~CThread()
{
    pthread_mutex_destroy(&m_mutexStop);
}

bool CThread::start(void* arg, bool bJoinable)
{
    bool bResult = false;
    pthread_mutex_lock(&m_mutexStop);
    addSelfToList();
    try {
        setArg(arg);
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        if (bJoinable) {
            pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
        } else {
            pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        }

        if (pthread_create(&m_thread, &thread_attr, &(CThread::entryPoint), this) == 0) {
            bResult = true;
        } else {
            removeSelfFromList();
            bResult = false;
        }
    } catch (...) {
        removeSelfFromList();
        bResult = false;
    }
    pthread_mutex_unlock(&m_mutexStop);
    return bResult;
}

void CThread::stop()
{
    setStopFlag();
}

int CThread::join(void** ppRetval)
{
    return pthread_join(getThreadId(), ppRetval);
}

void CThread::run(void* arg)
{
    setup();
    execute(arg);
    resetStopFlag();
    removeSelfFromList();
}

void* CThread::entryPoint(void* pthis)
{
    CThread* pt = (CThread*)pthis;
    pt->run(pt->getArg());
    return nullptr;
}

void CThread::setup()
{
    // Anything you want to be executed before Execute().
}

bool CThread::getStopFlag()
{
    bool bResult;
    pthread_mutex_lock(&m_mutexStop);
    bResult = m_bStop;
    pthread_mutex_unlock(&m_mutexStop);
    return bResult;
}

void CThread::setStopFlag()
{
    pthread_mutex_lock(&m_mutexStop);
    m_bStop = true;
    pthread_mutex_unlock(&m_mutexStop);
}

void CThread::resetStopFlag()
{
    pthread_mutex_lock(&m_mutexStop);
    m_bStop = false;
    pthread_mutex_unlock(&m_mutexStop);
}

void CThread::addSelfToList()
{
    pthread_mutex_lock(&m_mutexRun);
    m_ThreadList.push_back(this);
    pthread_mutex_unlock(&m_mutexRun);
}

void CThread::removeSelfFromList()
{
    pthread_mutex_lock(&m_mutexRun);
    std::vector<void*>::iterator iter = find(m_ThreadList.begin(), m_ThreadList.end(), this);
    if (iter != m_ThreadList.end()) {
        m_ThreadList.erase(iter);
    }
    pthread_mutex_unlock(&m_mutexRun);
}

bool CThread::isRunning()
{
    bool bResult;
    std::vector<void*>::iterator iter;
    pthread_mutex_lock(&m_mutexRun);
    iter = find(m_ThreadList.begin(), m_ThreadList.end(), this);
    if (iter == m_ThreadList.end()) {
        bResult = false;
    } else {
        bResult = true;
    }
    pthread_mutex_unlock(&m_mutexRun);
    return bResult;
}


///////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// class CThreadPool
CThreadPool::CThreadPool()
{
    m_arg = nullptr;
    pthread_mutex_init(&m_mutexCond, nullptr);
    pthread_cond_init(&m_cond, nullptr);
    pthread_mutex_init(&m_mutexTask, nullptr);
}

CThreadPool::~CThreadPool()
{
    pthread_mutex_destroy(&m_mutexCond);
    pthread_cond_destroy(&m_cond);
    pthread_mutex_destroy(&m_mutexTask);
}

bool CThreadPool::start(void* arg)
{
    bool bResult = true;
    pthread_t thread;

    try {
        setArg(arg);
        for (int i = 0; i < m_nPoolSize; i++) {
            pthread_attr_t thread_attr;
            pthread_attr_init(&thread_attr);
            pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
            if (pthread_create(&thread, &thread_attr, &(CThreadPool::entryPoint), this) == 0) {
                bResult = true;
            } else {
                bResult = false;
                break;
            }
            pthread_attr_destroy(&thread_attr); // 释放资源
        }
    } catch (...) {
        bResult = false;
    }
    return bResult;
}

bool CThreadPool::startThread()
{
    bool bResult = false;
    pthread_t thread;
    try {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&thread, &thread_attr, &(CThreadPool::entryPoint), this) == 0) {
            bResult = true;
            m_nCurPoolSize++;
        } else {
            bResult = false;
        }
        pthread_attr_destroy(&thread_attr); // 释放资源
    } catch (...) {
        bResult = false;
    }
    return bResult;
}

void CThreadPool::stop()
{
    setStopFlag();
    for (int i = 0; i < m_nPoolSize; i++) {
        dealTask();
    }
}

bool CThreadPool::getStopFlag()
{
    bool bResult = m_bStop;
    return bResult;
}

bool CThreadPool::getThreadStop()
{
    if (getWaitingThreads() <= 0) {
        return false;
    }
    bool bResult = false;
    if (m_nPoolSize.load() < m_nCurPoolSize.load()) {
        m_nCurPoolSize--;
        bResult = true;
    }
    return bResult;
}

void CThreadPool::setStopFlag()
{
    m_bStop = true;
}

void CThreadPool::resetStopFlag()
{
    m_bStop = false;
}

void CThreadPool::run(void* arg)
{
    loopTask(arg);
    resetStopFlag();
}

void CThreadPool::loopTask(void* arg)
{
    while (true) {
        const void* pTask;
        int nTotalTask = 0;
        while ((pTask = getTask(nTotalTask)) == nullptr) {
            pthread_mutex_lock(&m_mutexCond);
            m_nWaitingThread++;
            pthread_cond_wait(&m_cond, &m_mutexCond);
            m_nWaitingThread--;
            pthread_mutex_unlock(&m_mutexCond);
            if (getStopFlag()) {
                return;
            }
        }
        void* pTmp = initThreadData();
        execute(pTask, nTotalTask, pTmp);
        deInitThreadData(pTmp);
        if (getThreadStop()) {
            break;
        }
    }
    return;
}

void* CThreadPool::entryPoint(void* pthis)
{
    CThreadPool* pt = (CThreadPool*)pthis;
    pt->run(pt->getArg());
    return nullptr;
}

void CThreadPool::addTask(const void* arg)
{
    pthread_mutex_lock(&m_mutexTask);
    m_taskQue.push(arg);
    pthread_mutex_unlock(&m_mutexTask);
    if (getWaitingThreads() > 0 || m_nCurPoolSize.load() >= MAX_THREAD_NUMBER_BOUNDARY) {
        dealTask();
    } else {
        startThread();
    }
}

const void* CThreadPool::getTask(int& nTotalTask)
{
    const void* pRet = nullptr;
    pthread_mutex_lock(&m_mutexTask);
    if (!m_taskQue.empty()) {
        pRet = m_taskQue.front();
        m_taskQue.pop();
        nTotalTask = m_taskQue.size();
    }
    pthread_mutex_unlock(&m_mutexTask);
    return pRet;
}

void CThreadPool::setPoolSize(const int nSize)
{
    m_nPoolSize = nSize;
    m_nCurPoolSize = nSize;
}

void CThreadPool::dealTask()
{
    pthread_cond_signal(&m_cond);
}

int CThreadPool::getWaitingThreads()
{
    int count = 0;
    count = m_nWaitingThread.load();
    return count;
}

int CThreadPool::getCurPoolSize()
{
    int count = 0;
    count = m_nCurPoolSize.load();
    return count;
}
////////////////////////////////////////////////////////////////////////////////

// class CThreadStep
//

CThreadStep::CThreadStep()
{
    pthread_mutex_init(&m_mutexStep, nullptr);
    pthread_cond_init(&m_cond, nullptr);
}

CThreadStep::~CThreadStep()
{
    pthread_mutex_destroy(&m_mutexStep);
    pthread_cond_destroy(&m_cond);
}

bool CThreadStep::isIdle()
{
    int nRet = pthread_mutex_trylock(&m_mutexStep);
    if (nRet == EBUSY) {
        return false;
    } else if (nRet == 0) {
        pthread_mutex_unlock(&m_mutexStep);
        return true;
    } else {
        return false;
    }
}

void CThreadStep::nextStep()
{
    pthread_cond_signal(&m_cond);
}

void CThreadStep::execute(void* arg)
{
    while (true) {
        pthread_cond_wait(&m_cond, &m_mutexStep);
        if (getStopFlag()) {
            pthread_mutex_unlock(&m_mutexStep);
            break;
        }
        stepTask(arg);
    }
}

void CThreadStep::stop()
{
    CThread::stop();
    pthread_cond_signal(&m_cond);
}


////////////////////////////////////////////////////////////////////
// class CThreadLoop
CThreadLoop::CThreadLoop()
{
    m_bPause = false;
    m_nSleepTime = 0;
    pthread_mutex_init(&m_mutexPause, nullptr);
    pthread_mutex_init(&m_mutexLoop, nullptr);
}

CThreadLoop::~CThreadLoop()
{
    pthread_mutex_destroy(&m_mutexPause);
    pthread_mutex_destroy(&m_mutexLoop);
}

void CThreadLoop::stop()
{
    CThread::stop();
}

void CThreadLoop::pause()
{
    pthread_mutex_lock(&m_mutexPause);
    m_bPause = true;
    pthread_mutex_unlock(&m_mutexPause);
    while (!isIdle()) {
        timeval tv = {0, 200000};
        select(0, nullptr, nullptr, nullptr, &tv);
    }
}

void CThreadLoop::resume()
{
    pthread_mutex_lock(&m_mutexPause);
    m_bPause = false;
    pthread_mutex_unlock(&m_mutexPause);
}

bool CThreadLoop::getPauseStatus()
{
    bool bPause;
    pthread_mutex_lock(&m_mutexPause);
    bPause = m_bPause;
    pthread_mutex_unlock(&m_mutexPause);
    return bPause;
}

bool CThreadLoop::isIdle()
{
    int nRet = pthread_mutex_trylock(&m_mutexLoop);
    if (nRet == EBUSY) {
        return false;
    } else if (nRet == 0) {
        pthread_mutex_unlock(&m_mutexLoop);
        return true;
    } else {
        return false;
    }
}

void CThreadLoop::execute(void* arg)
{
    while (!getStopFlag()) {
        if (getPauseStatus()) {
            // sleep 50ms
            timeval tv = {0, 50000};
            select(0, nullptr, nullptr, nullptr, &tv);
            continue;
        }

        pthread_mutex_lock(&m_mutexLoop);
        loopTask(arg);
        pthread_mutex_unlock(&m_mutexLoop);
        // don't sleep too long, replace with sleeping multitimes, 50ms per time
        if (m_nSleepTime > 0) {
            for (int i = 0; i <= m_nSleepTime / 50 && !getStopFlag() && !getPauseStatus(); i++) {
                // at least sleep 50ms
                timeval tv = {0, 50000};
                select(0, nullptr, nullptr, nullptr, &tv);
            }
        }
    }
}
