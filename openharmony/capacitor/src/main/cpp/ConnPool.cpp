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
 * distributed under the License is distribu    ted on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ConnPool.h"
#include "CallbackContext.h"
#include "HttpCode.h"
#include "HttpUrl.h"
#include "Application.h"
#include "FileCache.h"
#include <zlib.h>

ConnPool::ConnPool(std::string strDomain) { m_strDomain = strDomain; }

ConnPool::~ConnPool() {}

bool ConnPool::getSSL(SSL **ssl, const int nPort, const int nConnectTimeOut, int nTLSVersion,
                      const std::string strCAFile, const std::string strChain, const std::string strPKey)
{
    *ssl = nullptr;
    bool isConnected = false;
    int nSocket = -1;
    if (!CSocket::socket(nSocket)) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSocket::socket(nSocket) == false");
        return false;
    }
    struct timeval tv;
    tv.tv_sec = nConnectTimeOut;
    tv.tv_usec = 0;
    CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    int i = 0;
    for (; i < ATTEMPT_CONNECT_SERVER_TIMES; i++) {
        if (!CSocket::connect(nSocket, m_strDomain.c_str(), nPort)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::connect(nSocket, m_strUrl.c_str(){%{public}s}, nPort(%{public}d)) == false",
                         m_strDomain.c_str(), nPort);
            continue;
        }
        break;
    }
    if (i >= ATTEMPT_CONNECT_SERVER_TIMES) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "Failed to connect to the server after 5 attempts");
        return false;
    }

    int nRet = 0;
    if (!CSocket::fcntl_Int(nRet, nSocket, F_GETFL, 0)) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "CSocket::fcntl_Int(nRet, nSocket, F_GETFL, 0) == false");
        return false;
    }
    if ((nRet & O_NONBLOCK) != 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "CSocket::fcntl_Int(nRet, nSocket, F_GETFL, 0) == false");
    }

    int nValue = 1;
    if (!CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_KEEPALIVE, &nValue, sizeof(nValue))) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "CSocket::SetSockOpt(nSocket, SOL_SOCKET, SO_KEEPALIVE, &nValue, sizeof(nValue)) == false");
        return false;
    }
    int isVerify = 0;
    if (!strCAFile.empty()) {
        isVerify = 1;
    }
    SSL_CTX *ctx = CSSLSocket::sslCTXInit(false, isVerify, nTLSVersion, 0);
    char *pCAFile = nullptr;
    if (!strCAFile.empty()) {
        pCAFile = const_cast<char *>(strCAFile.c_str());
    }
    char *pChain = nullptr;
    if (!strChain.empty()) {
        pChain = const_cast<char *>(strChain.c_str());
    }
    char *pKey = nullptr;
    if (!strPKey.empty()) {
        pKey = const_cast<char *>(strPKey.c_str());
    }

    if (!CSSLSocket::sslCTXSetCertificate(ctx, pCAFile, pChain, pKey)) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
            "CSSLSocket::sslCTXSetCertificate(ctx, pCAFile{%{public}s}, pChain{%{public}s}, pKey{%{public}s}) == false",
            strCAFile.c_str(), strChain.c_str(), strPKey.c_str());
        return false;
    }

    std::string strBaseDir = Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1);
    char szBuf[1024];
    snprintf(szBuf, sizeof(szBuf), "%s/base/files/%s.pem", strBaseDir.c_str(), m_strDomain.c_str());
    CSSLSocket::sslCTXSetClientSessionSavePathName(ctx, szBuf);
    *ssl = CSSLSocket::sslInit(ctx, nSocket, false, nConnectTimeOut);
    CSSLSocket::sslSetClientSession(*ssl);
    SSL_set_tlsext_host_name(*ssl, m_strDomain.c_str());
    isConnected = CSSLSocket::sslConnect(*ssl);
    return isConnected;
}

bool ConnPool::getSocket(int &nSocket, const int nPort, const int nConnectTimeOut)
{
    if (!CSocket::socket(nSocket)) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSocket::Socket(nSocket) == false");
        return false;
    }

    struct timeval tv;
    tv.tv_sec = nConnectTimeOut;
    tv.tv_usec = 0;
    CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    int i = 0;
    for (; i < ATTEMPT_CONNECT_SERVER_TIMES; i++) {
        if (!CSocket::connect(nSocket, m_strDomain.c_str(), nPort)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::Connect(nSocket, m_strUrl.c_str(){%{public}s}, nPort(%{public}d)) == false",
                         m_strDomain.c_str(), nPort);
            continue;
        }
        break;
    }
    if (i >= ATTEMPT_CONNECT_SERVER_TIMES) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "Failed to connect to the server after 5 attempts");
        return false;
    }

    int nRet = 0;
    if (!CSocket::fcntl_Int(nRet, nSocket, F_GETFL, 0)) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "CSocket::fcntl_Int(nRet, nSocket, F_GETFL, 0) == false");
        return false;
    }
    if ((nRet & O_NONBLOCK) != 0) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "CSocket::Fcntl_Int(nRet, nSocket, F_GETFL, 0) == false");
    }
    int nValue = 1;
    if (!CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_KEEPALIVE, &nValue, sizeof(nValue))) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                     "CSocket::SetSockOpt(nSocket, SOL_SOCKET, SO_KEEPALIVE, &nValue, sizeof(nValue)) == false");
        return false;
    }
    return true;
}

void ConnPool::freeSocket(SSL *ssl, const bool isReconnect)
{
    if (ssl && isReconnect) {
        int nSocket = SSL_get_fd(ssl);
        if (!CSSLSocket::sslFree(ssl)) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "CSSLSocket::sslFree(ssl) == false");
        }
        if (!CSocket::close(nSocket)) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::close(nSocket{%{public}d}) == false", nSocket);
        }
    }
}

void ConnPool::freeSocket(int nSocket, const bool isReconnect)
{
    if (nSocket != -1 && isReconnect) {
        if (!CSocket::close(nSocket)) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "CSocket::close(nSocket) == false");
        }
    }
}

bool ConnPool::sendWithTimeOut(SSL *ssl, const std::string strRequest, std::atomic<bool> &isAbort,
                               const int nSendTimeOut)
{
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }
    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }
        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                         nSendTimeOut);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Sending, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    return true;
}

bool ConnPool::sendWithTimeOut(int nSocket, const std::string strRequest, std::atomic<bool> &isAbort,
                               const int nSendTimeOut)
{
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }
    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                         nSendTimeOut);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Sending, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    return true;
}

bool ConnPool::uploadFileWithTimeOut(SSL *ssl, const std::string strRequest, const std::string strTailParams,
                                     const std::string strFilePath, void *pMaxBuffer, const int nBufSize,
                                     std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut)
{
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }

    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                         nSendTimeOut);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    const int nFileSize = static_cast<int>(FileCache::getFileSize(strFilePath));
    const int nTotalSize = strRequest.length() + nFileSize + strTailParams.length();
    int nTotalSendSize = strRequest.length();
    if (pCbc != nullptr) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
        cJSON_AddNumberToObject(json, "total", nTotalSize);
        cJSON_AddTrueToObject(json, "lengthComputable");
        PluginResult pluginResult(PluginResult::OK, json);
        pluginResult.setKeepCallback(true);
        ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
        cJSON_Delete(json);
    }

    FILE *pf = FileCache::openFile(strFilePath, "");
    int consumed = 0;
    while (true) {
        int ret = FileCache::readFile(pf, reinterpret_cast<unsigned char *>(pMaxBuffer), nBufSize, consumed);
        if (ret == 0) {
            break;
        }
        consumed += ret;

        pBuf = static_cast<char *>(pMaxBuffer);
        nSendSize = ret;
        nWritePos = 0;
        while (true) {
            struct pollfd pfd[1];
            pfd[0].fd = nSocket;
            pfd[0].events = POLLOUT | POLLERR;
            pfd[0].revents = 0;
            int nRet = 0;
            if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if ((pfd[0].revents & POLLERR) != 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                return false;
            }

            if (nRet == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                             POLL_SEND_TIME_OUT);
                return false;
            }
            if (isAbort) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                             "User canceled Uploading, isAbort is true");
                return false;
            }
            if ((pfd[0].revents & POLLOUT) != 0) {
                int nWriteSize = nSendSize - nWritePos;
                if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                    return false;
                }
                if (nWriteSize == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                    return false;
                }
                nWritePos += nWriteSize;
                nTotalSendSize += nWriteSize;
                if (pCbc != nullptr) {
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                    cJSON_AddNumberToObject(json, "total", nTotalSize);
                    cJSON_AddTrueToObject(json, "lengthComputable");
                    PluginResult pluginResult(PluginResult::OK, json);
                    pluginResult.setKeepCallback(true);
                    ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                    cJSON_Delete(json);
                }
                if (nWritePos == nSendSize) {
                    break;
                }
            }
        }

        if (consumed >= nFileSize) {
            break;
        }
    }
    FileCache::closeFile(pf);

    nSendSize = strTailParams.length();
    pBuf = strTailParams.c_str();
    nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}d)) == false", POLL_SEND_TIME_OUT);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms)",
                         POLL_SEND_TIME_OUT);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            nTotalSendSize += nWriteSize;
            if (pCbc != nullptr) {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                cJSON_AddNumberToObject(json, "total", nTotalSize);
                cJSON_AddTrueToObject(json, "lengthComputable");
                PluginResult pluginResult(PluginResult::OK, json);
                pluginResult.setKeepCallback(true);
                ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                cJSON_Delete(json);
            }
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    return true;
}

bool ConnPool::uploadFileWithTimeOut(int nSocket, const std::string strRequest, const std::string strTailParams,
                                     const std::string strFilePath, void *pMaxBuffer, const int nBufSize,
                                     std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut)
{
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }

    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                         nSendTimeOut);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    const int nFileSize = static_cast<int>(FileCache::getFileSize(strFilePath));
    const int nTotalSize = strRequest.length() + nFileSize + strTailParams.length();
    int nTotalSendSize = strRequest.length();
    if (pCbc != nullptr) {
        cJSON *json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
        cJSON_AddNumberToObject(json, "total", nTotalSize);
        cJSON_AddTrueToObject(json, "lengthComputable");
        PluginResult pluginResult(PluginResult::OK, json);
        pluginResult.setKeepCallback(true);
        ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
        cJSON_Delete(json);
    }

    FILE *pf = FileCache::openFile(strFilePath, "");
    int consumed = 0;
    while (true) {
        int ret = FileCache::readFile(pf, reinterpret_cast<unsigned char *>(pMaxBuffer), nBufSize, consumed);
        if (ret == 0) {
            break;
        }
        consumed += ret;

        pBuf = static_cast<char *>(pMaxBuffer);
        nSendSize = ret;
        nWritePos = 0;
        while (true) {
            struct pollfd pfd[1];
            pfd[0].fd = nSocket;
            pfd[0].events = POLLOUT | POLLERR;
            pfd[0].revents = 0;
            int nRet = 0;
            if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if ((pfd[0].revents & POLLERR) != 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                return false;
            }

            if (nRet == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                             POLL_SEND_TIME_OUT);
                return false;
            }
            if (isAbort) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                             "User canceled Uploading, isAbort is true");
                return false;
            }
            if ((pfd[0].revents & POLLOUT) != 0) {
                int nWriteSize = nSendSize - nWritePos;
                if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                    return false;
                }
                if (nWriteSize == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                    return false;
                }
                nWritePos += nWriteSize;
                nTotalSendSize += nWriteSize;
                if (pCbc != nullptr) {
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                    cJSON_AddNumberToObject(json, "total", nTotalSize);
                    cJSON_AddTrueToObject(json, "lengthComputable");
                    PluginResult pluginResult(PluginResult::OK, json);
                    pluginResult.setKeepCallback(true);
                    ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                    cJSON_Delete(json);
                }
                if (nWritePos == nSendSize) {
                    break;
                }
            }
        }

        if (consumed >= nFileSize) {
            break;
        }
    }
    FileCache::closeFile(pf);

    nSendSize = strTailParams.length();
    pBuf = strTailParams.c_str();
    nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}d)) == false", POLL_SEND_TIME_OUT);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms)",
                         POLL_SEND_TIME_OUT);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            nTotalSendSize += nWriteSize;
            if (pCbc != nullptr) {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                cJSON_AddNumberToObject(json, "total", nTotalSize);
                cJSON_AddTrueToObject(json, "lengthComputable");
                PluginResult pluginResult(PluginResult::OK, json);
                pluginResult.setKeepCallback(true);
                ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                cJSON_Delete(json);
            }
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    return true;
}

bool ConnPool::uploadFileWithTimeOuts(SSL *ssl, const std::string strRequest, const std::string strTailParams,
                                      const std::vector<std::string> &vecRequestParams,
                                      const std::vector<std::string> &vecFilePath, void *pMaxBuffer, const int nBufSize,
                                      std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut)
{
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }

    int nTotalSize = 0;
    if (pCbc != nullptr) {
        nTotalSize += strRequest.length();
        for (int i = 0; i < vecRequestParams.size(); i++) {
            nTotalSize += vecRequestParams[i].length();
            nTotalSize += FileCache::getFileSize(vecFilePath[i]);
        }
        nTotalSize += strTailParams.length();
    }

    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                         nSendTimeOut);
            return false;
        }

        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    int nContentSize = 0;
    int nAlreadyWriteSize = 0;
    int nTotalSendSize = strRequest.length();
    for (int i = 0; i < vecRequestParams.size(); i++) {
        int nSendSize = vecRequestParams[i].length();
        nContentSize += nSendSize;
        const char *pBuf = vecRequestParams[i].c_str();
        int nWritePos = 0;
        while (true) {
            struct pollfd pfd[1];
            pfd[0].fd = nSocket;
            pfd[0].events = POLLOUT | POLLERR;
            pfd[0].revents = 0;
            int nRet = 0;
            if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if ((pfd[0].revents & POLLERR) != 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                return false;
            }

            if (nRet == 0) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if (isAbort) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                             "User canceled Uploading, isAbort is true");
                return false;
            }

            if ((pfd[0].revents & POLLOUT) != 0) {
                int nWriteSize = nSendSize - nWritePos;
                if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                    return false;
                }
                if (nWriteSize == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                    return false;
                }
                nWritePos += nWriteSize;
                nAlreadyWriteSize += nWriteSize;
                if (nWritePos == nSendSize) {
                    break;
                }
            }
        }

        const int nFileSize = (int)FileCache::getFileSize(vecFilePath[i]);
        nContentSize += nFileSize;
        if (pCbc != nullptr) {
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
            cJSON_AddNumberToObject(json, "total", nTotalSize);
            cJSON_AddTrueToObject(json, "lengthComputable");
            PluginResult pluginResult(PluginResult::OK, json);
            pluginResult.setKeepCallback(true);
            ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
            cJSON_Delete(json);
        }

        FILE *pf = FileCache::openFile(vecFilePath[i], "");
        int consumed = 0;
        while (true) {
            int ret = FileCache::readFile(pf, static_cast<unsigned char *>(pMaxBuffer), nBufSize - 2,
                                          consumed); // 减2主要是为了在文件末尾加上\r\n
            if (ret == 0) {
                break;
            }
            consumed += ret;
            if (consumed >= nFileSize) {
                std::copy_n("\r\n", 2, static_cast<unsigned char *>(pMaxBuffer) + ret);
                ret += 2;
                nContentSize += 2;
            }
            pBuf = static_cast<char *>(pMaxBuffer);
            nSendSize = ret;
            nWritePos = 0;
            while (true) {
                struct pollfd pfd[1];
                pfd[0].fd = nSocket;
                pfd[0].events = POLLOUT | POLLERR;
                pfd[0].revents = 0;
                int nRet = 0;
                if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                                 POLL_SEND_TIME_OUT);
                    return false;
                }

                if ((pfd[0].revents & POLLERR) != 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                    return false;
                }

                if (nRet == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                                 POLL_SEND_TIME_OUT);
                    return false;
                }
                if (isAbort) {
                    OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                                 "User canceled Uploading, isAbort is true");
                    return false;
                }
                if ((pfd[0].revents & POLLOUT) != 0) {
                    int nWriteSize = nSendSize - nWritePos;
                    if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                     "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                        return false;
                    }
                    if (nWriteSize == 0) {
                        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                        return false;
                    }
                    nWritePos += nWriteSize;
                    nAlreadyWriteSize += nWriteSize;
                    nTotalSendSize += nWriteSize;
                    if (pCbc != nullptr) {
                        cJSON *json = cJSON_CreateObject();
                        cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                        cJSON_AddNumberToObject(json, "total", nTotalSize);
                        cJSON_AddTrueToObject(json, "lengthComputable");
                        PluginResult pluginResult(PluginResult::OK, json);
                        pluginResult.setKeepCallback(true);
                        ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                        cJSON_Delete(json);
                    }
                    if (nWritePos == nSendSize) {
                        break;
                    }
                }
            }

            if (consumed >= nFileSize) {
                break;
            }
        }
        FileCache::closeFile(pf);
    }

    nSendSize = strTailParams.length();
    nContentSize += nSendSize;
    pBuf = strTailParams.c_str();
    nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}d)) == false", POLL_SEND_TIME_OUT);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms)",
                         POLL_SEND_TIME_OUT);
            return false;
        }

        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            nAlreadyWriteSize += nWriteSize;
            nTotalSendSize += nWriteSize;
            if (pCbc != nullptr) {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                cJSON_AddNumberToObject(json, "total", nTotalSize);
                cJSON_AddTrueToObject(json, "lengthComputable");
                PluginResult pluginResult(PluginResult::OK, json);
                pluginResult.setKeepCallback(true);
                ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                cJSON_Delete(json);
            }
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    if (nContentSize != nAlreadyWriteSize) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                     "nContentSize:%{public}d != nAlreadyWriteSize:%{public}d", nContentSize, nAlreadyWriteSize);
        return false;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    for (int i = 0; i < vecRequestParams.size(); i++) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", vecRequestParams[i].c_str());
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strTailParams.c_str());
    return true;
}

bool ConnPool::uploadFileWithTimeOuts(int nSocket, const std::string strRequest, const std::string strTailParams,
                                      const std::vector<std::string> &vecRequestParams,
                                      const std::vector<std::string> &vecFilePath, void *pMaxBuffer, const int nBufSize,
                                      std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut)
{
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }

    int nTotalSize = 0;
    if (pCbc != nullptr) {
        nTotalSize += strRequest.length();
        for (int i = 0; i < vecRequestParams.size(); i++) {
            nTotalSize += vecRequestParams[i].length();
            nTotalSize += FileCache::getFileSize(vecFilePath[i]);
        }
        nTotalSize += strTailParams.length();
    }

    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                         nSendTimeOut);
            return false;
        }

        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }

        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    int nContentSize = 0;
    int nAlreadyWriteSize = 0;
    int nTotalSendSize = strRequest.length();
    for (int i = 0; i < vecRequestParams.size(); i++) {
        int nSendSize = vecRequestParams[i].length();
        nContentSize += nSendSize;
        const char *pBuf = vecRequestParams[i].c_str();
        int nWritePos = 0;
        while (true) {
            struct pollfd pfd[1];
            pfd[0].fd = nSocket;
            pfd[0].events = POLLOUT | POLLERR;
            pfd[0].revents = 0;
            int nRet = 0;
            if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if ((pfd[0].revents & POLLERR) != 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                return false;
            }

            if (nRet == 0) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if (isAbort) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                             "User canceled Uploading, isAbort is true");
                return false;
            }

            if ((pfd[0].revents & POLLOUT) != 0) {
                int nWriteSize = nSendSize - nWritePos;
                if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                    return false;
                }
                if (nWriteSize == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                    return false;
                }
                nWritePos += nWriteSize;
                nAlreadyWriteSize += nWriteSize;
                if (nWritePos == nSendSize) {
                    break;
                }
            }
        }

        const int nFileSize = (int)FileCache::getFileSize(vecFilePath[i]);
        nContentSize += nFileSize;
        if (pCbc != nullptr) {
            cJSON *json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
            cJSON_AddNumberToObject(json, "total", nTotalSize);
            cJSON_AddTrueToObject(json, "lengthComputable");
            PluginResult pluginResult(PluginResult::OK, json);
            pluginResult.setKeepCallback(true);
            ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
            cJSON_Delete(json);
        }

        FILE *pf = FileCache::openFile(vecFilePath[i], "");
        int consumed = 0;
        while (true) {
            int ret = FileCache::readFile(pf, static_cast<unsigned char *>(pMaxBuffer), nBufSize - 2,
                                          consumed); // 减2主要是为了在文件末尾加上\r\n
            if (ret == 0) {
                break;
            }
            consumed += ret;
            if (consumed >= nFileSize) {
                std::copy_n("\r\n", 2, static_cast<unsigned char *>(pMaxBuffer) + ret);
                ret += 2;
                nContentSize += 2;
            }
            pBuf = static_cast<char *>(pMaxBuffer);
            nSendSize = ret;
            nWritePos = 0;
            while (true) {
                struct pollfd pfd[1];
                pfd[0].fd = nSocket;
                pfd[0].events = POLLOUT | POLLERR;
                pfd[0].revents = 0;
                int nRet = 0;
                if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                                 POLL_SEND_TIME_OUT);
                    return false;
                }

                if ((pfd[0].revents & POLLERR) != 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                    return false;
                }

                if (nRet == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                                 POLL_SEND_TIME_OUT);
                    return false;
                }
                if (isAbort) {
                    OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                                 "User canceled Uploading, isAbort is true");
                    return false;
                }
                if ((pfd[0].revents & POLLOUT) != 0) {
                    int nWriteSize = nSendSize - nWritePos;
                    if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                     "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                        return false;
                    }
                    if (nWriteSize == 0) {
                        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                        return false;
                    }
                    nWritePos += nWriteSize;
                    nAlreadyWriteSize += nWriteSize;
                    nTotalSendSize += nWriteSize;
                    if (pCbc != nullptr) {
                        cJSON *json = cJSON_CreateObject();
                        cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                        cJSON_AddNumberToObject(json, "total", nTotalSize);
                        cJSON_AddTrueToObject(json, "lengthComputable");
                        PluginResult pluginResult(PluginResult::OK, json);
                        pluginResult.setKeepCallback(true);
                        ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                        cJSON_Delete(json);
                    }
                    if (nWritePos == nSendSize) {
                        break;
                    }
                }
            }

            if (consumed >= nFileSize) {
                break;
            }
        }
        FileCache::closeFile(pf);
    }

    nSendSize = strTailParams.length();
    nContentSize += nSendSize;
    pBuf = strTailParams.c_str();
    nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}d)) == false", POLL_SEND_TIME_OUT);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms)",
                         POLL_SEND_TIME_OUT);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSocket::write(nSocket, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::write(nSocket, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            nAlreadyWriteSize += nWriteSize;
            nTotalSendSize += nWriteSize;
            if (pCbc != nullptr) {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "loaded", nTotalSendSize);
                cJSON_AddNumberToObject(json, "total", nTotalSize);
                cJSON_AddTrueToObject(json, "lengthComputable");
                PluginResult pluginResult(PluginResult::OK, json);
                pluginResult.setKeepCallback(true);
                ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                cJSON_Delete(json);
            }
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    if (nContentSize != nAlreadyWriteSize) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                     "nContentSize:%{public}d != nAlreadyWriteSize:%{public}d", nContentSize, nAlreadyWriteSize);
        return false;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    for (int i = 0; i < vecRequestParams.size(); i++) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", vecRequestParams[i].c_str());
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strTailParams.c_str());
    return true;
}

bool ConnPool::uploadFileWithTimeOut(SSL *ssl, const std::string url, const std::string strRequest, const std::string strTailParams,
                                     const std::string strFilePath, void *pMaxBuffer, const int nBufSize,
                                     std::atomic<bool> &isAbort, UploadCallback pCallback, const int nSendTimeOut)
{
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }

    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::Poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                         nSendTimeOut);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::SSLWrite(ssl, pBuf+nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize)
                break;
        }
    }

    const int nFileSize = (int)FileCache::getFileSize(strFilePath);
    const int nTotalSize = strRequest.length() + nFileSize + strTailParams.length();
    int nTotalSendSize = strRequest.length();
    if (pCallback != NULL) {
        pCallback("upload", url, nTotalSendSize, nTotalSize);
    }

    FILE *pf = FileCache::openFile(strFilePath, "");
    int consumed = 0;
    while (true) {
        int ret = FileCache::readFile(pf, (unsigned char *)pMaxBuffer, nBufSize, consumed);
        if (ret == 0) {
            break;
        }
        consumed += ret;

        pBuf = (char *)pMaxBuffer;
        nSendSize = ret;
        nWritePos = 0;
        while (true) {
            struct pollfd pfd[1];
            pfd[0].fd = nSocket;
            pfd[0].events = POLLOUT | POLLERR;
            pfd[0].revents = 0;
            int nRet = 0;
            if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::Poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if ((pfd[0].revents & POLLERR) != 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                return false;
            }

            if (nRet == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                             POLL_SEND_TIME_OUT);
                return false;
            }
            if (isAbort) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                             "User canceled Uploading, isAbort is true");
                return false;
            }
            if ((pfd[0].revents & POLLOUT) != 0) {
                int nWriteSize = nSendSize - nWritePos;
                if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSSLSocket::SSLWrite(ssl, pBuf+nWritePos, nWriteSize) == false");
                    return false;
                }
                if (nWriteSize == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                    return false;
                }
                nWritePos += nWriteSize;
                nTotalSendSize += nWriteSize;
                if (pCallback != NULL) {
                    pCallback("upload", url, nTotalSendSize, nTotalSize);
                }
                if (nWritePos == nSendSize)
                    break;
            }
        }

        if (consumed >= nFileSize) {
            break;
        }
    }
    FileCache::closeFile(pf);

    nSendSize = strTailParams.length();
    pBuf = strTailParams.c_str();
    nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::Poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}d)) == false", POLL_SEND_TIME_OUT);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms)",
                         POLL_SEND_TIME_OUT);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::SSLWrite(ssl, pBuf+nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            nTotalSendSize += nWriteSize;
            if (pCallback != NULL) {
                pCallback("upload", url, nTotalSendSize, nTotalSize);
            }
            if (nWritePos == nSendSize)
                break;
        }
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    return true;
}

bool ConnPool::uploadFileWithTimeOut(int nSocket, const std::string url, const std::string strRequest, const std::string strTailParams, const std::string strFilePath, void* pMaxBuffer, const int nBufSize, std::atomic<bool>& isAbort, UploadCallback pCallback, const int nSendTimeOut)
{
    if(nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }
    
    int nSendSize = strRequest.length();
    const char* pBuf = strRequest.c_str();
    int nWritePos = 0;
    while(true) {
       struct pollfd pfd[1];
       pfd[0].fd = nSocket;
       pfd[0].events = POLLOUT|POLLERR;
       pfd[0].revents = 0;
       int nRet =0;
       if(!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSocket::Poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",nSendTimeOut);
            return false;
       }
            
       if((pfd[0].revents & POLLERR) != 0) {    
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
       }
            
       if(nRet == 0) {
           OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms", nSendTimeOut);
           return false;
       }
       if(isAbort) {
           OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
           return false;
       } 
       if((pfd[0].revents & POLLOUT) != 0) {
           int nWriteSize = nSendSize - nWritePos;
           if(!CSocket::write(nSocket, pBuf+nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSSLSocket::SSLWrite(ssl, pBuf+nWritePos, nWriteSize) == false");
                return false;
           }
           if(nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
           }
           nWritePos += nWriteSize;
           if(nWritePos == nSendSize)
               break;
       }
    }
    
    const int nFileSize = (int)FileCache::getFileSize(strFilePath);
    const int nTotalSize = strRequest.length()+nFileSize+strTailParams.length();
    int nTotalSendSize = strRequest.length();
    if(pCallback != NULL) {
        pCallback("upload",url,nTotalSendSize,nTotalSize);
    }   
    
    FILE* pf = FileCache::openFile(strFilePath, "");
    int consumed = 0;
    while(true) {
        int ret = FileCache::readFile(pf, (unsigned char*)pMaxBuffer, nBufSize, consumed);
        if (ret == 0) {
            break;
        }
        consumed += ret;
        
        pBuf = (char*)pMaxBuffer;
        nSendSize = ret;
        nWritePos = 0;
        while(true) {
           struct pollfd pfd[1];
           pfd[0].fd = nSocket;
           pfd[0].events = POLLOUT|POLLERR;
           pfd[0].revents = 0;
           int nRet =0;
           if(!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSocket::Poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", POLL_SEND_TIME_OUT);
                return false;
           }
                
           if((pfd[0].revents & POLLERR) != 0) {    
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                return false;
           }
                
           if(nRet == 0) {
               OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",POLL_SEND_TIME_OUT);
               return false;
           }
           if(isAbort) {
               OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
               return false;
           }      
           if((pfd[0].revents & POLLOUT) != 0) {
               int nWriteSize = nSendSize - nWritePos;
               if(!CSocket::write(nSocket, pBuf+nWritePos, nWriteSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSSLSocket::SSLWrite(ssl, pBuf+nWritePos, nWriteSize) == false");
                    return false;
               }
               if(nWriteSize == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                    return false;
               }
               nWritePos += nWriteSize;
               nTotalSendSize += nWriteSize;
               if(pCallback != NULL) {
                    pCallback("upload",url,nTotalSendSize,nTotalSize);
               }   
               if(nWritePos == nSendSize)
                   break;
           }
        }
        
        if(consumed >= nFileSize) {
            break;
        }
    }
    FileCache::closeFile(pf);
    
    nSendSize = strTailParams.length();
    pBuf = strTailParams.c_str();
    nWritePos = 0;
    while(true) {
       struct pollfd pfd[1];
       pfd[0].fd = nSocket;
       pfd[0].events = POLLOUT|POLLERR;
       pfd[0].revents = 0;
       int nRet =0;
       if(!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSocket::Poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}d)) == false",POLL_SEND_TIME_OUT);
            return false;
       }
            
       if((pfd[0].revents & POLLERR) != 0) {    
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
       }
            
       if(nRet == 0) {
           OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms)",POLL_SEND_TIME_OUT);
           return false;
       }
       if(isAbort) {
           OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled Uploading, isAbort is true");
           return false;
       }     
       if((pfd[0].revents & POLLOUT) != 0) {
           int nWriteSize = nSendSize - nWritePos;
           if(!CSocket::write(nSocket, pBuf+nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "CSSLSocket::SSLWrite(ssl, pBuf+nWritePos, nWriteSize) == false");
                return false;
           }
           if(nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
           }
           nWritePos += nWriteSize;
           nTotalSendSize += nWriteSize;
           if(pCallback != NULL) {
                pCallback("upload",url,nTotalSendSize,nTotalSize);
           }   
           if(nWritePos == nSendSize)
               break;
       }
    }
    
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    return true;
}

bool ConnPool::recvHttpWithTimeOut(SSL *ssl, const std::string &strMethod, std::string &strFilePath,
                                   std::map<std::string, std::string> &mapHeaders, const std::string &strDirectory,
                                   void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                                   void *pCbc, const int nRecvTimeOut)
{
    isClose = true;
    CMemPool *pMemPool = (CMemPool *)Application::g_memPool;
    std::vector<SMemPage> vecMemPage;
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "recv data without socket");
        return false;
    }

    bool isReceiveOk = false;
    char *szBuf = static_cast<char *>(pMaxBuffer);
    std::fill(szBuf, szBuf + nBufSize, 0);
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLIN | POLLERR;
        pfd[0].revents = 0;
        int nRetCount = 0;
        if (!CSocket::poll(nRetCount, pfd, 1, nRecvTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRetCount, pfd, 1, POLL_RECV_TIME_OUT(%{public}dms)) == false",
                         POLL_RECV_TIME_OUT);
            return false;
        }

        SSL *pSsl[1];
        pSsl[0] = ssl;
        SSL *rSsl[1];
        int nHasCount = 1;
        CSSLSocket::sslHasPending(pSsl, rSsl, nHasCount);

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRetCount == 0 && nHasCount == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                         "recv data time out %{public}dms nRetCount == 0 && nHasCount == 0", nRecvTimeOut);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled receiving, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLIN) != 0 || nHasCount > 0) {
            int nRecvSize = nBufSize;
            bool bRetResult = CSSLSocket::sslRead(ssl, szBuf, nRecvSize, isClose);
            if (nRecvSize <= 0) {
                break;
            }
            if (nRecvSize > 0) {
                if (!pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize)) {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                 "pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize) == false");
                    return false;
                }

                int nHttpHeader = getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                if (nHttpHeader > 0) {
                    int nContentLength = getContentLength(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                    if (nContentLength == -1) {
                        std::vector<unsigned char> vecLastFiveBytes;
                        // 这里只处理单一数据块的，例如：Range:0-XXX的，不处理Range:0-XXX,XXX-XXX,XXX-XXX的多个数据块内容，如果您的请求是多个数据块内容，请调整您的请求方案，如果一定要处理多个数据块，联系开发者或者自己定制研发，因为这种情况及其少用
                        if (getTransferEncodingChunked(vecMemPage[0].m_point, vecMemPage[0].m_nCount) &&
                            pMemPool->getLastFiveBytes(vecMemPage, vecLastFiveBytes)) {
                            if (vecLastFiveBytes[0] == '0' && vecLastFiveBytes[1] == '\r' &&
                                vecLastFiveBytes[2] == '\n' && vecLastFiveBytes[3] == '\r' &&
                                vecLastFiveBytes[4] == '\n') {
                                modifyHttpBodyByTransferEncodingChunked(vecMemPage);
                                isReceiveOk = true;
                            }
                        }
                        if (pCbc != nullptr) {
                            int totalLength = pMemPool->getMemPageUseLength(vecMemPage);
                            cJSON *json = cJSON_CreateObject();
                            cJSON_AddNumberToObject(json, "loaded", totalLength);
                            cJSON_AddNumberToObject(json, "total", totalLength);
                            cJSON_AddFalseToObject(json, "lengthComputable");
                            PluginResult pluginResult(PluginResult::OK, json);
                            pluginResult.setKeepCallback(true);
                            ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                            cJSON_Delete(json);
                        }
                    } else {
                        int totalLength = pMemPool->getMemPageUseLength(vecMemPage);
                        if (totalLength - nHttpHeader >= nContentLength) {
                            isReceiveOk = true;
                        }
                        if (pCbc != nullptr) {
                            cJSON *json = cJSON_CreateObject();
                            cJSON_AddNumberToObject(json, "loaded", totalLength);
                            cJSON_AddNumberToObject(json, "total", nContentLength);
                            cJSON_AddTrueToObject(json, "lengthComputable");
                            PluginResult pluginResult(PluginResult::OK, json);
                            pluginResult.setKeepCallback(true);
                            ((CallbackContext *)pCbc)->sendPluginResult(pluginResult);
                            cJSON_Delete(json);
                        }
                    }

                    if (isWithoutHttpBody(strMethod, vecMemPage[0].m_point, vecMemPage[0].m_nCount)) {
                        isReceiveOk = true;
                    }
                }
            }

            if (isReceiveOk) {
                break;
            }

            int nShutdown = SSL_get_shutdown(ssl);
            if ((nShutdown & SSL_RECEIVED_SHUTDOWN) != 0) {
                OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                             "SSL_get_shutdown(ssl) is SSL_RECEIVED_SHUTDOWN");
                ((CMemPool *)Application::g_memPool)->freePage(vecMemPage);
                return false;
            }

            if (!bRetResult) {
                if (SSL_get_error(ssl, 0) == SSL_ERROR_ZERO_RETURN) {
                    if (!isReceiveOk) {
                        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                     "SSL_get_error(ssl, 0) == SSL_ERROR_ZERO_RETURN and isReceiveOk == false");
                    }
                }
                ((CMemPool *)Application::g_memPool)->freePage(vecMemPage);
                return false;
            }
        }
    }
    if (vecMemPage.size() == 0) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "vecMemPage.size() == 0");
        return true;
    }

    int httpCode = getHttpCode(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
    if (!getHttpHeaders(vecMemPage[0].m_point, vecMemPage[0].m_nCount, mapHeaders)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                     "getHttpHeaders(vecMemPage[0].m_point, vecMemPage[0].m_nCount,mapHeaders) == false");
        return false;
    }

    std::string strRealFileName = "";
    if (mapHeaders.find("Content-Disposition") != mapHeaders.end()) {
        std::string strFileName = mapHeaders["Content-Disposition"];
        if (strFileName.find("filename=") != std::string::npos) {
            strFileName = strFileName.substr(strFileName.find("filename="));
            if (strFileName.find("\"") != std::string::npos) {
                strFileName = strFileName.substr(strFileName.find("\"") + 1);
                if (strFileName.find("\"") != std::string::npos) {
                    strRealFileName = strFileName.substr(0, strFileName.find("\""));
                }
            }
        }
    }
    if (!strRealFileName.empty()) {
        if (strFilePath.find_last_of("/") != std::string::npos) {
            strFilePath = strFilePath.substr(0, strFilePath.find_last_of("/"));
            strFilePath += strRealFileName;
        } else {
            strFilePath = strRealFileName;
        }
    }

    if (strFilePath == "/") {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        char szFileName[128];
        snprintf(szFileName, sizeof(szFileName), "%ld", tv.tv_sec * 1000000 + tv.tv_usec);
        strFilePath = szFileName;
    }

    if (httpCode == HttpCode::OK) {
        FILE *pFd = FileCache::openFile(strFilePath, strDirectory);
        if (pFd != nullptr) {
            int nHeaderLength = getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
            std::string contentEncode = getContentEncoding(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
            if (contentEncode.find("gzip") != std::string::npos) {
                std::vector<unsigned char> vecResponse;
                if (!gzipDecompress(vecMemPage, nHeaderLength, vecResponse)) {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                 "gzipDecompress(vecMemPage, nHeaderLength, vecResponse) == false");
                    return false;
                }
                int nWriteSize =
                    FileCache::writeFile(pFd, reinterpret_cast<char *>(&vecResponse[0]), vecResponse.size());
                if (nWriteSize != vecResponse.size()) {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                 "nWriteSize{%{public}d} != vecResponse.size(){%{public}d}", nWriteSize,
                                 vecResponse.size());
                    return false;
                }
            } else {
                for (int i = 0; i < vecMemPage.size(); i++) {
                    if (i != 0) {
                        nHeaderLength = 0;
                    }
                    int nTotalWriteSize = 0;
                    while (vecMemPage[i].m_nCount - nTotalWriteSize - nHeaderLength > 0) {
                        int nWriteSize =
                            FileCache::writeFile(pFd, vecMemPage[i].m_point + nHeaderLength + nTotalWriteSize,
                                                 vecMemPage[i].m_nCount - nTotalWriteSize - nHeaderLength);
                        nTotalWriteSize += nWriteSize;
                    }
                }
            }
            FileCache::closeFile(pFd);
        }
    }
    std::fill(static_cast<char *>(pMaxBuffer), static_cast<char *>(pMaxBuffer) + nBufSize, 0);
    char *pEnd = strstr(vecMemPage[0].m_point, "\r\n\r\n");
    if (pEnd == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                     "strstr(vecMemPage[0].m_point, \r\n\r\n) == nullptr");
        return true;
    }
    std::copy_n(vecMemPage[0].m_point, pEnd - vecMemPage[0].m_point + 4, static_cast<char *>(pMaxBuffer));
    ((CMemPool *)Application::g_memPool)->freePage(vecMemPage);
    return true;
}

bool ConnPool::recvHttpWithTimeOut(const int nSocket, const std::string &strMethod, std::string &strFilePath,
                                   std::map<std::string, std::string> &mapHeaders, const std::string &strDirectory,
                                   void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                                   void *pCbc, const int nRecvTimeOut)
{
    isClose = true;
    CMemPool *pMemPool = (CMemPool *)Application::g_memPool;
    std::vector<SMemPage> vecMemPage;
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "recv data without socket");
        return false;
    }

    bool isReceiveOk = false;
    char *szBuf = static_cast<char *>(pMaxBuffer);
    std::fill(szBuf, szBuf + nBufSize, 0);
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLIN | POLLERR;
        pfd[0].revents = 0;
        int nRetCount = 0;
        if (!CSocket::poll(nRetCount, pfd, 1, nRecvTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRetCount, pfd, 1, POLL_RECV_TIME_OUT(%{public}dms)) == false", nRecvTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRetCount == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                         "recv data time out %{public}dms nRetCount == 0", nRecvTimeOut);
            return false;
        }

        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled receiving, isAbort is true");
            return false;
        }

        if ((pfd[0].revents & POLLIN) != 0) {
            int nRecvSize = nBufSize;
            bool bRetResult = CSocket::read(nSocket, szBuf, nRecvSize);
            if (nRecvSize <= 0) {
                break;
            }
            if (nRecvSize > 0) {
                if (!pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize)) {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                 "pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize) == false");
                    return false;
                }
                int nHttpHeader = getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                if (nHttpHeader > 0) {
                    int nContentLength = getContentLength(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                    if (nContentLength == -1) {
                        std::vector<unsigned char> vecLastFiveBytes;
                        // 这里只处理单一数据块的，例如：Range:0-XXX的，不处理Range:0-XXX,XXX-XXX,XXX-XXX的多个数据块内容，如果您的请求是多个数据块内容，请调整您的请求方案，如果一定要处理多个数据块，联系开发者或者自己定制研发，因为这种情况及其少用
                        if (getTransferEncodingChunked(vecMemPage[0].m_point, vecMemPage[0].m_nCount) &&
                            pMemPool->getLastFiveBytes(vecMemPage, vecLastFiveBytes)) {
                            if (vecLastFiveBytes[0] == '0' && vecLastFiveBytes[1] == '\r' &&
                                vecLastFiveBytes[2] == '\n' && vecLastFiveBytes[3] == '\r' &&
                                vecLastFiveBytes[4] == '\n') {
                                modifyHttpBodyByTransferEncodingChunked(vecMemPage);
                                isReceiveOk = true;
                            }
                        }
                    } else {
                        int totalLength = pMemPool->getMemPageUseLength(vecMemPage);
                        if (totalLength - nHttpHeader == nContentLength) {
                            isReceiveOk = true;
                        }
                    }

                    if (isWithoutHttpBody(strMethod, vecMemPage[0].m_point, vecMemPage[0].m_nCount)) {
                        isReceiveOk = true;
                    }
                }
            }
            if (isReceiveOk) {
                break;
            }

            if (!bRetResult) {
                ((CMemPool *)Application::g_memPool)->freePage(vecMemPage);
                return false;
            }
        }
    }

    if (vecMemPage.size() == 0) {
        OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "vecMemPage.size() == 0");
        return true;
    }

    int httpCode = getHttpCode(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
    if (!getHttpHeaders(vecMemPage[0].m_point, vecMemPage[0].m_nCount, mapHeaders)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                     "getHttpHeaders(vecMemPage[0].m_point, vecMemPage[0].m_nCount,mapHeaders) == false");
        return false;
    }

    std::string strRealFileName = "";
    if (mapHeaders.find("Content-Disposition") != mapHeaders.end()) {
        std::string strFileName = mapHeaders["Content-Disposition"];
        if (strFileName.find("filename=") != std::string::npos) {
            strFileName = strFileName.substr(strFileName.find("filename="));
            if (strFileName.find("\"") != std::string::npos) {
                strFileName = strFileName.substr(strFileName.find("\"") + 1);
                if (strFileName.find("\"") != std::string::npos) {
                    strRealFileName = strFileName.substr(0, strFileName.find("\""));
                }
            }
        }
    }
    if (!strRealFileName.empty()) {
        if (strFilePath.find_last_of("/") != std::string::npos) {
            strFilePath = strFilePath.substr(0, strFilePath.find_last_of("/"));
            strFilePath += strRealFileName;
        } else {
            strFilePath = strRealFileName;
        }
    }

    if (strFilePath == "/") {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        char szFileName[128];
        snprintf(szFileName, sizeof(szFileName), "%ld", tv.tv_sec * 1000000 + tv.tv_usec);
        strFilePath = szFileName;
    }

    if (httpCode == HttpCode::OK) {
        FILE *pFd = FileCache::openFile(strFilePath, strDirectory);
        if (pFd != nullptr) {
            int nHeaderLength = getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
            std::string contentEncode = getContentEncoding(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
            if (contentEncode.find("gzip") != std::string::npos) {
                std::vector<unsigned char> vecResponse;
                if (!gzipDecompress(vecMemPage, nHeaderLength, vecResponse)) {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                 "gzipDecompress(vecMemPage, nHeaderLength, vecResponse) == false");
                    return false;
                }
                int nWriteSize =
                    FileCache::writeFile(pFd, reinterpret_cast<char *>(&vecResponse[0]), vecResponse.size());
                if (nWriteSize != vecResponse.size()) {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                 "nWriteSize{%{public}d} != vecResponse.size(){%{public}d}", nWriteSize,
                                 vecResponse.size());
                    return false;
                }
            } else {
                for (int i = 0; i < vecMemPage.size(); i++) {
                    if (i != 0) {
                        nHeaderLength = 0;
                    }
                    int nTotalWriteSize = 0;
                    while (vecMemPage[i].m_nCount - nTotalWriteSize - nHeaderLength > 0) {
                        int nWriteSize =
                            FileCache::writeFile(pFd, vecMemPage[i].m_point + nHeaderLength + nTotalWriteSize,
                                                 vecMemPage[i].m_nCount - nTotalWriteSize - nHeaderLength);
                        nTotalWriteSize += nWriteSize;
                    }
                }
            }
            FileCache::closeFile(pFd);
        }
    }

    std::fill(static_cast<char *>(pMaxBuffer), static_cast<char *>(pMaxBuffer) + nBufSize, 0);
    char *pEnd = strstr(vecMemPage[0].m_point, "\r\n\r\n");
    if (pEnd == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                     "strstr(vecMemPage[0].m_point, \r\n\r\n) == nullptr");
        return true;
    }
    std::copy_n(vecMemPage[0].m_point, pEnd - vecMemPage[0].m_point + 4, static_cast<char *>(pMaxBuffer));
    ((CMemPool *)Application::g_memPool)->freePage(vecMemPage);
    return true;
}

bool ConnPool::recvHttpWithTimeOut(SSL *ssl, const std::string &strMethod, std::vector<SMemPage> &vecMemPage,
                                   void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                                   const int nRecvTimeOut)
{
    isClose = true;
    CMemPool *pMemPool = (CMemPool *)Application::g_memPool;
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "recv data without socket");
        return false;
    }

    bool isReceiveOk = false;
    char *szBuf = static_cast<char *>(pMaxBuffer);
    std::fill(szBuf, szBuf + nBufSize, 0);

    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLIN | POLLERR;
        pfd[0].revents = 0;
        int nRetCount = 0;
        if (!CSocket::poll(nRetCount, pfd, 1, nRecvTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRetCount, pfd, 1, POLL_RECV_TIME_OUT(%{public}dms)) == false", nRecvTimeOut);
            return false;
        }

        SSL *pSsl[1];
        pSsl[0] = ssl;
        SSL *rSsl[1];
        int nHasCount = 1;
        CSSLSocket::sslHasPending(pSsl, rSsl, nHasCount);

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRetCount == 0 && nHasCount == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                         "recv time out %{public}dms nRetCount == 0 && nHasCount == 0", nRecvTimeOut);
            return false;
        }

        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled receiving, isAbort is true");
            return false;
        }

        if ((pfd[0].revents & POLLIN) != 0 || nHasCount > 0) {
            int nRecvSize = nBufSize;
            bool bRetResult = CSSLSocket::sslRead(ssl, szBuf, nRecvSize, isClose);
            if (nRecvSize <= 0) {
                break;
            }
            if (nRecvSize > 0) {
                if (!pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize)) {
                    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                 "pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize) == false");
                    return false;
                }

                std::string isAcceptRanges = getAcceptRanges(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                int nHttpHeader = getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                if (nHttpHeader > 0) {
                    int nContentLength = getContentLength(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                    if (nContentLength == -1) {
                        std::vector<unsigned char> vecLastFiveBytes;
                        // 这里只处理单一数据块的，例如：Range:0-XXX的，不处理Range:0-XXX,XXX-XXX,XXX-XXX的多个数据块内容，如果您的请求是多个数据块内容，请调整您的请求方案，如果一定要处理多个数据块，联系开发者或者自己定制研发，因为这种情况及其少用
                        if (getTransferEncodingChunked(vecMemPage[0].m_point, vecMemPage[0].m_nCount) &&
                            pMemPool->getLastFiveBytes(vecMemPage, vecLastFiveBytes)) {
                            if (vecLastFiveBytes[0] == '0' && vecLastFiveBytes[1] == '\r' &&
                                vecLastFiveBytes[2] == '\n' && vecLastFiveBytes[3] == '\r' &&
                                vecLastFiveBytes[4] == '\n') {
                                modifyHttpBodyByTransferEncodingChunked(vecMemPage);
                                isReceiveOk = true;
                            }
                        }
                    } else if (isAcceptRanges !=
                               "none") { // http206返回同样采用Content-Length计算是否接收完毕和常规接收业务逻辑相同
                        int totalLength = pMemPool->getMemPageUseLength(vecMemPage);
                        if (totalLength - nHttpHeader == nContentLength) {
                            isReceiveOk = true;
                        }
                    } else {
                        int totalLength = pMemPool->getMemPageUseLength(vecMemPage);
                        if (totalLength - nHttpHeader == nContentLength) {
                            isReceiveOk = true;
                        }
                    }

                    if (isWithoutHttpBody(strMethod, vecMemPage[0].m_point, vecMemPage[0].m_nCount)) {
                        isReceiveOk = true;
                    }
                }
            }

            int nShutdown = SSL_get_shutdown(ssl);
            if ((nShutdown & SSL_RECEIVED_SHUTDOWN) != 0) {
                OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                             "SSL_get_shutdown(ssl) is SSL_RECEIVED_SHUTDOWN");
                if (isReceiveOk) {
                    return true;
                }
                return false;
            }

            if (isReceiveOk) {
                return true;
            }

            int httpCode = 0;
            if (vecMemPage.size() > 0) {
                httpCode = getHttpCode(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
            }

            if (!bRetResult) {
                if (SSL_get_error(ssl, 0) == SSL_ERROR_ZERO_RETURN) {
                    if (!isReceiveOk) {
                        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                     "SSL_get_error(ssl, 0) == SSL_ERROR_ZERO_RETURN and isReceiveOk == false");
                    }
                }
                if (HttpCode::isSuccess(httpCode)) {
                    return true;
                }
                return false;
            }
        }
    }
    return true;
}

bool ConnPool::recvHttpWithTimeOut(const int nSocket, const std::string &strMethod, std::vector<SMemPage> &vecMemPage,
                                   void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                                   const int nRecvTimeOut)
{
    isClose = true;
    CMemPool *pMemPool = (CMemPool *)Application::g_memPool;
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "recv data without socket");
        return false;
    }

    bool isReceiveOk = false;
    char *szBuf = static_cast<char *>(pMaxBuffer);
    std::fill(szBuf, szBuf + nBufSize, 0);

    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLIN | POLLERR;
        pfd[0].revents = 0;
        int nRetCount = 0;
        if (!CSocket::poll(nRetCount, pfd, 1, nRecvTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRetCount, pfd, 1, POLL_RECV_TIME_OUT(%{public}dms)) == false", nRecvTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRetCount == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "recv time out %{public}dms，nRetCount == 0",
                         nRecvTimeOut);
            return false;
        }
        if (isAbort) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "User canceled receiving, isAbort is true");
            return false;
        }
        if ((pfd[0].revents & POLLIN) != 0) {
            int nRecvSize = nBufSize;
            bool bRetResult = CSocket::read(nSocket, szBuf, nRecvSize);
            if (nRecvSize <= 0) {
                break;
            }
            if (nRecvSize > 0) {
                if (!pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "pMemPool->memcpyPage(vecMemPage, szBuf, nRecvSize) == false");
                    return false;
                }
                std::string isAcceptRanges = getAcceptRanges(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                int nHttpHeader = getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                if (nHttpHeader > 0) {
                    int nContentLength = getContentLength(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
                    if (nContentLength == -1) {
                        std::vector<unsigned char> vecLastFiveBytes;
                        // 这里只处理单一数据块的，例如：Range:0-XXX的，不处理Range:0-XXX,XXX-XXX,XXX-XXX的多个数据块内容，如果您的请求是多个数据块内容，请调整您的请求方案，如果一定要处理多个数据块，联系开发者或者自己定制研发，因为这种情况及其少用
                        if (getTransferEncodingChunked(vecMemPage[0].m_point, vecMemPage[0].m_nCount) &&
                            pMemPool->getLastFiveBytes(vecMemPage, vecLastFiveBytes)) {
                            if (vecLastFiveBytes[0] == '0' && vecLastFiveBytes[1] == '\r' &&
                                vecLastFiveBytes[2] == '\n' && vecLastFiveBytes[3] == '\r' &&
                                vecLastFiveBytes[4] == '\n') {
                                modifyHttpBodyByTransferEncodingChunked(vecMemPage);
                                isReceiveOk = true;
                            }
                        }
                    } else if (isAcceptRanges !=
                               "none") { // http206返回同样采用Content-Length计算是否接收完毕和常规接收业务逻辑相同
                        int totalLength = pMemPool->getMemPageUseLength(vecMemPage);
                        if (totalLength - nHttpHeader == nContentLength) {
                            isReceiveOk = true;
                        }
                    } else {
                        int totalLength = pMemPool->getMemPageUseLength(vecMemPage);
                        if (totalLength - nHttpHeader == nContentLength) {
                            isReceiveOk = true;
                        }
                    }

                    if (isWithoutHttpBody(strMethod, vecMemPage[0].m_point, vecMemPage[0].m_nCount)) {
                        isReceiveOk = true;
                    }
                }
            }

            if (isReceiveOk) {
                return true;
            }

            int httpCode = 0;
            if (vecMemPage.size() > 0) {
                httpCode = getHttpCode(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
            }

            if (!bRetResult) {
                if (HttpCode::isSuccess(httpCode)) {
                    return true;
                }
                return false;
            }
        }
    }
    return true;
}

bool ConnPool::sendWithTimeOut(SSL *ssl, const std::string strRequest, void *userData, void *pMaxBuffer,
                               const int nBufSize,
                               size_t (*sendBody)(char *ptr, size_t size, size_t nmemb, void *userdata),
                               const int nSendTimeOut)
{
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "send data without socket");
        return false;
    }

    int nSendSize = strRequest.length();
    const char *pBuf = strRequest.c_str();
    int nWritePos = 0;
    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLOUT | POLLERR;
        pfd[0].revents = 0;
        int nRet = 0;
        if (!CSocket::poll(nRet, pfd, 1, nSendTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false", nSendTimeOut);
            return false;
        }

        if (nRet == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                         "send data time out %{public}dms，nRetCount == 0", nSendTimeOut);
            return false;
        }

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if ((pfd[0].revents & POLLOUT) != 0) {
            int nWriteSize = nSendSize - nWritePos;
            if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                return false;
            }
            if (nWriteSize == 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                return false;
            }
            nWritePos += nWriteSize;
            if (nWritePos == nSendSize) {
                break;
            }
        }
    }

    while (true) {
        int ret = sendBody(static_cast<char *>(pMaxBuffer), nBufSize, 1, userData);
        if (ret == 0) {
            break;
        }

        pBuf = static_cast<char *>(pMaxBuffer);
        nSendSize = ret;
        nWritePos = 0;
        while (true) {
            struct pollfd pfd[1];
            pfd[0].fd = nSocket;
            pfd[0].events = POLLOUT | POLLERR;
            pfd[0].revents = 0;
            int nRet = 0;
            if (!CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT)) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                             "CSocket::poll(nRet, pfd, 1, POLL_SEND_TIME_OUT(%{public}dms)) == false",
                             POLL_SEND_TIME_OUT);
                return false;
            }
            if ((pfd[0].revents & POLLERR) != 0) {
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
                return false;
            }

            if (nRet == 0) {
                OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool", "Error: write time out %{public}dms",
                             POLL_SEND_TIME_OUT);
                return false;
            }

            if ((pfd[0].revents & POLLOUT) != 0) {
                int nWriteSize = nSendSize - nWritePos;
                if (!CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize)) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                 "CSSLSocket::sslWrite(ssl, pBuf + nWritePos, nWriteSize) == false");
                    return false;
                }
                if (nWriteSize == 0) {
                    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "nWriteSize == 0");
                    return false;
                }
                nWritePos += nWriteSize;
                if (nWritePos == nSendSize) {
                    break;
                }
            }
        }
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "%{public}s", strRequest.c_str());
    return true;
}

bool ConnPool::recvWithTimeOut(SSL *ssl, const std::string &strMethod, void *userData, void *pMaxBuffer,
                               const int nBufSize,
                               size_t (*recvHeaders)(char *buffer, size_t size, size_t nitems, void *userdata),
                               size_t (*recvBody)(char *ptr, size_t size, size_t nmemb, void *userdata),
                               const int nRecvTimeOut)
{
    bool isClose = true;
    CMemPool *pMemPool = (CMemPool *)Application::g_memPool;
    int nSocket = SSL_get_fd(ssl);
    if (nSocket == -1) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "recv data without socket");
        return false;
    }

    bool isReceiveOk = false;
    char *szBuf = static_cast<char *>(pMaxBuffer);
    std::fill(szBuf, szBuf + nBufSize, 0);

    while (true) {
        struct pollfd pfd[1];
        pfd[0].fd = nSocket;
        pfd[0].events = POLLIN | POLLERR;
        pfd[0].revents = 0;
        int nRetCount = 0;
        if (!CSocket::poll(nRetCount, pfd, 1, nRecvTimeOut)) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                         "CSocket::poll(nRetCount, pfd, 1, POLL_RECV_TIME_OUT(%{public}dms)) == false", nRecvTimeOut);
            return false;
        }

        SSL *pSsl[1];
        pSsl[0] = ssl;
        SSL *rSsl[1];
        int nHasCount = 1;
        CSSLSocket::sslHasPending(pSsl, rSsl, nHasCount);

        if ((pfd[0].revents & POLLERR) != 0) {
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool", "(pfd[0].revents & POLLERR) != 0");
            return false;
        }

        if (nRetCount == 0 && nHasCount == 0) {
            OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "ConnPool",
                         "recv data time out(%{public}dms) nRetCount == 0 && nHasCount == 0", nRecvTimeOut);
            return false;
        }

        int nRecvSize = nBufSize;
        int nRecvPos = 0;
        int nHttpHeader = 0;
        int nContentLength = 0;
        int totalLength = 0;
        bool isChunked = false;
        char szLastFiveBytes[5];
        std::fill(szLastFiveBytes, szLastFiveBytes + sizeof(szLastFiveBytes), 0);
        if ((pfd[0].revents & POLLIN) != 0 || nHasCount > 0) {
            nRecvSize = nBufSize - nRecvPos;
            bool bRetResult = CSSLSocket::sslRead(ssl, szBuf + nRecvPos, nRecvSize, isClose);
            if (nRecvSize <= 0) {
                break;
            }
            nRecvPos += nRecvSize;
            totalLength += nRecvSize;
            if (nRecvSize > 0) {
                if (nHttpHeader == 0) {
                    nHttpHeader = getHttpHeader(szBuf, nRecvSize);
                    if (nHttpHeader == 0) {
                        continue;
                    }
                    nContentLength = getContentLength(szBuf, nRecvSize);
                    if (nContentLength == -1) {
                        // 这里只处理单一数据块的，例如：Range:0-XXX的，不处理Range:0-XXX,XXX-XXX,XXX-XXX的多个数据块内容，如果您的请求是多个数据块内容，请调整您的请求方案，如果一定要处理多个数据块，联系开发者或者自己定制研发，因为这种情况及其少用
                        isChunked = getTransferEncodingChunked(szBuf, nRecvSize);
                    }

                    if (nContentLength == -1 && isChunked == false) {
                        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ConnPool",
                                     "nContentLength == -1 && isChunked == false");
                        return false;
                    }
                    recvHeaders(szBuf, nRecvPos, 1, userData);
                    if (nRecvSize > nHttpHeader) {
                        recvBody(szBuf + nHttpHeader, nRecvSize - nHttpHeader, 1, userData);
                    }
                } else {
                    recvBody(szBuf, nRecvSize, 1, userData);
                }

                if (nContentLength == -1) {
                    if (nRecvSize >= 5) {
                        std::copy_n(szBuf + nRecvSize - 5, 5, szLastFiveBytes);
                    } else {
                        std::copy_n(szLastFiveBytes + nRecvSize, 5 - nRecvSize, szLastFiveBytes);
                        std::copy_n(szBuf, nRecvSize, szLastFiveBytes + nRecvSize);
                    }
                    if (isChunked) {
                        if (szLastFiveBytes[0] == '0' && szLastFiveBytes[1] == '\r' && szLastFiveBytes[2] == '\n' &&
                            szLastFiveBytes[3] == '\r' && szLastFiveBytes[4] == '\n') {
                            isReceiveOk = true;
                        }
                    }
                } else {
                    if (totalLength - nHttpHeader == nContentLength) {
                        isReceiveOk = true;
                    }
                }

                if (isWithoutHttpBody(strMethod, szBuf, nRecvSize)) {
                    isReceiveOk = true;
                }
            }

            int nShutdown = SSL_get_shutdown(ssl);
            if ((nShutdown & SSL_RECEIVED_SHUTDOWN) != 0) {
                OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                             "SSL_get_shutdown(ssl) is SSL_RECEIVED_SHUTDOWN");
                if (isReceiveOk) {
                    return true;
                }
                return false;
            }

            if (!bRetResult) {
                if (SSL_get_error(ssl, 0) == SSL_ERROR_ZERO_RETURN) {
                    if (!isReceiveOk) {
                        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool",
                                     "SSL_get_error(ssl, 0) == SSL_ERROR_ZERO_RETURN and isReceiveOk == false");
                    }
                }
                if (isReceiveOk) {
                    return true;
                }
                return false;
            }

            if (isReceiveOk) {
                return true;
            }
        }
    }
    return true;
}

int ConnPool::getHttpCode(const char *pBuf, const int nLength)
{
    int nPos = 0;
    for (int i = 0; i < nLength; i++) {
        if (pBuf[i] == '\n') {
            nPos = i;
            break;
        }
    }

    if (nPos == 0) {
        return -1;
    }

    char szCode[32];
    std::fill(szCode, szCode + sizeof(szCode), 0);

    nPos = 0;
    int j = 0;
    for (int i = 0; i < nLength; i++) {
        if (pBuf[i] == ' ') {
            if (nPos != 0) {
                break;
            }
            nPos = i;
            continue;
        }
        if (nPos != 0) {
            szCode[j++] = pBuf[i];
        }
    }

    return atoi(szCode);
}

int ConnPool::getStartPosByEnter(const char *pBuf, const int nLength)
{
    int nPos = -1;
    for (int i = 0; i < nLength - 1; i++) {
        if (pBuf[i] == '\r' && pBuf[i + 1] == '\n') {
            nPos = i + 1;
            break;
        }
    }
    return nPos + 1;
}

int ConnPool::getHttpHeader(const char *pBuf, const int nLength)
{
    int nPos = -1;
    for (int i = 0; i < nLength - 3; i++) {
        if (pBuf[i] == '\r' && pBuf[i + 1] == '\n' && pBuf[i + 2] == '\r' && pBuf[i + 3] == '\n') {
            nPos = i + 3;
            break;
        }
    }
    return nPos + 1;
}

int ConnPool::getContentLength(const char *pBuf, const int nLength)
{
    if (getHttpHeader(pBuf, nLength) == 0) {
        return -1;
    }

    const char *pContentLength = "Content-Length:";
    const char *pPos = strcasestr(pBuf, pContentLength);
    if (pPos == nullptr) {
        return -1;
    }

    char szLength[32];
    std::fill(szLength, szLength + sizeof(szLength), 0);

    int j = 0;
    for (int i = strlen(pContentLength); i < strlen(pPos); i++) {
        if (pPos[i] == '\r') {
            break;
        }
        szLength[j++] = pPos[i];
    }

    return atoi(szLength);
}

std::string ConnPool::getHttpLocation(const char *pBuf, const int nLength)
{
    std::string strRet;
    const char *pLocation = "Location:";
    const char *pPos = strcasestr(pBuf, pLocation);
    if (pPos == nullptr) {
        return strRet;
    }
    const char *pEnd = strstr(pPos, "\r");
    if (pPos != nullptr && pEnd != nullptr) {
        strRet.insert(0, pPos + strlen(pLocation), pEnd - pPos - strlen(pLocation));
    }
    strRet = strRet.erase(0, strRet.find_first_not_of(" "));
    strRet = strRet.erase(strRet.find_last_not_of(" ") + 1);
    return strRet;
}

std::string ConnPool::getAcceptRanges(const char *pBuf, const int nLength)
{
    if (getHttpHeader(pBuf, nLength) == 0) {
        return "none";
    }

    const char *pAcceptRanges = "Accept-Ranges:";
    const char *pPos = strcasestr(pBuf, pAcceptRanges);
    if (pPos == nullptr) {
        return "none";
    }

    char szLength[128];
    std::fill(szLength, szLength + sizeof(szLength), 0);

    int j = 0;
    for (int i = strlen(pAcceptRanges); i < strlen(pPos); i++) {
        if (pPos[i] == '\r') {
            break;
        }
        szLength[j++] = pPos[i];
    }

    if (strcmp(pAcceptRanges, "none") == 0) {
        return "none";
    }
    return szLength;
}

void ConnPool::getCookies(const char *pBuf, const int nLength, std::vector<std::string> &vecCordovaCookie)
{
    const char *pPos = pBuf;
    const char *pSetCookie = "Set-Cookie";
    while (true) {
        pPos = strcasestr(pPos, pSetCookie);
        if (pPos == nullptr) {
            break;
        }
        std::string strTmp;
        for (int i = 0; i < strlen(pPos); i++) {
            if (pPos[i] == '\r') {
                break;
            }
            strTmp.insert(strTmp.length(), pPos + i, 1);
        }
        vecCordovaCookie.push_back(strTmp);
        pPos = pPos + strTmp.length();
    }
    return;
}

bool ConnPool::getTransferEncodingChunked(const char *pBuf, const int nLength)
{
    const char *pTransferEncoding = "Transfer-Encoding:";
    const char *pPos = strcasestr(pBuf, pTransferEncoding);
    if (pPos == nullptr) {
        return false;
    }

    std::string strTmp;
    for (int i = strlen(pTransferEncoding); i < strlen(pPos); i++) {
        if (pPos[i] == '\r') {
            break;
        }
        strTmp.insert(strTmp.length(), pPos + i, 1);
    }
    strTmp = strTmp.erase(0, strTmp.find_first_not_of(" "));
    if (strTmp == "chunked") {
        return true;
    }
    return false;
}

bool ConnPool::modifyHttpBodyByTransferEncodingChunked(std::vector<unsigned char> &vecResponse)
{
    if (!getTransferEncodingChunked(reinterpret_cast<const char *>(&vecResponse[0]), vecResponse.size())) {
        return false;
    }
    const int nBodyPos = getHttpHeader(reinterpret_cast<const char *>(&vecResponse[0]), vecResponse.size());
    char *pBody = reinterpret_cast<char *>(&vecResponse[nBodyPos]);
    char *pEndPos = strstr(pBody, "\r\n");

    int nBlockLength = 0;
    std::string strBlockLength;
    strBlockLength.insert(0, pBody, pEndPos - pBody);
    char *endPtr;
    nBlockLength = static_cast<int>(strtol(strBlockLength.c_str(), &endPtr, 16));
    if (*endPtr != '\0') {
        return false;
    }
    int nTotalPos = 0;
    while (true) {
        if (nBlockLength == 0) {
            int nVecSize = vecResponse.size();
            vecResponse.erase(vecResponse.begin() + nTotalPos + nBodyPos, vecResponse.end()); // 清除最后一块数据
            pBody = reinterpret_cast<char *>(&vecResponse[nTotalPos + nBodyPos]);
            nVecSize = vecResponse.size();
            break;
        }
        for (int i = 0; i < pEndPos - pBody; i++) {
            vecResponse.erase(vecResponse.begin() + nTotalPos + nBodyPos); // 清除块长度16进制数字
        }

        pBody = reinterpret_cast<char *>(&vecResponse[nTotalPos + nBodyPos]);

        vecResponse.erase(vecResponse.begin() + nTotalPos + nBodyPos); // 清除数字后面的\r
        vecResponse.erase(vecResponse.begin() + nTotalPos + nBodyPos); // 清除数字后面的\n

        pBody = reinterpret_cast<char *>(&vecResponse[nTotalPos + nBodyPos]);

        nTotalPos += nBlockLength;
        pBody = reinterpret_cast<char *>(&vecResponse[nTotalPos + nBodyPos]);
        vecResponse.erase(vecResponse.begin() + nTotalPos + nBodyPos); // 清除块后面的\r
        vecResponse.erase(vecResponse.begin() + nTotalPos + nBodyPos); // 清除块后面的\n

        pBody = reinterpret_cast<char *>(&vecResponse[nTotalPos + nBodyPos]);
        pEndPos = strstr(pBody, "\r\n");
        strBlockLength = "";
        strBlockLength.insert(0, pBody, pEndPos - pBody);
        nBlockLength = static_cast<int>(strtol(strBlockLength.c_str(), &endPtr, 16));
        if (*endPtr != '\0') {
            return false;
        }
    }
    return true;
}

bool ConnPool::modifyHttpBodyByTransferEncodingChunked(std::vector<SMemPage> &vecMemPage)
{
    std::vector<unsigned char> vecResponse;
    for (int i = 0; i < vecMemPage.size(); i++) {
        vecResponse.insert(vecResponse.end(), vecMemPage[i].m_point, vecMemPage[i].m_point + vecMemPage[i].m_nCount);
    }
    modifyHttpBodyByTransferEncodingChunked(vecResponse);
    int nPageSize = ((CMemPool *)Application::g_memPool)->getPageSize();
    int nMaxCount = vecResponse.size() / nPageSize + (vecResponse.size() % nPageSize > 0 ? 1 : 0);

    ((CMemPool *)Application::g_memPool)->freePage(vecMemPage);
    vecMemPage.clear();
    ((CMemPool *)Application::g_memPool)->mallocPage(vecMemPage, nMaxCount);
    ((CMemPool *)Application::g_memPool)
        ->memcpyPage(vecMemPage, reinterpret_cast<char *>(&vecResponse[0]), vecResponse.size());
    return true;
}

bool ConnPool::getHttpHeaders(const char *pBuf, const int nLength, std::map<std::string, std::string> &mapHeaders)
{
    int nStartPos = getStartPosByEnter(pBuf, nLength);
    int nEndPos = getHttpHeader(pBuf, nLength) - 2; // httpbody开始前有两个回车和换行，去除一个回车和换行

    if (nStartPos <= 0 || nEndPos <= 0) {
        return false;
    }

    std::string headers;
    headers.insert(0, pBuf + nStartPos, nEndPos - nStartPos);
    while (headers.length() > 0) {
        int nStart = headers.find("\r\n");
        std::string header = headers.substr(0, nStart);
        int nPos = headers.find(":");
        if (nPos != std::string::npos) {
            std::string key = header.substr(0, nPos);
            std::string value = header.substr(nPos + 1);
            key = HttpUrl::trim(key);
            value = HttpUrl::trim(value);
            mapHeaders[key] = value;
        }
        headers = headers.substr(nStart + 2);
    }
    return true;
}

std::string ConnPool::getContentEncoding(const char *pBuf, const int nLength)
{
    std::string strRet;
    const char *pContentEncoding = "Content-Encoding:";
    const char *pPos = strcasestr(pBuf, pContentEncoding);
    if (pPos == nullptr) {
        return strRet;
    }
    const char *pEnd = strstr(pPos, "\r");
    if (pPos != nullptr && pEnd != nullptr) {
        strRet.insert(0, pPos + strlen(pContentEncoding), pEnd - pPos - strlen(pContentEncoding));
    }
    return strRet;
}

bool ConnPool::gzipDecompress(std::vector<SMemPage> &vecMemPage, const int nHeaderLength,
                              std::vector<unsigned char> &vecResponse)
{
    std::vector<unsigned char> vecData;
    vecData.insert(vecData.end(), vecMemPage[0].m_point + nHeaderLength,
                   vecMemPage[0].m_point + vecMemPage[0].m_nCount);
    for (int i = 1; i < vecMemPage.size(); i++) {
        vecData.insert(vecData.end(), vecMemPage[i].m_point, vecMemPage[i].m_point + vecMemPage[i].m_nCount);
    }

    const int CONST_CHUNK_SIZE = 16 * 1024;
    unsigned char out[CONST_CHUNK_SIZE];
    unsigned char *uncompressed = nullptr;
    unsigned long lngUncompressedCapacity = 0;
    unsigned long lngTotalOut = 0;

    // 初始化 zlib 流
    z_stream strm;
    std::fill(reinterpret_cast<char *>(&strm), reinterpret_cast<char *>(&strm) + sizeof(strm), 0);
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = vecData.size();
    strm.next_in = (Bytef *)&vecData[0];

    // 使用 16+MAX_WBITS 参数表示处理 gzip 格式
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "inflateInit2 failed: %{public}s",
                     strm.msg ? strm.msg : "no error message");
        return -1;
    }

    do {
        strm.avail_out = CONST_CHUNK_SIZE;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR || ret == Z_BUF_ERROR ||
            ret == Z_VERSION_ERROR) {
            inflateEnd(&strm);
            if (uncompressed != nullptr) {
                free(uncompressed);
            }
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "inflate failed: %{public}s",
                         strm.msg ? strm.msg : "no error message");
            return false;
        }
        // 分配或扩展输出缓冲区
        unsigned int nHave = CONST_CHUNK_SIZE - strm.avail_out;
        if (nHave == 0) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "CONST_CHUNK_SIZE - strm.avail_out == 0");
            continue;
        }
        if (lngTotalOut + nHave > lngUncompressedCapacity) {
            lngUncompressedCapacity = (lngTotalOut + nHave) * 2;
            unsigned char *newUncompressed = (unsigned char *)realloc(uncompressed, lngUncompressedCapacity);
            if (!newUncompressed) {
                inflateEnd(&strm);
                free(uncompressed);
                OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ConnPool", "Memory allocation failed");
                return false;
            }
            uncompressed = newUncompressed;
        }

        std::copy_n(out, nHave, uncompressed + lngTotalOut);
        lngTotalOut += nHave;
    } while (ret != Z_STREAM_END);
    vecResponse.insert(vecResponse.end(), uncompressed, uncompressed + lngTotalOut);

    inflateEnd(&strm);
    if (uncompressed != nullptr) {
        free(uncompressed);
    }
    return true;
}

bool ConnPool::isWithoutHttpBody(const std::string &strMethod, const char *pBuf, const int nLength)
{
    if (getHttpHeader(pBuf, nLength) > 0) {
        if (strcasecmp(strMethod.c_str(), "HEAD") == 0) {
            return true;
        }
        int nHttpCode = getHttpCode(pBuf, nLength);
        if (nHttpCode >= HttpCode::CONTINUE && nHttpCode < HttpCode::OK) {
            return true;
        }
        if (nHttpCode == HttpCode::NO_CONTENT) {
            return true;
        }
        if (nHttpCode == HttpCode::RESET_CONTENT) {
            return true;
        }
        if (nHttpCode == HttpCode::NOT_MODIFIED) {
            return true;
        }
    }
    return false;
}

ConnPoolManage::ConnPoolManage() {}

ConnPoolManage::~ConnPoolManage()
{
    for (auto it = m_mapUrl2Pool.begin(); it != m_mapUrl2Pool.end(); it++) {
        ConnPool *pConn = it->second;
        delete pConn;
    }
}

ConnPool *ConnPoolManage::getConnPool(const std::string &strDomain)
{
    ConnPool *pool = nullptr;
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_mapUrl2Pool.find(strDomain) == m_mapUrl2Pool.end()) {
        pool = new ConnPool(strDomain);
        m_mapUrl2Pool[strDomain] = pool;
    } else {
        pool = m_mapUrl2Pool[strDomain];
    }
    return pool;
}