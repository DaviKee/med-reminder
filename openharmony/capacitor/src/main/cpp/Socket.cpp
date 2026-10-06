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

#include "Socket.h"
#include <algorithm>
#include <stdio.h>
#include <ctype.h>

#ifndef NDEBUG
CLog* CLog::s_pLog = new CLog();
#else
CLog* CLog::s_pLog = nullptr;
#endif

void CSocket::log(CLog::Level enuLevel, const char* pFunName, const char* pFormat, ...)
{
    va_list arglist;
    va_start(arglist, pFormat);
    if (CLog::s_pLog != nullptr) {
        CLog::s_pLog->log(enuLevel, "socket", pFunName, pFormat, arglist);
    }
}

bool CSocket::socket(int& nSocket, const int nDomain, const int nType, const int nProtocol)
{
    nSocket = ::socket(nDomain, nType, nProtocol);
    if (nSocket == -1) {
        const int nErrNo = errno;
        const char* pErrMsg = strerror(nErrNo);
        log(CLog::Error,
            "socket",
            "::socket(nSocket, nDomain{%d}, nProtocol{%d}){error:%s,errno:%d}",
            nDomain,
            nProtocol,
            pErrMsg,
            nErrNo);
        return false;
    }
    return true;
}

bool CSocket::setSockOpt(const int nSocket, const int nLevel, const int nOptName, void* pOptVal, const int nOptSize)
{
    if (::setsockopt(nSocket, nLevel, nOptName, pOptVal, nOptSize) == -1) {
        const int nErrNo = errno;
        const char* pErrMsg = strerror(nErrNo);
        log(CLog::Error,
            "SetSockOpt",
            "::setsockopt(nSocket{%d}, nLevel{%d}, nOptName{%d}, pOptVal, nOptSize){error:%s, errno:%d}",
            nSocket,
            nLevel,
            nOptName,
            pErrMsg,
            nErrNo);
        return false;
    }
    return true;
}

bool CSocket::iNetAddr(struct sockaddr* pSockaddr, const char* pIp, const unsigned int uPort)
{
    struct sockaddr_in* pIn = reinterpret_cast<struct sockaddr_in*>(pSockaddr);
    pIn->sin_family = AF_INET;
    pIn->sin_port = htons(uPort);
    pIn->sin_addr.s_addr = inet_addr(pIp);
    return true;
}

bool CSocket::iMakeAddr(struct sockaddr& sSockaddr, const char* pHostName, const unsigned uPort)
{
    struct addrinfo sAddrInfo, *pRes;
    sAddrInfo.ai_family = AF_INET;
    sAddrInfo.ai_socktype = SOCK_STREAM;
    sAddrInfo.ai_flags = 0;
    sAddrInfo.ai_protocol = 0;

    char szBuf[32];
    std::fill(szBuf, szBuf + sizeof(szBuf), 0);
    sprintf(szBuf, "%d", uPort);
    int nRet = ::getaddrinfo(pHostName, szBuf, &sAddrInfo, &pRes);
    if (nRet != 0) {
        const char* pErrMsg = gai_strerror(nRet);
        log(CLog::Error,
            "iMakeAddr",
            "::getaddrinfo(pHostName{%s}, szBuf{%s}, &sAddrInfo, &pRes){error:%s,errno:%d}",
            pHostName,
            szBuf,
            pErrMsg,
            nRet);
        return false;
    }
    sSockaddr = *(pRes->ai_addr);
    ::freeaddrinfo(pRes);
    return true;
}

bool CSocket::bind(const int nSocket, const struct sockaddr* pSockaddr)
{
    if (::bind(nSocket, pSockaddr, sizeof(struct sockaddr)) == -1) {
        const int nErrNo = errno;
        const char* pErrMsg = strerror(nErrNo);
        log(CLog::Error,
            "bind",
            "::bind(nSocket{%d},pSockaddr, sizeof(struct sockaddr){error:%s,errno:%d}",
            nSocket,
            pErrMsg,
            nErrNo);
        return false;
    }
    return true;
}

bool CSocket::bind(const int nSocket, const char* pIp, const unsigned int uPort)
{
    struct sockaddr sSockAddr;
    if (!CSocket::iNetAddr(&sSockAddr, pIp, uPort)) {
        return false;
    }
    if (!CSocket::bind(nSocket, &sSockAddr)) {
        return false;
    }
    return true;
}

bool CSocket::listen(const int nSocket, const int nBackLog)
{
    if (::listen(nSocket, nBackLog) == -1) {
        const int nErrNo = errno;
        const char* pErrMsg = strerror(nErrNo);
        log(CLog::Error,
            "Listen",
            "::listen(nSocket{%d}, nBackLog{%d}){error:%d, errno:%d}",
            nSocket,
            nBackLog,
            pErrMsg,
            nErrNo);
        return false;
    }
    return true;
}

bool CSocket::poll(int& nRetCount, struct pollfd* pPollFd, const int nPollSize, const int nTimeOut)
{
    while (true) {
        nRetCount = ::poll(pPollFd, nPollSize, nTimeOut);
        if (nRetCount == -1) {
            const int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "Poll", "poll EINTR");
                continue;
            }
            log(CLog::Error,
                "Poll",
                "::poll(pPollFd{%p}, nPollSize{%d}, nTimeOut{%d}){error:%s,errno:%d}",
                pPollFd,
                nPollSize,
                nTimeOut,
                pErrMsg,
                nErrNo);
            return false;
        }
        break;
    }
    return true;
}

bool CSocket::accept(const int nSocket, int& nNewScoket, char* pIp, int& nPort)
{
    struct sockaddr_in sNewSockAddr;
    unsigned int nNewSize = sizeof(struct sockaddr);
    while (true) {
        nNewScoket = ::accept(nSocket, reinterpret_cast<struct sockaddr*>(&sNewSockAddr), &nNewSize);
        if (nNewScoket == -1) {
            int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "Accept", "accept EINTR");
                continue;
            }
            log(CLog::Error,
                "Accept",
                "::accept(nSocket{%d}, reinterpret_cast<struct sockaddr*>(&sNewSockAddr){%p}, "
                "&nNewSize{%d}){error:%s,errno:%d}",
                nSocket,
                &sNewSockAddr,
                nNewSize,
                pErrMsg,
                nErrNo);
            return false;
        }
        sprintf(pIp, "%s", inet_ntoa(sNewSockAddr.sin_addr));
        nPort = ntohs(sNewSockAddr.sin_port);
        break;
    }
    return true;
}

bool CSocket::connect(const int nSocket, const struct sockaddr* pSockaddr, const int nSockSize)
{
    while (true) {
        if (::connect(nSocket, pSockaddr, nSockSize) == -1) {
            const int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "Connect", "Connect EINTR");
                continue;
            }
            log(CLog::Error,
                "Connect",
                "::connect(nSocket{%d}, pSockaddr{%p}, nSockSize{%d})",
                nSocket,
                pSockaddr,
                nSockSize,
                nErrNo,
                pErrMsg);
            return false;
        }
        break;
    }
    return true;
}

bool CSocket::connect(const int nSocket, const char* pHostName, const unsigned int uPort)
{
    bool bIsHostName = false;
    for (int i = 0; i < strlen(pHostName); i++) {
        if (pHostName[i] != '.') {
            if (isalpha(pHostName[i])) {
                bIsHostName = true;
                break;
            }
        }
    }

    struct sockaddr sSockAddr;
    if (bIsHostName) {
        if (!CSocket::iMakeAddr(sSockAddr, pHostName, uPort)) {
            return false;
        }
    } else {
        if (!CSocket::iNetAddr(&sSockAddr, pHostName, uPort)) {
            return false;
        }
    }

    if (!CSocket::connect(nSocket, &sSockAddr, sizeof(struct sockaddr))) {
        return false;
    }
    return true;
}

bool CSocket::read(const int nSocket, void* pBuf, int& nSize)
{
    while (true) {
        nSize = ::read(nSocket, pBuf, nSize);
        if (nSize == -1) {
            const int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "Read", "read EINTR");
                continue;
            }
            log(CLog::Error,
                "Read",
                ":read(nSocket{%d}, pBuf{%p}, nSize{%d}){error:%s,errno:%d}",
                nSocket,
                pBuf,
                nSize,
                pErrMsg,
                nErrNo);
            return false;
        }
        break;
    }
    return true;
}

bool CSocket::write(const int nSocket, const void* pBuf, int& nSize)
{
    while (true) {
        nSize = ::write(nSocket, pBuf, nSize);
        if (nSize == -1) {
            const int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "Write", "write EINTR");
                continue;
            }
            log(CLog::Error,
                "Write",
                "::write(nSocket{%d}, pBuf{%p}, nSize{%d}){error:%s,errno:%d}",
                nSocket,
                pBuf,
                nSize,
                pErrMsg,
                nErrNo);
            return false;
        }
        break;
    }
    return true;
}

bool CSocket::close(const int nSocket)
{
    while (true) {
        if (::close(nSocket) == -1) {
            const int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "Close", "close EINTR");
                continue;
            }
            log(CLog::Error, "Close", "::close(nSocket{%d}){error:%d,errno:%s}", nSocket, nErrNo, pErrMsg);
            return false;
        }
        break;
    }
    return true;
}

bool CSocket::fcntl_Int(int& nRet, const int nFds, const int nCmd, const int nArg)
{
    while (true) {
        nRet = ::fcntl(nFds, nCmd, nArg);
        if (nRet == -1) {
            int err_no = errno;
            if (err_no == EINTR) {
                log(CLog::Warn, "fcntl_Int", "err_no == EINTR,Api will be re-started!{errno=%d}", errno);
                continue;
            }
            const char* err_msg = strerror(err_no);
            if (err_msg == nullptr) {
                err_msg = "Unknown error.";
            }
            // set return errcode
            log(CLog::Error,
                "fcntl_Int",
                "fcntl(nFds,nCmd,nArg)==-1{nFds=%d,nCmd=%x,nArg=%x,errno=%d,err_msg=%s}",
                nFds,
                nCmd,
                nArg,
                errno,
                err_msg);
            return false;
        }
        break;
    }
    return true;
}
