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

#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stack>
#include <vector>
#include "Lock.h"

/**
 * @brief Defines a memory pool, using paged memory management.
 *      Primarily used when the system needs to allocate large blocks of memory:
 *      retrieves the required number of memory pages from the pool and returns them after use.
 *      Facilitates memory reuse and avoids frequent memory allocation/deallocation by the system, ensuring stable
 * operation.
 */

struct SMemPage {
    /**Memory page pointer offset.*/
    int m_nOffSize;
    /**Memory usage*/
    int m_nCount;
    /**Memory pointer*/
    char* m_point;

    SMemPage()
    {
        m_nOffSize = 0;
        m_nCount = 0;
        m_point = nullptr;
    }

    ~SMemPage() {}

    SMemPage(const SMemPage& arg)
    {
        m_nOffSize = arg.m_nOffSize;
        m_nCount = arg.m_nCount;
        m_point = arg.m_point;
    }

    SMemPage& operator=(const SMemPage& arg)
    {
        if (this == &arg) {
            return *this;
        }
        m_nOffSize = arg.m_nOffSize;
        m_nCount = arg.m_nCount;
        m_point = arg.m_point;
        return *this;
    }
};

/**
 * @brief Paged memory management.
 *      Memory pages are only added and never reduced. When acquiring memory pages
 *      the pool automatically allocates new pages if insufficient pages exist to meet application needs.
 *      Provides methods for allocation, release, and data filling
 */
class CMemPool {
    /**Number of memory pages.*/
    int m_nCount;
    /**Size per memory page.*/
    int m_nSize;
    /**Memory pool lock.*/
    CLock m_lock;
    /**Stack for storing memory pages.*/
    std::stack<SMemPage> m_pageStk;
    /**Copying and assignment are disabled*/
    CMemPool(const CMemPool&);
    CMemPool& operator=(const CMemPool&);

public:
    CMemPool();
    virtual ~CMemPool();
    /**
     * @brief Initializes the memory pool.
     * @param nCount Number of pages.
     * @param nSize Size per memory page.
     * @return true if successful, false if failed.
     */
    bool init(const int nCount, const int nSize);
    bool Init(const int nCount, const int nSize)
    {
        return init(nCount, nSize);
    }
    /**
     * @brief External application requests memory pages.
     * @param memPage The requested memory page(s).
     * @param nCount Number of pages to request.
     * @return true if successful, false if failed.
     */
    bool mallocPage(std::vector<SMemPage>& memPage, const int nCount);
    bool MallocPage(std::vector<SMemPage>& memPage, const int nCount)
    {
        return mallocPage(memPage, nCount);
    }
    /**
     * @brief Releases memory pages (returns them to the pool). Applications must return pages to the pool after use.
     * @param memPage The memory page(s) to release.
     * @return true if successful, false if failed.
     */
    bool freePage(std::vector<SMemPage>& memPage);
    bool FreePage(std::vector<SMemPage>& memPage)
    {
        return freePage(memPage);
    }
    /**
     * @brief Releases a single memory page.
     * @param memPage The memory page.
     * @return true if successful, false if failed.
     */
    bool freePage(SMemPage memPage);
    bool FreePage(SMemPage memPage)
    {
        return freePage(memPage);
    }
    /**
     * @brief Releases a single memory page.
     * @param pPagePoint Pointer to the memory page (must be a previously allocated page).
     * @return true if successful, false if failed.
     */
    bool freePage(char* pPagePoint);
    bool FreePage(char* pPagePoint)
    {
        return freePage(pPagePoint);
    }
    /**
     * @brief Writes data into the page(s).
     * @param memPage A std::vector of memory pages.
     * @param pBuf Data buffer.
     * @param nSize Buffer size.
     * @return true if successful, false if failed.
     */
    bool memcpyPage(std::vector<SMemPage>& memPage, const char* pBuf, const int nSize);
    bool MemcpyPage(std::vector<SMemPage>& memPage, const char* pBuf, const int nSize)
    {
        return memcpyPage(memPage, pBuf, nSize);
    }
    /**
     * @brief Gets the total amount of memory used in a set of memory pages.
     * @param vecPage A std::vector of memory pages.
     * @return Amount of memory used.
     */
    int getMemPageUseLength(const std::vector<SMemPage>& vecPage);
    int GetMemPageUseLength(const std::vector<SMemPage>& vecPage)
    {
        return getMemPageUseLength(vecPage);
    }
    /**
     * @brief Retrieves the last 5 bytes from memory pages, used in HTTP protocol for chunked transfer encoding to
     * determine if transmission is complete.
     * @param vecPage A std::vector of memory pages.
     * @param vecLastFiveBytes Outputs the last 5 bytes.
     * @return
     */
    bool getLastFiveBytes(const std::vector<SMemPage>& vecPage, std::vector<unsigned char>& vecLastFiveBytes);
    bool GetLastFiveBytes(const std::vector<SMemPage>& vecPage, std::vector<unsigned char>& vecLastFiveBytes)
    {
        return getLastFiveBytes(vecPage, vecLastFiveBytes);
    }

    /**
     * @brief Gets the total number of memory pages.
     * @return Number of memory pages.
     */
    inline int getPageCount() const
    {
        return m_nCount;
    }
    inline int GetPageCount() const
    {
        return getPageCount();
    }
    /**
     * @brief Gets the number of free memory pages.
     * @return Number of free memory pages.
     */
    int getFreePageCount();
    int GetFreePageCount()
    {
        return getFreePageCount();
    }

    /**
     * @brief Gets the size of a memory page.
     * @return Size of the memory page.
     */
    int getPageSize() const
    {
        return m_nSize;
    }
    int GetPageSize() const
    {
        return getPageSize();
    }
};

#endif
