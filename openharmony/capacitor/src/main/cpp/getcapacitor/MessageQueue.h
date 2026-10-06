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

#ifndef MyApplication_MESSAGEQUEUE_H
#define MyApplication_MESSAGEQUEUE_H

#include "Thread.h"
#include "hilog/log.h"
#include "Application.h"

#include <sys/time.h>
#include <string>
#include <thread>
#include <web/arkweb_type.h>

class MessageQueue {
    MessageQueue(const MessageQueue &);
    MessageQueue &operator=(const MessageQueue &);

    static const unsigned int LOG_PRINT_DOMAIN = 0xff00;
    std::mutex m_mutexTimer;
    std::map<unsigned long, void *> m_threadIdToTimer;

    std::mutex m_mutexDetach;
    std::vector<unsigned long> m_vecDetachId;

public:
    MessageQueue();
    ~MessageQueue() = default;
    class JsMessage {
        std::string m_strWebTag;
        std::string m_strJsStatement;

    public:
        JsMessage() {}
        JsMessage(const JsMessage &arg)
        {
            m_strWebTag = arg.m_strWebTag;
            m_strJsStatement = arg.m_strJsStatement;
        }
        JsMessage &operator=(const JsMessage &arg)
        {
            if (this == &arg) {
                return *this;
            }
            m_strWebTag = arg.m_strWebTag;
            m_strJsStatement = arg.m_strJsStatement;
            return *this;
        }

        JsMessage(const std::string strWebTag, const std::string &js)
        {
            m_strWebTag = strWebTag;
            m_strJsStatement = js;
        }
        std::string getWebTag() const { return m_strWebTag; }
        std::string getJsStatement() const { return m_strJsStatement; }
    };

    class SafeTimer {
    public:
        SafeTimer(MessageQueue *jsQueue, unsigned long seqId, const std::string &jsCode, const int nMilSec,
                  std::function<void(const std::string &)> handler)
            : m_jsQueue(jsQueue), m_seqId(seqId)
        {
            m_thread = std::thread([jsQueue, seqId, jsCode, nMilSec, handler]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(nMilSec));
                if (!jsQueue->delDetachId(seqId))
                    handler(jsCode);
            });
        }

        ~SafeTimer()
        {
            if (m_thread.joinable()) {
                m_jsQueue->addDetachId(m_seqId);
                m_thread.detach();
            }
        }

    private:
        std::thread m_thread;
        MessageQueue *m_jsQueue{nullptr};
        unsigned long m_seqId{0};
    };

    class EvalBridge : public CThread {
        static const int LOG_PRINT_DOMAIN = 0xff00;
        struct SCallBackUserData {
            unsigned long m_seqId;
            MessageQueue *m_queue{nullptr};
        };
        MessageQueue *m_queue;

    public:
        void Execute(void *) override
        {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridge", "Start Execute js");
            std::string strWebTag;
            std::string strJsStatement;
            m_queue->popJs(strWebTag, strJsStatement);
            if (!strJsStatement.empty()) {
                unsigned long seqId = getSeqId();
                /*
                 *Adds a timer; if execution exceeds 2000ms, only logs are printed.
                 *JavaScript execution time is only a reference for developers to evaluate system efficiency.
                 *For example, if JS involves UI interaction, whether it times out depends on user operations.
                 */
                m_queue->addTimer(strJsStatement, seqId);
                evaluateJavascript(strWebTag, strJsStatement, seqId);
            }
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridge", "End Execute js");
        }

        explicit EvalBridge(MessageQueue *queue) { m_queue = queue; }

        /**
         * @brief Starts a thread to execute the task after adding it to the queue.
         * Can run multiple times concurrently.
         */
        void onNativeToJsMessageAvailable() { Start(nullptr, false); }

        /**
         * @brief JavaScript execution callback function.
         * @param webTag Web tag string.
         * @param data Data returned after JavaScript execution on the JS side.
         * @param userData Pointer to the SCallBackUserData object passed during JavaScript execution.
         */
        static void StaticRunJavaScriptCallback(const char *webTag, const ArkWeb_JavaScriptBridgeData *data,
                                                void *userData)
        {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridge",
                         "EvalBridge StaticRunJavaScriptCallback webTag:%{public}s", webTag);
            if (!userData) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridge",
                             "EvalBridge StaticRunJavaScriptCallback userData is nullptr");
                return;
            }
            SCallBackUserData *pUserData = (SCallBackUserData *)userData;
            pUserData->m_queue->delTimer(pUserData->m_seqId);
            std::string result((char *)data->buffer, data->size);
            pUserData->m_queue->RunJavaScriptCallback(result.c_str());
            delete pUserData;
        }

        /**
         * @brief Executes JavaScript.
         * @param strWebTag Web tag string.
         * @param jsCode JavaScript code to execute.
         * @param seqId Task ID for execution.
         */
        void evaluateJavascript(const std::string &strWebTag, const std::string &jsCode, unsigned long seqId) const
        {
            if (strWebTag.empty()) {
                return;
            }
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "EvalBridge", "evaluateJavascript:%{public}s",
                         jsCode.c_str());
            SCallBackUserData *pUserData = new SCallBackUserData();
            pUserData->m_queue = m_queue;
            pUserData->m_seqId = seqId;
            ArkWeb_JavaScriptObject object = {(uint8_t *)jsCode.c_str(), jsCode.size(),
                                              &EvalBridge::StaticRunJavaScriptCallback, static_cast<void *>(pUserData)};
            Application::g_controller->runJavaScript(strWebTag.c_str(), &object);
        }

        /**
         * @brief Generates a task sequence based on execution time.
         * @return Task sequence.
         */
        unsigned long getSeqId() const
        {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            unsigned long microseconds = tv.tv_sec * 1000000 + tv.tv_usec;
            return microseconds;
        }
    };

    void addJavaScript(const std::string &strWebTag, const std::string &statement);
    bool popJs(std::string &strWebTag, std::string &strJsStatment);
    void RunJavaScriptCallback(const char *result) const;
    void addTimer(const std::string &jsCode, const unsigned long seqId);
    void delTimer(const unsigned long seqId);
    void addDetachId(const unsigned long seqId);
    bool delDetachId(const unsigned long seqId);

    EvalBridge m_evalBridge;

private:
    std::mutex m_mutex;
    std::queue<JsMessage> m_queue;
};

#endif // MyApplication_MESSAGEQUEUE_H
