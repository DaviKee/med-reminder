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

#ifndef __SOCKET_H
#define __SOCKET_H

#include "Log.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cerrno>
#include <poll.h>
#include <netdb.h>
#include <cstring>
#include <fcntl.h>

/**
 * @brief Socket wrapper.
 *      All functions are static, effectively adding a namespace.
 */
class CSocket {
    static void log(CLog::Level enuLevel, const char* pFunName, const char* pFormat, ...);

public:
    /**
     * @brief create an endpoint for communication
     * @param[out] nSocket a file descriptor for the new socket is returned.  On error, -1 is returned, and errno is set
     * appropriately
     * @param nDomain AF_INET of default IPv4 Internet protocols
     * @param nType type of socket，SOCK_STREAM of default
     * @param nProtocl The  protocol  specifies  a  particular  protocol to be used with the socket. in which case
     * protocol can be specified as 0.
     * @return true if success, false if failed
     */
    static bool
    socket(int& nSocket, const int nDomain = AF_INET, const int nType = SOCK_STREAM, const int nProtocl = 0);
    /**
     * @brief set options on sockets
     * @param nSocket the file descriptor sockfd
     * @param nLevel level is specified as SOL_SOCKET
     * @param nOptName Optname and any specified options are passed uninterpreted to the appropriate protocol module for
     * interpretation
     * @param pOptVal Most socket-level options utilize an int argument for optval
     * @param nOptSize initially containing the size of the buffer pointed to by optval
     * @return true if success, false if failed
     */
    static bool setSockOpt(const int nSocket, const int nLevel, const int nOptName, void* pOptVal, const int nOptSize);
    /**
     * @brief Sets sockaddr
     * @param pSockaddr
     * @param pIp ip address
     * @param uPort port
     * @return true if success, false if failed
     */
    static bool iNetAddr(struct sockaddr* pSockaddr, const char* pIp, const unsigned int uPort);
    /**
     * @brief Sets sockaddr
     * @param sSockaddr
     * @param pHostName domain
     * @param uPort port
     * @return true if success, false if failed
     */
    static bool iMakeAddr(struct sockaddr& sSockaddr, const char* pHostName, const unsigned uPort);
    /**
     * @brief bind a name to a socket
     * @param nSocket the file descriptor sockfd
     * @param pSockaddr
     * @return true if success, false if failed
     */
    static bool bind(const int nSocket, const struct sockaddr* pSockaddr);
    /**
     * @brief bind a name to a socket
     * @param nSocket the file descriptor sockfd
     * @param pIp ip address
     * @param uPort port
     * @return true if success, false if failed
     */
    static bool bind(const int nSocket, const char* pIp, const unsigned int uPort);
    /**
     * @brief listen for connections on a socket
     * @param nSocket the file descriptor sockfd
     * @param nBackLog
     * @return true if success, false if failed
     */
    static bool listen(const int nSocket, const int nBackLog = 100);
    /**
     * @brief wait for some event on a file descriptor
     * @param nRetCount Number of sockets that meet the event condition.
     * @param pPollFd The set of file descriptors to be monitored is specified in the fds argument, which is an array of
     * structures
     * @param nPollSize size of array of pPollFd
     * @param nTimeOut
     * @return true if success, false if failed
     */
    static bool poll(int& nRetCount, struct pollfd* pPollFd, const int nPollSize, const int nTimeOut = 5000);
    /**
     * @brief accept a connection on a socket
     * @param nSocket Server listening socket.
     * @param nNewSocket Client connection socket.
     * @param pIp Client IP address.
     * @param nPort Port number.
     * @return true if success, false if failed
     */
    static bool accept(const int nSocket, int& nNewSocket, char* pIp, int& nPort);
    /**
     * @brief initiate a connection on a socket
     * @param nSocket the file descriptor sockfd
     * @param pSockaddr The address to connect to.
     * @param nSockSize Size of the address structure.
     * @return true if success, false if failed
     */
    static bool connect(const int nSocket, const struct sockaddr* pSockaddr, const int nSockSize);
    /**
     * @brief initiate a connection on a socket
     * @param nSocket the file descriptor sockfd
     * @param pHostName domain
     * @param uPort Port number.
     * @return true if success, false if failed
     */
    static bool connect(const int nSocket, const char* pHostName, const unsigned int uPort);
    /**
     * @brief read from a file descriptor
     * @param nSocket the file descriptor sockfd
     * @param pBuf Buffer to store the read content.
     * @param nSize Input: buffer length; Output: number of bytes read.
     * @return true if success, false if failed
     */
    static bool read(const int nSocket, void* pBuf, int& nSize);
    /**
     * @brief write to a file descriptor
     * @param nSocket the file descriptor sockfd
     * @param pBuf Buffer containing the content to write.
     * @param nSize Input: length of content to write; Output: number of bytes written.
     * @return true if success, false if failed
     */
    static bool write(const int nSocket, const void* pBuf, int& nSize);
    /**
     * @brief close a file descriptor
     * @param nSocket the file descriptor sockfd
     * @return true if success, false if failed
     */
    static bool close(const int nSocket);
    /**
     * @brief manipulate file descriptor
     * @param nRet On error, -1 is returned, and errno is set appropriately.
     * @param nFds the file descriptor sockfd
     * @param nCmd The operation is determined by cmd
     * @param nArg in most cases, the required type is int
     * @return true if success, false if failed
     */
    static bool fcntl_Int(int& nRet, const int nFds, const int nCmd, const int nArg);
};

#endif // __SOCKET_H
