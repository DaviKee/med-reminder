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

#include "FileCache.h"
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <openssl/md5.h>

#include <cctype>
#include <sstream>
#include <iomanip>

bool FileCache::IsFile(const std::string& strFilePath, const std::string& strDirectory)
{
    std::string strFile = strDirectory + urlDecode(strFilePath);
    if (access(strFile.c_str(), F_OK) != 0) {
        return false;
    }
    struct stat s_buf;
    stat(strFile.c_str(), &s_buf);
    if (S_ISREG(s_buf.st_mode)) {
        return true;
    }
    return false;
}

/*
 *strFilePath路径以/开头
 *strDirectory不以/结尾,必须以/开头
 *只要确保strDirectory和strFilePath有一个/就可以
 *先创建目录，
 *后创建文件夹目录
 */
FILE* FileCache::openFile(const std::string& strFilePath, const std::string& strDirectory)
{
    if (!strDirectory.empty()) {
        std::string strChildPath = "";
        std::string strPath = strDirectory.substr(1) + "/";
        int nChildPos = strPath.find("/");
        while (nChildPos != std::string::npos) {
            strChildPath += "/";
            strChildPath += strPath.substr(0, nChildPos);
            if (access(strChildPath.c_str(), F_OK) != 0) {
                if (mkdir(strChildPath.c_str(), 0777) != 0) {
                    return nullptr;
                }
            }
            strPath = strPath.substr(nChildPos + 1);
            nChildPos = strPath.find("/");
        }
    }

    std::string strFilePathTmp = urlDecode(strFilePath);
    if (strFilePath.find("/data/storage/el2/media") != 0) { // 媒体文件media不存在，但可以读取
        int nPos = strFilePathTmp.find_last_of("/");
        if (nPos != std::string::npos) {
            std::string strPath = strFilePathTmp.substr(1, nPos + 1);
            std::string strChildPath = strDirectory;
            int nChildPos = strPath.find("/");
            while (nChildPos != std::string::npos) {
                strChildPath += "/";
                strChildPath += strPath.substr(0, nChildPos);
                if (access(strChildPath.c_str(), F_OK) != 0) {
                    if (mkdir(strChildPath.c_str(), 0777) != 0) {
                        return nullptr;
                    }
                }
                strPath = strPath.substr(nChildPos + 1);
                nChildPos = strPath.find("/");
            }
        }
    }

    std::string strFile = strDirectory + strFilePathTmp;
    FILE* file = nullptr;
    if (access(strFile.c_str(), F_OK) != 0) {
        file = fopen(strFile.c_str(), "w+"); // 文件不存在，默认el2级别文件
    } else if (strFile.find("resources/resfile") != std::string::npos ||
               strFile.find("el2/media") != std::string::npos) {
        file = fopen(strFile.c_str(), "r"); // resfile资源文件只能读，不能写
    } else {
        file = fopen(strFile.c_str(), "r+"); // 其他沙箱路径文件可读可写
    }
    return file;
}

int FileCache::readFile(FILE* pFd, unsigned char* pBuf, const int nSize, const long lngSeek)
{
    if (pFd == nullptr) {
        return 0;
    }
    fseek(pFd, lngSeek, SEEK_SET);
    int nReadSize = fread(pBuf, sizeof(char), nSize, pFd);
    return nReadSize;
}

bool FileCache::readFile(
    const std::string& strFilePath, unsigned char* pBuf, const int nSize, const long lngStart, const long lngEnd)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    if (access(strFilePathTmp.c_str(), F_OK) != 0) {
        return false;
    }
    long lngFileSize = getFileSize(strFilePathTmp);
    if (lngStart < 0) {
        return false;
    }
    if (lngEnd < 0 || lngStart > lngEnd) {
        return false;
    }
    if (nSize < lngEnd - lngStart) {
        return false;
    }
    if (lngEnd > lngFileSize) {
        return false;
    }
    FILE* file = fopen(strFilePathTmp.c_str(), "r+");
    fseek(file, lngStart, SEEK_SET);
    int nReadSize = fread(pBuf, sizeof(char), nSize, file);
    closeFile(file);
    if (nReadSize != nSize) {
        return false;
    }
    return true;
}

bool FileCache::readFile(const std::string& strFilePath, unsigned char* pBuf, const int nSize)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    if (access(strFilePathTmp.c_str(), F_OK) != 0) {
        return false;
    }

    FILE* file = fopen(strFilePathTmp.c_str(), "r");
    long consumed = 0;
    while (true) {
        unsigned char* pMaxBuffer = pBuf + consumed;
        int nBufSize = nSize - consumed;
        int ret = FileCache::readFile(file, pMaxBuffer, nBufSize, consumed);
        if (ret == 0) {
            break;
        }
        consumed += ret;
        if (consumed >= nSize) {
            break;
        }
    }
    closeFile(file);
    return true;
}

int FileCache::writeFile(FILE* pFd, const char* pBuf, const int nSize)
{
    if (pFd == nullptr) {
        return 0;
    }
    int nWriteSize = fwrite(pBuf, sizeof(char), nSize, pFd);
    return nWriteSize;
}

int FileCache::writeFile(const std::string& strFilePath, const char* pBuf, const int nSize)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    int nPos = strFilePathTmp.find_last_of("/");
    if (nPos != std::string::npos) {
        std::string strPath = strFilePathTmp.substr(1, nPos + 1);
        std::string strChildPath = "";
        int nChildPos = strPath.find("/");
        while (nChildPos != std::string::npos) {
            strChildPath += "/";
            strChildPath += strPath.substr(0, nChildPos);
            if (access(strChildPath.c_str(), F_OK) != 0) {
                if (mkdir(strChildPath.c_str(), 0777) != 0) {
                    return 0;
                }
                strChildPath += "/";
            }
            strPath = strPath.substr(nChildPos + 1);
            nChildPos = strPath.find("/");
        }
    }

    std::string strFile = strFilePathTmp;
    FILE* pFd = fopen(strFile.c_str(), "w+");
    int nWriteSize = fwrite(pBuf, sizeof(char), nSize, pFd);
    fclose(pFd);
    return nWriteSize;
}

int FileCache::writeFile(const std::string& strFilePath, const char* pBuf, const int nSize, const long lngOffset)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    int nPos = strFilePathTmp.find_last_of("/");
    if (nPos != std::string::npos) {
        std::string strPath = strFilePathTmp.substr(1, nPos + 1);
        std::string strChildPath = "";
        int nChildPos = strPath.find("/");
        while (nChildPos != std::string::npos) {
            strChildPath += "/";
            strChildPath += strPath.substr(0, nChildPos);
            if (access(strChildPath.c_str(), F_OK) != 0) {
                if (mkdir(strChildPath.c_str(), 0777) != 0) {
                    return 0;
                }
                strChildPath += "/";
            }
            strPath = strPath.substr(nChildPos + 1);
            nChildPos = strPath.find("/");
        }
    }

    std::string strFile = strFilePathTmp;
    FILE* pFd = nullptr;
    if (lngOffset > 0) {
        pFd = fopen(strFile.c_str(), "a");
    } else {
        pFd = fopen(strFile.c_str(), "w+");
    }
    int nWriteSize = fwrite(pBuf, sizeof(char), nSize, pFd);
    fclose(pFd);
    return nWriteSize;
}

bool FileCache::createDirectory(const std::string& strFilePath)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    std::string strPath = strFilePathTmp.substr(1);
    std::string strChildPath = "";
    int nChildPos = strPath.find("/");
    while (nChildPos != std::string::npos) {
        strChildPath += "/";
        strChildPath += strPath.substr(0, nChildPos);
        if (access(strChildPath.c_str(), F_OK) != 0) {
            if (mkdir(strChildPath.c_str(), 0777) != 0) {
                return false;
            }
            strChildPath += "/";
        }
        strPath = strPath.substr(nChildPos + 1);
        nChildPos = strPath.find("/");
    }
    strChildPath += "/";
    strChildPath += strPath;
    if (access(strChildPath.c_str(), F_OK) != 0) {
        if (mkdir(strChildPath.c_str(), 0777) != 0) {
            return false;
        }
    }
    return true;
}

bool FileCache::isDirectory(const std::string& strFilePath)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    if (access(strFilePathTmp.c_str(), F_OK) != 0) {
        return false;
    }
    struct stat s_buf;
    stat(strFilePathTmp.c_str(), &s_buf);
    if (S_ISDIR(s_buf.st_mode)) {
        return true;
    }
    return false;
}

bool FileCache::createFile(const std::string& strFilePath)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    std::string strPath = strFilePathTmp.substr(1);
    std::string strChildPath = "";
    int nChildPos = strPath.find("/");
    while (nChildPos != std::string::npos) {
        strChildPath += "/";
        strChildPath += strPath.substr(0, nChildPos);
        if (access(strChildPath.c_str(), F_OK) != 0) {
            if (mkdir(strChildPath.c_str(), 0777) != 0) {
                return false;
            }
            strChildPath += "/";
        }
        strPath = strPath.substr(nChildPos + 1);
        nChildPos = strPath.find("/");
    }
    strChildPath += "/";
    strChildPath += strPath;
    if (access(strChildPath.c_str(), F_OK) != 0) {
        FILE* pf = fopen(strChildPath.c_str(), "w");
        if (pf == nullptr) {
            return false;
        }
        fclose(pf);
    }
    return true;
}

long FileCache::getFileSize(FILE* pFd)
{
    if (pFd == nullptr) {
        return 0;
    }
    long curOffset = ftell(pFd);
    fseek(pFd, 0, SEEK_END);
    long fileSize = ftell(pFd);
    return fileSize;
}

long FileCache::getFileSize(const std::string& strFilePath)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    struct stat statBuf;
    int res = -1;
    if (stat(strFilePathTmp.c_str(), &statBuf) != 0) { // 获取linux操作系统下文件的属性
        return 0;
    }

    if (!S_ISREG(statBuf.st_mode)) {
        return 0;
    }

    return statBuf.st_size;
}

void FileCache::closeFile(FILE* pFd)
{
    fclose(pFd);
}

bool FileCache::moveFile(const std::string& strSrcFile, const std::string& strObjFile)
{
    std::string strSrcFileTmp = urlDecode(strSrcFile);
    std::string strObjFileTmp = urlDecode(strObjFile);
    if (rename(strSrcFileTmp.c_str(), strObjFileTmp.c_str()) == 0) {
        return true;
    }
    return false;
}

bool FileCache::copyFile(const std::string& strSrcFile, const std::string& strObjFile)
{
    std::string strSrcFileTmp = urlDecode(strSrcFile);
    std::string strObjFileTmp = urlDecode(strObjFile);
    FILE* pRead = fopen(strSrcFileTmp.c_str(), "rb");
    if (pRead == nullptr) {
        return false;
    }
    FILE* pWrite = fopen(strObjFileTmp.c_str(), "wb");
    if (pWrite == nullptr) {
        fclose(pRead);
        return false;
    }

    int nReadCount = 0;
    const int bufLength = 1024 * 1024 * 2;
    char* pBuffer = static_cast<char*>(malloc(bufLength));
    while ((nReadCount = fread(pBuffer, sizeof(char), bufLength, pRead)) > 0) {
        int nWriteCount = 0;
        do {
            nWriteCount += fwrite(pBuffer + nWriteCount, sizeof(char), nReadCount - nWriteCount, pWrite);
        } while (nWriteCount < nReadCount);
    }

    free(pBuffer);
    fclose(pRead);
    fclose(pWrite);
    return true;
}

void FileCache::closeDeleteFile(FILE* pFd, const std::string& strFileName, const std::string& strDirectory)
{
    std::string strFile = strDirectory + urlDecode(strFileName);
    fclose(pFd);
    remove(strFile.c_str());
}

bool FileCache::deleteFile(const std::string& strFile)
{
    std::string strFilePathTmp = urlDecode(strFile);
    if (remove(strFilePathTmp.c_str()) == 0) {
        return true;
    }
    return false;
}

bool FileCache::truncateFile(const std::string& strFilePath, const long lngSize)
{
    std::string strFilePathTmp = urlDecode(strFilePath);
    if (access(strFilePathTmp.c_str(), F_OK) != 0) {
        return false;
    }
    FILE* fp = fopen(strFilePathTmp.c_str(), "r+");
    ftruncate(fileno(fp), lngSize);
    fclose(fp);
    return true;
}

void FileCache::deleteDirectory(const char* path)
{
    DIR* dir = opendir(path);
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                deleteDirectory(full_path);
            }
        } else {
            remove(full_path);
        }
    }

    closedir(dir);
    rmdir(path);
}

void FileCache::getDirectoryFile(const char* path, std::vector<std::string>& vecFile)
{
    DIR* dir = opendir(path);
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s%s", path, entry->d_name);
            vecFile.push_back(full_path);
        }
    }

    closedir(dir);
}

bool FileCache::clearDirectory(const std::string& strPath)
{
    DIR* dir;
    struct dirent* entry;
    struct stat statbuf;
    char fullpath[PATH_MAX];

    if ((dir = opendir(strPath.c_str())) == nullptr) {
        perror("opendir");
        return false;
    }

    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(fullpath, sizeof(fullpath), "%s/%s", strPath.c_str(), entry->d_name);

        if (lstat(fullpath, &statbuf) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            clearDirectory(fullpath); // 递归删除子目录文件
        } else {
            if (unlink(fullpath) == -1) {
                perror("unlink");
            }
        }
    }

    closedir(dir);

    return true;
}

std::string FileCache::urlDecode(const std::string& strPath)
{
    std::string decoded;
    decoded.reserve(strPath.length()); // 预分配空间提高效率

    for (size_t i = 0; i < strPath.length(); ++i) {
        char c = strPath[i];

        if (c == '%' && i + 2 < strPath.length()) {
            // 提取两个十六进制字符
            char hex1 = strPath[i + 1];
            char hex2 = strPath[i + 2];
            // 验证十六进制字符
            if (std::isxdigit(hex1) && std::isxdigit(hex2)) {
                // 手动转换十六进制，避免异常
                int value = 0;
                if (hex1 >= '0' && hex1 <= '9')
                    value += (hex1 - '0') * 16;
                else if (hex1 >= 'A' && hex1 <= 'F')
                    value += (hex1 - 'A' + 10) * 16;
                else if (hex1 >= 'a' && hex1 <= 'f')
                    value += (hex1 - 'a' + 10) * 16;

                if (hex2 >= '0' && hex2 <= '9')
                    value += (hex2 - '0');
                else if (hex2 >= 'A' && hex2 <= 'F')
                    value += (hex2 - 'A' + 10);
                else if (hex2 >= 'a' && hex2 <= 'f')
                    value += (hex2 - 'a' + 10);

                decoded += static_cast<char>(value);
                i += 2; // 跳过两个字符
            } else {
                // 无效的百分比编码，保持原样
                decoded += c;
            }
        } else if (c == '+') {
            decoded += ' ';
        } else {
            decoded += c;
        }
    }
    return decoded;
}

std::string FileCache::urlEncode(const std::string& strPath)
{
    std::ostringstream encoded;
    encoded << std::hex << std::uppercase;

    for (unsigned char c : strPath) {
        // 空格编码为 '+'
        if (c == ' ') {
            encoded << '+';
        }
        // 字母数字和常见符号不编码
        else if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '!' || c == '*' || c == '\'' ||
                 c == '(' || c == ')' || c == ';' || c == ':' || c == '@' || c == '&' || c == '=' || c == '$' ||
                 c == ',' || c == '/' || c == '?' || c == '#' || c == '[' || c == ']') {
            encoded << c;
        }
        // 其他字符进行百分比编码
        else {
            encoded << '%' << std::setw(2) << static_cast<int>(c);
        }
    }

    return encoded.str();
}

std::string FileCache::getImageType(const std::string& base64Str)
{
    if (strncmp(base64Str.c_str(), "iVBORw0KGgo", 11) == 0) {
        return ".png"; // PNG的典型开头
    } else if (strncmp(base64Str.c_str(), "/9j/4", 5) == 0) {
        return ".jpg"; // JPEG的典型开头
    } else if (strncmp(base64Str.c_str(), "R0lGOD", 6) == 0) {
        return ".gif"; // GIF的典型开头
    } else if (strncmp(base64Str.c_str(), "Qk0", 3) == 0) {
        return ".bmp"; // BMP的典型开头
    }
    return ".jpg";
}

std::string FileCache::getMimeType(const std::string& base64Str)
{
    if (strncmp(base64Str.c_str(), "iVBORw0KGgo", 11) == 0) {
        return "image/png"; // PNG的典型开头
    } else if (strncmp(base64Str.c_str(), "/9j/4", 5) == 0) {
        return "image/jpeg"; // JPEG的典型开头
    } else if (strncmp(base64Str.c_str(), "R0lGOD", 6) == 0) {
        return "image/gif"; // GIF的典型开头
    } else if (strncmp(base64Str.c_str(), "Qk0", 3) == 0) {
        return "image/bmp"; // BMP的典型开头
    }
    return "image/jpeg";
}

std::string FileCache::generalFileName()
{
    struct timeval tv;
    struct tm* timeinfo;
    char filename[100];
    // 获取当前时间（精确到微秒）
    gettimeofday(&tv, nullptr);
    // 转换为本地时间
    timeinfo = localtime(&tv.tv_sec);
    // 格式化文件名：年月日_时分秒_微秒.txt
    strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S", timeinfo);
    // 添加微秒部分
    sprintf(filename + strlen(filename), "_%06ld", tv.tv_usec);
    return filename;
}
