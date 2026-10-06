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

#ifndef FILE_CACHE_H
#define FILE_CACHE_H

#include <string>

/**
 * @brief Defines a file cache class, primarily used for managing local sandbox files.
 *      All functions are static, effectively adding a namespace.
 */
class FileCache {
public:
    /**
     * @brief Checks if the path points to a file.
     * @param strFilePath File name.
     * @param strDirectory File directory (a "/" must separate the file name and directory).
     * @return true true if it is a file, false otherwise.
     */
    static bool IsFile(const std::string& strFilePath, const std::string& strDirectory);
    /**
     * @brief Opens a file.
     * @param strFilePath File name (creates the file if it does not exist).
     * @param strDirectory File directory (a "/" must separate the file name and directory).
     * @return FILE*; returns NULL if opening fails.
     */
    static FILE* openFile(const std::string& strFilePath, const std::string& strDirectory);
    /**
     * @brief Reads file content.
     * @param pFd File descriptor.
     * @param pBuf Output buffer.
     * @param nSize Output buffer size.
     * @param nSeek File read position.
     * @return int Number of bytes read.
     */
    static int readFile(FILE* pFd, unsigned char* pBuf, const int nSize, const long lngSeek);
    /**
     * @brief Reads file content.
     * @param strFilePath File path
     * @param pBuf Output buffer.
     * @param nSize Output buffer size.（number of bytes that must be read）
     * @param nStart tart position in the file.
     * @param nEnd  End position in the file.
     * @return true if nSize bytes are read, false otherwise.
     */
    static bool readFile(
        const std::string& strFilePath, unsigned char* pBuf, const int nSize, const long lngStart, const long lngEnd);
    /**
     * @brief Reads the entire file content.
     * @param strFilePath File path
     * @param pBuf Output buffer.
     * @param nSize Output buffer size.(typically the file size)
     * @return true if successful, false if failed
     */
    static bool readFile(const std::string& strFilePath, unsigned char* pBuf, const int nSize);
    /**
     * @brief Writes content to a file.
     * @param pFd File descriptor.
     * @param pBuf Content buffer.
     * @param nSize Buffer length.
     * @return Number of bytes written.
     */
    static int writeFile(FILE* pFd, const char* pBuf, const int nSize);
    /**
     * @brief Writes content to a file.
     * @param strFilePath File path
     * @param pBuf Content buffer.
     * @param nSize Buffer length.
     * @return Number of bytes written.
     */
    static int writeFile(const std::string& strFilePath, const char* pBuf, const int nSize);
    /**
     * @brief Writes content to a file.
     * @param strFilePath File path
     * @param pBuf Content buffer.
     * @param nSize Buffer length.
     * @param nOffset Write offset.
     * @return Number of bytes written.
     */
    static int writeFile(const std::string& strFilePath, const char* pBuf, const int nSize, const long lngOffset);
    /**
     * @brief create Directory
     * @param strFilePath Directory path
     * @return true if successful, false if failed
     */
    static bool createDirectory(const std::string& strFilePath);
    /**
     * @brief Checks if the path is a directory.
     * @param strFilePath Directory path
     * @return true if successful, false if failed
     */
    static bool isDirectory(const std::string& strFilePath);
    /**
     * @brief Create a file
     * @param strFilePath
     * @return true if successful, false if failed
     */
    static bool createFile(const std::string& strFilePath);
    /**
     * @brief Gets file size
     * @param pFd File descriptor.
     * @return Number of file size
     */
    static long getFileSize(FILE* pFd);
    /**
     * @brief Gets file size
     * @param strFilePath File path
     * @return Number of file size
     */
    static long getFileSize(const std::string& strFilePath);
    /**
     * @brief Close file
     * @param pFd File descriptor.
     */
    static void closeFile(FILE* pFd);
    /**
     * @brief Closes and deletes a file.
     * @param pFd File descriptor.
     * @param strFileName File name
     * @param strDirectory Directory path.(a "/" must separate the file name and directory).
     */
    static void closeDeleteFile(FILE* pFd, const std::string& strFileName, const std::string& strDirectory);
    /**
     * @brief Moves a file.
     * @param strSrcFile Source file.
     * @param strObjFile Target file.
     * @return true if successful, false if failed
     */
    static bool moveFile(const std::string& strSrcFile, const std::string& strObjFile);
    /**
     * @brief Copys a file.
     * @param strSrcFile Source file.
     * @param strObjFile Target file.
     * @return true if successful, false if failed
     */
    static bool copyFile(const std::string& strSrcFile, const std::string& strObjFile);
    /**
     * @brief Deletes a file.
     * @param strFile File path
     * @return true if successful, false if failed
     */
    static bool deleteFile(const std::string& strFile);
    /**
     * @brief Truncates a file.
     * @param strFilePath File path
     * @param nSize File size
     * @return true if successful, false if failed
     */
    static bool truncateFile(const std::string& strFilePath, const long lngSize);
    /**
     * @brief Deletes a directory and all its sub files and subdirectories.
     * @param path Directory Path
     */
    static void deleteDirectory(const char* path);
    /**
     * @brief Gets all files (including directories) under the specified directory.
     * @param path Directory Path
     * @param vecFile Output file vector.
     */
    static void getDirectoryFile(const char* path, std::vector<std::string>& vecFile);
    /**
     * @brief Clears all contents under the directory, including subdirectories (differs from deleteDirectory in that
     * the directory itself is not deleted).
     * @param strPath Directory Path
     * @return true if successful, false if failed
     */
    static bool clearDirectory(const std::string& strPath);
    /**
     * @brief Encodes a path (for Chinese characters or special symbols in paths). Cordova will encode the path after
     * receiving it.
     * @param value file path
     * @return Decoded path.
     */
    static std::string urlDecode(const std::string& strPath);
    static std::string urlEncode(const std::string& strPath);
    /**
     * @brief
     * @param base64Str
     * @return
     */
    static std::string getImageType(const std::string& base64Str);
    /**
     * @brief
     * @param base64Str
     * @return
     */
    static std::string getMimeType(const std::string& base64Str);
    /**
     *
     * @return
     */
    static std::string generalFileName();
};
#endif