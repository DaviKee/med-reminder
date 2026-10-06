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


#ifndef _THREAD_H__
#define _THREAD_H__

#include <vector>
#include <queue>
#include <pthread.h>

/**
 *  @brief Base class for multi-threading. Cannot be used directly; must be implemented by subclasses for specific
 * functionality. 1，Starting a single thread. 2，Starting a thread pool. 3，Step-by-step execution threads.
 *      4，Repeatedly executed threads.
 */
class CThread {
public:
    CThread();
    virtual ~CThread();
    /**
     * @brief Starts the thread.
     * @param arg A pointer to user data.
     * @param bJoinable  Whether the thread is joinable or detached.
     * @return true if success,false if failed
     */
    bool start(void* arg, bool bJoinable = true);
    [[deprecated("Use start instead")]]
    bool Start(void* arg, bool bJoinable = true)
    {
        return start(arg, bJoinable);
    }
    /**
     * @brief Checks if the thread is currently running.
     * @return true if running, false if finished
     */
    bool isRunning();
    [[deprecated("Use isRunning instead")]]
    bool IsRunning()
    {
        return isRunning();
    }
    /**
     * @brief Sets stop flag
     * @return
     */
    virtual void stop();
    [[deprecated("Use stop instead")]]
    virtual void Stop()
    {
        return stop();
    }
    /**
     * @brief Gets id of thread
     * @return id of thread
     */
    pthread_t getThreadId()
    {
        return m_thread;
    }
    [[deprecated("Use getThreadId instead")]]
    pthread_t GetThreadId()
    {
        return getThreadId();
    }
    /**
     * @brief Waits for the thread to finish.
     * @param[out] ppRetval Return value of the thread.
     * @return 0 if success，otherwise failed.
     */
    int join(void** ppRetval);
    [[deprecated("Use join instead")]]
    int Join(void** ppRetval)
    {
        return join(ppRetval);
    }
protected:
    /**
     * @brief Execution function of the thread.
     * @param arg A pointer to user data.
     */
    void run(void* arg);
    [[deprecated("Use run instead")]]
    void Run(void* arg)
    {
        return run(arg);
    }
    /**
     * @brief Entry function of the thread.
     * @param arg A pointer to user data.
     * @return null (return value has no effect).
     */
    static void* entryPoint(void* pthis);
    [[deprecated("Use entryPoint instead")]]
    static void* EntryPoint(void* pthis)
    {
        return entryPoint(pthis);
    }
    /**
     * @brief Anything you want to be executed before Execute()
     */
    virtual void setup();
    [[deprecated("Use setup instead")]]
    virtual void Setup()
    {
        return setup();
    }
    /**
     * @brief Specific implementation function of the thread (to be implemented by subclasses).
     * @param arg A pointer to user data.
     */
    virtual void execute(void* arg)
    {
        return Execute(arg);
    };
    [[deprecated("Use execute instead")]]
    virtual void Execute(void*)
    {
        return;
    }
    /**
     * @brief Gets pointer of user data
     * @return A pointer to user data.
     */
    void* getArg() const
    {
        return m_arg;
    }
    [[deprecated("Use getArg instead")]]
    void* GetArg() const
    {
        return getArg();
    }
    /**
     * @brief Sets pointer of user data
     * @param a A pointer to user data.
     */
    void setArg(void* a)
    {
        m_arg = a;
    }
    [[deprecated("Use setArg instead")]]
    void SetArg(void* a)
    {
        return setArg(a);
    }
    /**
     * @brief Gets stop flag
     * @return true if stop, false if running
     */
    bool getStopFlag();
    [[deprecated("Use getStopFlag instead")]]
    bool GetStopFlag()
    {
        return getStopFlag();
    }
    /**
     * @brief Sets stop flag
     */
    void setStopFlag();
    [[deprecated("Use setStopFlag instead")]]
    void SetStopFlag()
    {
        return setStopFlag();
    }
    /**
     * @brief Resets stop flag
     */
    void resetStopFlag();
    [[deprecated("Use resetStopFlag instead")]]
    void ResetStopFlag()
    {
        return resetStopFlag();
    }
private:
    void* m_arg;
    bool m_bStop;
    pthread_t m_thread;
    pthread_mutex_t m_mutexStop;

private:
    void addSelfToList();
    void removeSelfFromList();
    static pthread_mutex_t m_mutexRun;
    static std::vector<void*> m_ThreadList;
};

/**
 * @brief defines class of thread pool
 */
class CThreadPool {
    /**
     * Maximum number of threads in the pool. This thread pool is used for Cordova network requests.
     * Network requests use a domain-based grouped thread pool, where each domain can have up to 20 threads processing
     * requests in the background.
     */
    static const int MAX_THREAD_NUMBER_BOUNDARY = 20;

public:
    CThreadPool();
    ~CThreadPool();
    /**
     * @brief Initial size of the thread pool.
     * @param nSize number of threads
     */
    void setPoolSize(const int nSize);
    [[deprecated("Use setPoolSize instead")]]
    void SetPoolSize(const int nSize)
    {
        return setPoolSize(nSize);
    }
    /**
     * @brief add task
     * @param arg data of task
     */
    void addTask(const void* arg);
    [[deprecated("Use addTask instead")]]
    void AddTask(const void* arg)
    {
        return addTask(arg);
    }
    /**
     * @brief start thread pool
     * @param arg pointer of use data
     * @return true if success, false if failed
     */
    bool start(void* arg);
    [[deprecated("Use start instead")]]
    bool Start(void* arg)
    {
        return start(arg);
    }
    /**
     * @brief stop thread pool
     */
    virtual void stop();
    [[deprecated("Use stop instead")]]
    virtual void Stop()
    {
        return stop();
    }
    /**
     * @brief Gets the number of idle threads in the pool.
     * @return int Number of idle threads.
     */
    int getWaitingThreads();
    [[deprecated("Use getWaitingThreads instead")]]
    int GetWaitingThreads()
    {
        return getWaitingThreads();
    }
    /**
     * @brief Gets the current number of threads in the pool.
     * @return Current number of threads in the pool.
     */
    int getCurPoolSize();
    [[deprecated("Use getCurPoolSize instead")]]
    int GetCurPoolSize()
    {
        return getCurPoolSize();
    }

protected:
    /**
     * @brief Execution function of the thread.
     * @param arg A pointer to user data.
     */
    void run(void* arg);
    [[deprecated("Use run instead")]]
    void Run(void* arg)
    {
        return run(arg);
    }
    /**
     * @brief Specific implementation function of the thread (to be implemented by subclasses).
     * @param arg A pointer to user data.
     */
    virtual void execute(const void* arg, const int nCountOfTask, void* pTmp)
    {
        return Execute(arg, nCountOfTask, pTmp);
    }
    [[deprecated("Use execute instead")]]
    virtual void Execute(const void*, const int, void*)
    {
        return;
    };
    /**
     * @brief Specific function to execute the task (can be implemented by either the subclass or the parent class).
     * @param arg pointer of use data
     */
    virtual void loopTask(void* arg);
    [[deprecated("Use loopTask instead")]]
    virtual void LoopTask(void* arg)
    {
        return loopTask(arg);
    }
    /**
     * @brief Gets pointer of use data
     * @return pointer of use data
     */
    void* getArg() const
    {
        return m_arg;
    }
    [[deprecated("Use getArg instead")]]
    void* GetArg() const
    {
        return getArg();
    }
    /**
     * @brief Sets pointer of use data
     * @param a pointer of use data
     */
    void setArg(void* a)
    {
        m_arg = a;
    }
    [[deprecated("Use setArg instead")]]
    void SetArg(void* a)
    {
        return setArg(a);
    }
    /**
     * @brief Thread creation function.
     * @param pObj Passes the current object.
     * @return null (return value has no effect).
     */
    static void* entryPoint(void*);
    [[deprecated("Use entryPoint instead")]]
    static void* EntryPoint(void* pthis)
    {
        return entryPoint(pthis);
    }
    /**
     * @brief Gets stop flag
     * @return true if stop, false if running
     */
    bool getStopFlag();
    [[deprecated("Use getStopFlag instead")]]
    bool GetStopFlag()
    {
        return getStopFlag();
    }
    /**
     * @brief Sets stop flag
     */
    void setStopFlag();
    [[deprecated("Use setStopFlag instead")]]
    void SetStopFlag()
    {
        return setStopFlag();
    }
    /**
     * @brief Resets stop flag
     */
    void resetStopFlag();
    [[deprecated("Use resetStopFlag instead")]]
    void ResetStopFlag()
    {
        return resetStopFlag();
    }

private:
    const void* getTask(int& nTotalTask);
    void* m_arg;
    std::atomic<bool> m_bStop{false};
    std::queue<const void*> m_taskQue;
    /**Initial number of threads. This count remains unchanged after initialization, maintaining the minimum number in
     * the pool.*/
    std::atomic<int> m_nPoolSize{0};
    /**Current number of threads. The thread count automatically increases based on the task queue, with the initial
     * count as the minimum.*/
    std::atomic<int> m_nCurPoolSize{0};
    /**Current number of idle threads (i.e., threads waiting for tasks).*/
    std::atomic<int> m_nWaitingThread{0};
    pthread_mutex_t m_mutexCond;
    pthread_cond_t m_cond;
    pthread_mutex_t m_mutexTask;
    /**
     * @brief Processes the task.
     */
    void dealTask();
    [[deprecated("Use dealTask instead")]]
    void DealTask()
    {
        return dealTask();
    }
    bool startThread();
    [[deprecated("Use startThread instead")]]
    bool StartThread()
    {
        return startThread();
    }
    /**
     * @brief Gets stop flag
     * @return true if stop, false if Non-stop
     */
    bool getThreadStop();
    [[deprecated("Use getThreadStop instead")]]
    bool GetThreadStop()
    {
        return getThreadStop();
    }
    /**
     * @brief Initialization function called when the thread starts (to be implemented by subclasses, typically used for
     * memory allocation for the thread).
     * @return Returns a memory pointer.
     */
    virtual void* initThreadData()
    {
        return InitThreadData();
    }
    [[deprecated("Use initThreadData instead")]]
    virtual void* InitThreadData()
    {
        return nullptr;
    }
    /**
     * @brief Function executed before the thread ends (to be implemented by subclasses, typically used to clean up
     * memory allocated during initialization).
     * @param pArg Pointer returned by thread initialization.
     */
    virtual void deInitThreadData(void* pArg)
    {
        return DeInitThreadData(pArg);
    }
    [[deprecated("Use deInitThreadData instead")]]
    virtual void DeInitThreadData(void* pArg)
    {
        return;
    }
};

/**
 * @brief Step-by-step execution thread.
 */
class CThreadStep : public CThread {
public:
    CThreadStep();
    virtual ~CThreadStep();
    /**
     * @brief Checks if the thread is idle.
     * @return true if idle, false if not idle.
     */
    bool isIdle();
    /**
     * @brief Executes the next step.
     */
    void nextStep();
    /**
     * @brief Sets stop flag
     */
    virtual void stop() override;

protected:
    /**
     * @brief Specific implementation function of the thread (to be implemented by subclasses).
     * @param arg A pointer to user data.
     */
    virtual void execute(void* arg) override;
    /**
     * @brief Executes the next step (to be implemented by subclasses).
     * @param arg A pointer to user data
     */
    virtual void stepTask(void* arg) = 0;

private:
    pthread_mutex_t m_mutexStep;
    pthread_cond_t m_cond;
};

class CThreadLoop : public CThread {
public:
    CThreadLoop();
    virtual ~CThreadLoop();
    /**
     * @brief Sets stop flag
     */
    virtual void stop() override;
    /**
     * @brief pause task
     */
    void pause();
    /**
     * @brief Restarts task
     */
    void resume();
    /**
     * @brief Get status of pause
     * @return true if pause,false if running
     */
    bool getPauseStatus();
    /**
     * @brief Checks if the thread is idle.
     * @return true if idle, false if not idle.
     */
    bool isIdle();

protected:
    /**
     * @brief Specific implementation function of the thread (to be implemented by subclasses).
     * @param arg A pointer to user data.
     */
    virtual void execute(void* arg) override;
    /**
     * @brief Specific execution function of the thread (to be implemented by subclasses).
     * @param arg A pointer of user data
     */
    virtual void loopTask(void* arg) = 0;

    /**
     * @brief Sets the thread sleep time.
     * @param nMilliSec Time in milliseconds.
     */
    void setSleepTime(int nMilliSec)
    {
        m_nSleepTime = nMilliSec;
    }

private:
    pthread_mutex_t m_mutexPause;
    pthread_mutex_t m_mutexLoop;
    bool m_bPause;
    int m_nSleepTime; // milli-seconds
};

#endif //_THREAD_H__
