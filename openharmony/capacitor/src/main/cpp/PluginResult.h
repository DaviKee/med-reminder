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

#ifndef PLUGIN_RESULT_H
#define PLUGIN_RESULT_H

#include "cJSON.h"
#include "Base64.h"
#include <string>
#include <vector>

/**
 * @brief Defines the plugin execution result class.
 */
class PluginResult {
    /**
     * Typically, m_status is set to a value of the enum Status type.
     * When m_status is set to -1, it indicates a direct call from the Native side to the JavaScript side.
     * In this case, the CallbackContext's m_callbackId property is a JavaScript statement.
     * Refer to JsMessage's encodeAsJsMessage combination function.
     */
    int m_status;
    int m_messageType;
    /**Repeated notification flag.*/
    bool m_keepCallback;
    std::string m_strMessage;
    std::string m_encodeMessage;
    std::vector<PluginResult> m_multipartMessages;

public:
    enum MessageType {
        MESSAGE_TYPE_STRING = 1,
        MESSAGE_TYPE_JSON = 2,
        MESSAGE_TYPE_NUMBER = 3,
        MESSAGE_TYPE_BOOLEAN = 4,
        MESSAGE_TYPE_NULL = 5,
        MESSAGE_TYPE_ARRAYBUFFER = 6,
        MESSAGE_TYPE_BINARYSTRING = 7,
        MESSAGE_TYPE_MULTIPART = 8,
    };

    static std::string StatusMessages[];

    enum Status {
        NO_RESULT,
        OK,
        CLASS_NOT_FOUND_EXCEPTION,
        ILLEGAL_ACCESS_EXCEPTION,
        INSTANTIATION_EXCEPTION,
        MALFORMED_URL_EXCEPTION,
        IO_EXCEPTION,
        INVALID_ACTION,
        JSON_EXCEPTION,
        ERROR
    };

    PluginResult();
    PluginResult(Status status);
    PluginResult(Status status, std::string message);
    PluginResult(Status status, cJSON* message);
    PluginResult(Status status, int i);
    PluginResult(Status status, long i);
    PluginResult(Status status, float f);
    PluginResult(Status status, double f);
    PluginResult(Status status, bool b);
    PluginResult(Status status, char data[], int length);
    PluginResult(Status status, const int& messageType, const std::string& message);
    PluginResult(Status status, char data[], int length, bool binaryString);
    PluginResult(Status status, std::vector<PluginResult> multipartMessages);

    PluginResult(const PluginResult& arg)
    {
        m_status = arg.m_status;
        m_messageType = arg.m_messageType;
        m_keepCallback = arg.m_keepCallback;
        m_strMessage = arg.m_strMessage;
        m_encodeMessage = arg.m_encodeMessage;
        m_multipartMessages = arg.m_multipartMessages;
    }

    PluginResult& operator=(const PluginResult& arg)
    {
        if (this == &arg) {
            return *this;
        }
        m_status = arg.m_status;
        m_messageType = arg.m_messageType;
        m_keepCallback = arg.m_keepCallback;
        m_strMessage = arg.m_strMessage;
        m_encodeMessage = arg.m_encodeMessage;
        m_multipartMessages = arg.m_multipartMessages;
        return *this;
    }

    /**
     * @brief Sets the callback flag (default is false).
     * @param b true: supports multiple callbacks; false: allows only one callback.
     */
    void setKeepCallback(bool b);
    void setStatus(const int nStatus);
    int getStatus() const;
    int getMessageType() const;
    std::string getMessage() const;
    int getMultipartMessagesSize() const;
    const PluginResult& getMultipartMessage(int index) const;
    std::string getStrMessage() const;
    bool getKeepCallback() const;
    std::string getJSONString() const;
};
#endif