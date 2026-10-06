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

 #include "PluginResult.h"

std::string PluginResult::StatusMessages [] =  {
    "No result",
    "OK",
    "Class not found",
    "Illegal access",
    "Instantiation error",
    "Malformed url",
    "IO error",
    "Invalid action",
    "JSON error",
    "Error"
};

PluginResult::PluginResult()
{
    m_status = -1;
    m_messageType = -1;
    m_keepCallback = false;
}

PluginResult::PluginResult(Status status):m_status(status)
{
    m_strMessage = "";
    if (m_status < PluginResult::NO_RESULT || m_status > PluginResult::ERROR) {
        m_messageType = MESSAGE_TYPE_NULL;
    }  else {
        m_messageType = MESSAGE_TYPE_STRING;
        m_strMessage = StatusMessages[status];
    }
}

PluginResult::PluginResult(Status status, std::string message) :m_status(status)
{
    m_strMessage = message;
    if (message.empty()) {
        m_messageType = MESSAGE_TYPE_NULL;
    } else {
        m_messageType = MESSAGE_TYPE_STRING;
    }
}

PluginResult::PluginResult(Status status, cJSON* message) :m_status(status)
{
    m_messageType = MESSAGE_TYPE_JSON;
    m_encodeMessage = cJSON_Print(message);
}

PluginResult::PluginResult(Status status, int i):m_status(status)
{
    m_messageType = MESSAGE_TYPE_NUMBER;
    char szBuf[32];
    snprintf(szBuf, sizeof(szBuf), "%d", i);
    m_encodeMessage = szBuf;
}

PluginResult::PluginResult(Status status, long i):m_status(status)
{
    m_messageType = MESSAGE_TYPE_NUMBER;
    char szBuf[64];
    snprintf(szBuf, sizeof(szBuf), "%ld", i);
    m_encodeMessage = szBuf;
}

PluginResult::PluginResult(Status status, float f):m_status(status)
{
    m_messageType = MESSAGE_TYPE_NUMBER;
    char szBuf[32];
    snprintf(szBuf, sizeof(szBuf), "%f", f);
    m_encodeMessage = szBuf;
}

PluginResult::PluginResult(Status status, double f):m_status(status)
{
    m_messageType = MESSAGE_TYPE_NUMBER;
    char szBuf[64];
    snprintf(szBuf, sizeof(szBuf), "%lf", f);
    m_encodeMessage = szBuf;
}

PluginResult::PluginResult(Status status, bool b):m_status(status)
{
    m_messageType = MESSAGE_TYPE_BOOLEAN;
    if (b) {
        m_encodeMessage = "true";
    } else {
        m_encodeMessage = "false";
    }
}
PluginResult::PluginResult(Status status, char data[], int length):m_status(status)
{
    m_messageType = MESSAGE_TYPE_ARRAYBUFFER;
    m_encodeMessage = CBase64::encode(data, length);
}

PluginResult::PluginResult(Status status, char data[], int length, bool binaryString):m_status(status)
{
    if (binaryString) {
        m_messageType = MESSAGE_TYPE_BINARYSTRING;
    } else {
        m_messageType = MESSAGE_TYPE_ARRAYBUFFER;
    }
    m_encodeMessage = CBase64::encode(data, length);
}

PluginResult::PluginResult(Status status, const int& messageType, const std::string& message):m_status(status)
{
    m_messageType = messageType;
    m_encodeMessage = message;
}

PluginResult::PluginResult(Status status, std::vector<PluginResult> multipartMessages):m_status(status)
{
    m_messageType = MESSAGE_TYPE_MULTIPART;
    m_multipartMessages = multipartMessages;
}

void PluginResult::setKeepCallback(bool b)
{
    m_keepCallback = b;
}

void PluginResult::setStatus(const int nStatus)
{
    m_status = nStatus;
}

int PluginResult::getStatus() const
{
    return m_status;
}

int PluginResult::getMessageType() const
{
    return m_messageType;
}

std::string PluginResult::getMessage() const
{
    if (m_encodeMessage.empty()) {
        return m_strMessage;
    }
    return m_encodeMessage;
}

int PluginResult::getMultipartMessagesSize() const
{
    return m_multipartMessages.size();
}

const PluginResult& PluginResult::getMultipartMessage(int index) const
{
    return m_multipartMessages[index];
}

std::string PluginResult::getStrMessage() const
{
    return m_strMessage;
}

bool PluginResult::getKeepCallback() const
{
    return m_keepCallback;
}

std::string PluginResult::getJSONString()  const
{
    cJSON* object = cJSON_CreateObject();
    cJSON_AddNumberToObject(object, "status", m_status);
    cJSON_AddStringToObject(object, "message", getMessage().c_str());
    cJSON_AddBoolToObject(object, "keepCallback", m_keepCallback);
    std::string ret = cJSON_Print(object);
    cJSON_Delete(object);
    return ret;
}
