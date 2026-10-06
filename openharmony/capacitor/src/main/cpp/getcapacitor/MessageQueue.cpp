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

#include "MessageQueue.h"
MessageQueue::MessageQueue() : m_evalBridge(this) {}

void MessageQueue::addJavaScript(const std::string &strWebTag, const std::string &statement)
{
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        JsMessage message(strWebTag, statement);
        m_queue.push(message);
    }
    m_evalBridge.onNativeToJsMessageAvailable();
}

bool MessageQueue::popJs(std::string &strWebTag, std::string &strJsStatement)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    int size = m_queue.size();
    if (size == 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "MessageQueue", "MessageQueue m_queue.size() == 0");
        return false;
    }

    JsMessage &jsMessage = m_queue.front();
    strWebTag = jsMessage.getWebTag();
    strJsStatement = jsMessage.getJsStatement();
    m_queue.pop();
    return true;
}

void MessageQueue::RunJavaScriptCallback(const char *result) const
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "MessageQueue",
                 "NativeToJsMessageQueue OH_NativeArkWeb_RunJavaScript result:%{public}s", result);
}


void MessageQueue::addTimer(const std::string &jsCode, const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexTimer);
    SafeTimer *pSafeTimer = new SafeTimer(this, seqId, jsCode, 2000, [](const std::string &js) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "MessageQueue", "%{public}s execution exceeds 2000ms",
                     js.c_str());
    });
    m_threadIdToTimer[seqId] = pSafeTimer;
}

void MessageQueue::delTimer(const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexTimer);
    if (m_threadIdToTimer.find(seqId) != m_threadIdToTimer.end()) {
        SafeTimer *pSafeTimer = (SafeTimer *)m_threadIdToTimer[seqId];
        m_threadIdToTimer.erase(seqId);
        delete pSafeTimer;
    }
}

void MessageQueue::addDetachId(const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexDetach);
    m_vecDetachId.push_back(seqId);
}
bool MessageQueue::delDetachId(const unsigned long seqId)
{
    std::lock_guard<std::mutex> guard(m_mutexDetach);
    auto it = find(m_vecDetachId.begin(), m_vecDetachId.end(), seqId);
    if (it != m_vecDetachId.end()) {
        m_vecDetachId.erase(it);
        return true;
    }
    return false;
}