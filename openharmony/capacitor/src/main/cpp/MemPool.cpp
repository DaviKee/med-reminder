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

#include "MemPool.h"
#include <cstring>

CMemPool::CMemPool()
{
    m_nCount = 0;
    m_nSize = 0;
}

CMemPool::~CMemPool() {}

bool CMemPool::init(const int nCount, const int nSize)
{
    m_nCount = nCount;
    m_nSize = nSize;

    for (int i = 0; i < m_nCount; i++) {
        SMemPage memPage;
        memPage.m_nOffSize = 0;
        memPage.m_nCount = 0;
        memPage.m_point = new char[m_nSize];
        if (memPage.m_point == nullptr) {
            return false;
        }
        m_pageStk.push(memPage);
    }
    return true;
}

bool CMemPool::mallocPage(std::vector<SMemPage>& vecPage, const int nCount)
{
    CGuard guard(m_lock);
    if (m_pageStk.size() < nCount) {
        for (int i = 0; i < nCount - m_pageStk.size(); i++) {
            SMemPage memPage;
            memPage.m_nOffSize = 0;
            memPage.m_nCount = 0;
            memPage.m_point = new char[m_nSize];
            if (memPage.m_point == nullptr) {
                return false;
            }
            std::fill(memPage.m_point, memPage.m_point + m_nSize, 0);
            m_pageStk.push(memPage);
        }
    }
    for (int i = 0; i < nCount; i++) {
        SMemPage memPage = m_pageStk.top();
        memPage.m_nOffSize = 0;
        memPage.m_nCount = 0;
        if (memPage.m_point == nullptr) {
            return false;
        }
        std::fill(memPage.m_point, memPage.m_point + m_nSize, 0);
        vecPage.push_back(memPage);
        m_pageStk.pop();
    }

    for (int i = 0; i < vecPage.size(); i++) {
        if (vecPage[i].m_point == nullptr) {
            return false;
        }
    }
    return true;
}

bool CMemPool::freePage(std::vector<SMemPage>& vecPage)
{
    CGuard guard(m_lock);
    for (int i = 0; i < vecPage.size(); i++) {
        SMemPage memPage = vecPage[i];
        memPage.m_nOffSize = 0;
        memPage.m_nCount = 0;
        if (memPage.m_point == nullptr) {
            return false;
        }
        m_pageStk.push(memPage);
    }
    vecPage.clear();
    return true;
}

bool CMemPool::freePage(SMemPage memPage)
{
    CGuard guard(m_lock);
    memPage.m_nOffSize = 0;
    memPage.m_nCount = 0;
    if (memPage.m_point == nullptr) {
        return false;
    }
    m_pageStk.push(memPage);
    return true;
}

bool CMemPool::freePage(char* pPagePoint)
{
    CGuard guard(m_lock);
    SMemPage memPage;
    memPage.m_nOffSize = 0;
    memPage.m_nCount = 0;
    memPage.m_point = pPagePoint;
    if (memPage.m_point == nullptr) {
        return false;
    }
    m_pageStk.push(memPage);
    return true;
}

int CMemPool::getFreePageCount()
{
    CGuard guard(m_lock);
    int nRet = m_pageStk.size();
    return nRet;
}

bool CMemPool::memcpyPage(std::vector<SMemPage>& vecPage, const char* pBuf, const int nSize)
{
    int nLeave;
    int nTotalSize = nSize;
    int nAddset = 0;

    int nSum = 0;
    while (true) {
        for (int i = 0; i < vecPage.size(); i++) {
            nSum += m_nSize - vecPage[i].m_nCount;
        }
        if (nSum < nSize) {
            if (!mallocPage(vecPage, 1)) {
                return false;
            }
            continue;
        }
        break;
    }

    int nCount = vecPage.size();
    for (int i = 0; i < nCount; i++) {
        if (vecPage[i].m_point == nullptr) {
            return false;
        }
        nLeave = nTotalSize - (m_nSize - vecPage[i].m_nCount);
        if (nLeave > 0) {
            std::copy_n(pBuf + nAddset, (m_nSize - vecPage[i].m_nCount), vecPage[i].m_point + vecPage[i].m_nCount);
            nTotalSize = nLeave;
            nAddset += m_nSize - vecPage[i].m_nCount;
            vecPage[i].m_nCount = m_nSize;
        } else if (nLeave <= 0) {
            std::copy_n(pBuf + nAddset, nTotalSize, vecPage[i].m_point + vecPage[i].m_nCount);
            vecPage[i].m_nCount += nTotalSize;
            return true;
        }
    }
    return true;
}

int CMemPool::getMemPageUseLength(const std::vector<SMemPage>& vecPage)
{
    int nSum = 0;
    for (int i = 0; i < vecPage.size(); i++) {
        nSum += vecPage[i].m_nCount;
    }
    return nSum;
}

bool CMemPool::getLastFiveBytes(const std::vector<SMemPage>& vecPage, std::vector<unsigned char>& vecLastFiveBytes)
{
    if (getMemPageUseLength(vecPage) < 5) {
        return false;
    }
    unsigned char szBuf[5];
    int nLeaveCount = 5;
    for (int i = vecPage.size() - 1; i >= 0; i--) {
        int nCount = vecPage[i].m_nCount;
        for (int k = nCount; k >= 0; k--) {
            szBuf[nLeaveCount - 1] = vecPage[i].m_point[k - 1];
            nLeaveCount--;
            if (nLeaveCount == 0) {
                break;
            }
        }
        if (nLeaveCount == 0) {
            break;
        }
    }
    vecLastFiveBytes.insert(vecLastFiveBytes.begin(), szBuf, szBuf + 5);
    return true;
}
