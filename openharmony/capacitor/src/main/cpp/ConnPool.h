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

#ifndef CONNPOOL_H
#define CONNPOOL_H

#include "MemPool.h"
#include "SSLSocket.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

class ConnPool {
    /**Number of attempts to connect to the server.*/
    static const int ATTEMPT_CONNECT_SERVER_TIMES = 5;
    /**Timeout for checking if a socket is readable.*/
    static const int POLL_RECV_TIME_OUT = 10000; // ms
    /**Timeout for checking if a socket is writable.*/
    static const int POLL_SEND_TIME_OUT = 60000; // ms
    /**Timeout for connecting to the server.*/
    static const int CONNECT_TIME_OUT = 60; // ss
    static const unsigned int LOG_PRINT_DOMAIN = 0xFF00;
    /**Domain name of the server to connect to.*/
    std::string m_strDomain;
    /**HTTP send body function.*/
    size_t (*sendBody)(char *ptr, size_t size, size_t nmemb, void *userdata);

public:
    using UploadCallback = std::function<void(std::string type, std::string url, int64_t sent, int64_t total)>;

public:
    explicit ConnPool(std::string strUrl);
    ~ConnPool();
    /**
     * @brief Creates an SSL connection.
     * @param ssl SSL object.
     * @param nPort Port number.
     * @param nConnectTimeOut Connection timeout duration.
     * @param nTLSVersion Minimum TLS/SSL version.
     * @param strCAFile Root certificate file (for client verification of server).
     * @param strChain Certificate and certificate chain file (for server verification of client).
     * @param strPKey Certificate key file.
     * @return true if successful, false if failed
     */
    bool getSSL(SSL **ssl, const int nPort = 443, const int nConnectTimeOut = CONNECT_TIME_OUT,
                const int nTLSVersion = TLS1_2_VERSION, const std::string strCAFile = "",
                const std::string strChain = "", const std::string strPKey = "");
    /**
     * @brief Creates a socket connection.
     * @param nSocket Socket descriptor.
     * @param nPort Port number.
     * @param nConnectTimeOut Connection timeout duration.
     * @return true if successful, false if failed
     */
    bool getSocket(int &nSocket, const int nPort = 80, const int nConnectTimeOut = CONNECT_TIME_OUT);
    /**
     * @brief Releases SSL resources.
     * @param ssl SSL object.
     * @param isReconnect Connection status; must be set to true to release.
     */
    void freeSocket(SSL *ssl, const bool isReconnect);
    /**
     * @brief Releases socket resources.
     * @param nSocket Socket descriptor.
     * @param isReconnect Connection status; must be set to true to release.
     */
    void freeSocket(int nSocket, const bool isReconnect);
    /**
     * @brief Sends std::string data over an SSL connection. Must send all data completely.
     * @param ssl SSL object.
     * @param strRequest Request std::string to send.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool sendWithTimeOut(SSL *ssl, const std::string strRequest, std::atomic<bool> &isAbort,
                         const int nSendTimeOut = POLL_SEND_TIME_OUT);
    /**
     * @brief Sends std::string data over a socket connection. Must send all data completely.
     * @param nSocket Socket descriptor.
     * @param strRequest Request std::string to send.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param nSendTimeOut  Send timeout duration.
     * @return true if successful, false if failed
     */
    bool sendWithTimeOut(int nSocket, const std::string strRequest, std::atomic<bool> &isAbort,
                         const int nSendTimeOut = POLL_SEND_TIME_OUT);
    /**
     * @brief Uploads a file using HTTP protocol over an SSL connection.
     * @param ssl SSL object.
     * @param strRequest HTTP request headers.
     * @param strTailParams End marker for file transmission in HTTP protocol.
     * @param strFilePath File to be transmitted.
     * @param pMaxBuffer Buffer for transmitting data.
     * @param nBufSize Buffer size.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param pCbc Plugin callback object for reporting upload progress.
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool uploadFileWithTimeOut(SSL *ssl, const std::string strRequest, const std::string strTailParams,
                               const std::string strFilePath, void *pMaxBuffer, const int nBufSize,
                               std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut = POLL_SEND_TIME_OUT);
    /**
     * @brief Uploads multiple files over an SSL connection.
     * @param ssl SSL object.
     * @param strRequest HTTP request headers.
     * @param strTailParams End marker for file transmission in HTTP protocol.
     * @param vecRequestParams Upload parameters for each file in the HTTP protocol (for multiple files).
     * @param vecFilePath Multiple files to be uploaded.
     * @param pMaxBuffer Buffer for transmitting data.
     * @param nBufSize Buffer size.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param pCbc Plugin callback object for reporting upload progress.
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool uploadFileWithTimeOuts(SSL *ssl, const std::string strRequest, const std::string strTailParams,
                                const std::vector<std::string> &vecRequestParams,
                                const std::vector<std::string> &vecFilePath, void *pMaxBuffer, const int nBufSize,
                                std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut = POLL_SEND_TIME_OUT);
        /**
     * @brief Uploads a file using HTTP protocol over an SSL connection.
     * @param ssl SSL object.
     * @param strRequest HTTP request headers.
     * @param strTailParams End marker for file transmission in HTTP protocol.
     * @param strFilePath File to be transmitted.
     * @param pMaxBuffer Buffer for transmitting data.
     * @param nBufSize Buffer size.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param pCallback callback for reporting upload progress.
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool uploadFileWithTimeOut(SSL *ssl, const std::string url, const std::string strRequest,
                               const std::string strTailParams, const std::string strFilePath, void *pMaxBuffer,
                               const int nBufSize, std::atomic<bool> &isAbort, UploadCallback pCallback,
                               const int nSendTimeOut = POLL_SEND_TIME_OUT);
    /**
     * @brief Uploads a file using HTTP protocol over an socket connection.
     * @param nSocket Socket descriptor.
     * @param strRequest HTTP request headers.
     * @param strTailParams End marker for file transmission in HTTP protocol.
     * @param strFilePath File to be transmitted.
     * @param pMaxBuffer Buffer for transmitting data.
     * @param nBufSize Buffer size.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param pCbc Plugin callback object for reporting upload progress.
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool uploadFileWithTimeOut(int nSocket, const std::string strRequest, const std::string strTailParams,
                               const std::string strFilePath, void *pMaxBuffer, const int nBufSize,
                               std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut = POLL_SEND_TIME_OUT);
    /**
     * @brief Uploads multiple files over a socket connection.
     * @param nSocket Socket descriptor.
     * @param strRequest HTTP request headers.
     * @param strTailParams End marker for file transmission in HTTP protocol.
     * @param vecRequestParams Upload parameters for each file in the HTTP protocol (for multiple files).
     * @param vecFilePath Multiple files to be uploaded.
     * @param pMaxBuffer Buffer for transmitting data.
     * @param nBufSize Buffer size.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param pCbc Plugin callback object for reporting upload progress.
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool uploadFileWithTimeOuts(int nSocket, const std::string strRequest, const std::string strTailParams,
                                const std::vector<std::string> &vecRequestParams,
                                const std::vector<std::string> &vecFilePath, void *pMaxBuffer, const int nBufSize,
                                std::atomic<bool> &isAbort, void *pCbc, const int nSendTimeOut = POLL_SEND_TIME_OUT);
        /**
     * @brief Uploads a file using HTTP protocol over an socket connection.
     * @param nSocket Socket descriptor.
     * @param strRequest HTTP request headers.
     * @param strTailParams End marker for file transmission in HTTP protocol.
     * @param strFilePath File to be transmitted.
     * @param pMaxBuffer Buffer for transmitting data.
     * @param nBufSize Buffer size.
     * @param isAbort Abort flag during sending (set in multi-threaded environments).
     * @param pCallback callback for reporting upload progress.
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool uploadFileWithTimeOut(int nSocket, const std::string url, const std::string strRequest, const std::string strTailParams,
                               const std::string strFilePath, void *pMaxBuffer, const int nBufSize,
                               std::atomic<bool> &isAbort, UploadCallback pCallback,
                               const int nSendTimeOut = POLL_SEND_TIME_OUT);
    /**
     * @brief Receives a resource (e.g., js, css, img) via HTTP protocol over an SSL connection.
     * @param ssl SSL object.
     * @param ssl http request method.
     * @param strFilePath Cached file path after reception.
     * @param mapHeaders HTTP headers of the file response.
     * @param strDirectory Directory to save the file.
     * @param pMaxBuffer Buffer for receiving data.
     * @param nBufSize Buffer size.
     * @param isClose Whether the SSL connection is closed.
     * @param isAbort Abort flag during reading (set in multi-threaded environments).
     * @param pCbc Plugin callback object for reporting upload progress.
     * @param nRecvTimeOut Receive timeout duration.
     * @return true if successful, false if failed
     */
    bool recvHttpWithTimeOut(SSL *ssl, const std::string &strMethod, std::string &strFilePath,
                             std::map<std::string, std::string> &mapHeaders, const std::string &strDirectory,
                             void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                             void *pCbc = nullptr, const int nRecvTimeOut = POLL_RECV_TIME_OUT);
    /**
     * @param Receives a resource (e.g., js, css, img) via HTTP protocol over a socket connection.
     * @param nSocket Socket descriptor.
     * @param ssl http request method.
     * @param strFilePath Cached file path after reception.
     * @param mapHeaders HTTP headers of the file response.
     * @param strDirectory Directory to save the file.
     * @param pMaxBuffer  Buffer for receiving data.
     * @param nBufSize Buffer size.
     * @param isClose Whether the socket connection is closed.
     * @param isAbort Abort flag during reading (set in multi-threaded environments).
     * @param pCbc Plugin callback object for reporting upload progress.
     * @param nRecvTimeOut Receive timeout duration.
     * @return true if successful, false if failed
     */
    bool recvHttpWithTimeOut(const int nSocket, const std::string &strMethod, std::string &strFilePath,
                             std::map<std::string, std::string> &mapHeaders, const std::string &strDirectory,
                             void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                             void *pCbc = nullptr, const int nRecvTimeOut = POLL_RECV_TIME_OUT);
    /**
     * @param Dynamically retrieves data (e.g., POST requests) via HTTP protocol over an SSL connection.
     * @param ssl SSL object.
     * @param ssl http request method.
     * @param vecMemPage Retrieved data stored in memory pages.
     * @param pMaxBuffer Buffer for receiving data.
     * @param nBufSize Buffer size.
     * @param isClose Whether the SSL connection is closed.
     * @param isAbort Abort flag during reading (set in multi-threaded environments).
     * @param nRecvTimeOut Receive timeout duration.
     * @return true if successful, false if failed
     */
    bool recvHttpWithTimeOut(SSL *ssl, const std::string &strMethod, std::vector<SMemPage> &vecMemPage,
                             void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                             const int nRecvTimeOut = POLL_RECV_TIME_OUT);
    /**
     * @param Dynamically retrieves data (e.g., POST requests) via HTTP protocol over a socket connection.
     * @param nSocket Socket descriptor.
     * @param ssl http request method.
     * @param vecMemPage Retrieved data stored in memory pages.
     * @param pMaxBuffer Buffer for receiving data.
     * @param nBufSize Buffer size.
     * @param isClose Whether the SSL connection is closed.
     * @param isAbort Abort flag during reading (set in multi-threaded environments).
     * @param nRecvTimeOut Receive timeout duration.
     * @return true if successful, false if failed
     */
    bool recvHttpWithTimeOut(const int nSocket, const std::string &strMethod, std::vector<SMemPage> &vecMemPage,
                             void *pMaxBuffer, const int nBufSize, bool &isClose, std::atomic<bool> &isAbort,
                             const int nRecvTimeOut = POLL_RECV_TIME_OUT);
    /**
     * @param Sends std::string data over an SSL connection.
     * @param ssl SSL object.
     * @param strRequest HTTP request headers.
     * @param userData User data (typically an object pointer).
     * @param pMaxBuffer Buffer for sending data.
     * @param nBufSize Buffer size.
     * @param sendBody Function to send body
     * @param nSendTimeOut Send timeout duration.
     * @return true if successful, false if failed
     */
    bool sendWithTimeOut(SSL *ssl, const std::string strRequest, void *userData, void *pMaxBuffer, const int nBufSize,
                         size_t (*sendBody)(char *ptr, size_t size, size_t nmemb, void *userdata),
                         const int nSendTimeOut = POLL_SEND_TIME_OUT);
    /**
     * @param Receives data via HTTP protocol over an SSL connection.
     * @param ssl SSL object.
     * @param ssl http request method.
     * @param userData User data (typically an object pointer).
     * @param pMaxBuffer Buffer for receiving data.
     * @param nBufSize Buffer size.
     * @param recvHeaders Function to handle HTTP headers.
     * @param recvBody Function to handle HTTP body.
     * @param nRecvTimeOut Receive timeout duration.
     * @return true if successful, false if failed
     */
    bool recvWithTimeOut(SSL *ssl, const std::string &strMethod, void *userData, void *pMaxBuffer, const int nBufSize,
                         size_t (*recvHeaders)(char *buffer, size_t size, size_t nitems, void *userdata),
                         size_t (*recvBody)(char *ptr, size_t size, size_t nmemb, void *userdata),
                         const int nRecvTimeOut = POLL_RECV_TIME_OUT);
    /**
     * @param Gets the HTTP status code.
     * @param pBuf Input buffer.
     * @param nLength Buffer length.
     * @return HTTP status code. Returns 0 if not found.
     */
    int getHttpCode(const char *pBuf, const int nLength);
    /**
     * @param Gets the end position(\r\n) of a line in the HTTP protocol.
     * @param pBuf Input buffer.
     * @param nLength Buffer length.
     * @return The end position of the HTTP protocol line.
     */
    int getStartPosByEnter(const char *pBuf, const int nLength);
    /**
     * @param Gets the end position of HTTP headers.
     * @param pBuf Input buffer.
     * @param nLength Buffer length.
     * @return End position of HTTP headers. Returns -1 if not found.
     */
    int getHttpHeader(const char *pBuf, const int nLength);
    /**
     * @param Gets the Content-Length value.
     * @param pBuf Input buffer.
     * @param nLength Buffer length.
     * @return Content length. Returns -1 if not found.
     */
    int getContentLength(const char *pBuf, const int nLength);
    /**
     * @param Gets the Location value.
     * @param pBuf Input buffer.
     * @param nLength Buffer length.
     * @return location， Returns "" if not found
     */
    std::string getHttpLocation(const char *pBuf, const int nLength);
    /**
     * @param Checks if the transmission is chunked.
     * @param pBuf Input buffer.
     * @param nLength Buffer length.
     * @return true if chunked transfer, false otherwise.
     */
    bool getTransferEncodingChunked(const char *pBuf, const int nLength);
    /**
     * @param Removes chunked transfer encoding markers.
     * @param vecResponse Data buffer.
     * @return true if successful, false if failed
     */
    bool modifyHttpBodyByTransferEncodingChunked(std::vector<unsigned char> &vecResponse);
    /**
     * Removes chunked transfer encoding markers.
     * @param vecMemPage Data memory pages.
     * @return true if successful, false if failed
     */
    bool modifyHttpBodyByTransferEncodingChunked(std::vector<SMemPage> &vecMemPage);
    /**
     * @param Parses HTTP headers from the buffer.
     * @param pBuf Data transmission buffer.
     * @param nLength Buffer length.
     * @param mapHeaders Output HTTP headers.
     * @return true if successful, false if failed
     */
    bool getHttpHeaders(const char *pBuf, const int nLength, std::map<std::string, std::string> &mapHeaders);
    /**
     * @param Gets the Accept-Ranges std::string.
     * @param pBuf Data transmission buffer.
     * @param nLength Buffer length.
     * @return Ranges value. Returns "none" if not found.
     */
    std::string getAcceptRanges(const char *pBuf, const int nLength);
    /**
     * @param Gets cookies from the data buffer.
     * @param pBuf Data transmission buffer.
     * @param nLength Buffer length.
     * @param vecCordovaCookie Output cookies
     */
    void getCookies(const char *pBuf, const int nLength, std::vector<std::string> &vecCordovaCookie);
    /**
     * @param Gets the Content-Encoding std::string.
     * @param pBuf Data transmission buffer.
     * @param nLength Buffer length.
     * @return Encoding std::string. Returns "" if not found.
     */
    std::string getContentEncoding(const char *pBuf, const int nLength);
    /**
     * @param Decompresses data.
     * @param vecMemPage Input data memory pages.
     * @param nHeaderLength Length of HTTP headers.
     * @param vecResponse Output decompressed data.
     * @return  true if successful, false if failed
     */
    bool gzipDecompress(std::vector<SMemPage> &vecMemPage, const int nHeaderLength,
                        std::vector<unsigned char> &vecResponse);
    /**
     *
     * @param strMethod http method
     * @param pBuf Data transmission buffer.
     * @param nLength Buffer length.
     * @return true if successful, false if failed
     */
    bool isWithoutHttpBody(const std::string &strMethod, const char *pBuf, const int nLength);
};

/**
 * @brief Manages server connection objects, where one domain corresponds to one connection.
 */
class ConnPoolManage {
    std::mutex m_mutex;
    std::map<std::string, ConnPool *> m_mapUrl2Pool;

public:
    ConnPoolManage();
    ~ConnPoolManage();
    /**
     * @brief Gets the connection object.
     * @param strDomain Domain name.
     * @return Connection object.
     */
    ConnPool *getConnPool(const std::string &strDomain);
};
#endif