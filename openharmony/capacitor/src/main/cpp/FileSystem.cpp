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

#include "FileSystem.h"
#include <filemanagement/file_uri/oh_file_uri.h>
#include "Application.h"
std::vector<CFileSystem::FileSystem> CFileSystem::s_vecFileSystems;

void CFileSystem::initFileSystem()
{
    FileSystem system;
    system.m_strName = "temporary";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/temp";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "persistent";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "content";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "assets";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "files";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "files-external";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "documents";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "sdcard";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "cache";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/cache";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "cache-external";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/cache";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);

    system.m_strName = "root";
    system.m_strRootUri =
        Application::g_databaseDir.substr(0, Application::g_databaseDir.find("database") - 1) + "/base/files";
    system.resourceApi = nullptr;
    system.preferences = nullptr;
    s_vecFileSystems.push_back(system);
    return;
}

std::string CFileSystem::getNativeUrl(const std::string& strFilePath)
{
    std::string strNativePath = strFilePath;
    if (strNativePath.find("file://") == 0) { // uri转path
        char* pathResult = nullptr;
        FileManagement_ErrCode ret = OH_FileUri_GetPathFromUri(strFilePath.c_str(), strFilePath.length(), &pathResult);
        if (ret == 0 && pathResult != nullptr) {
            strNativePath = pathResult;
            free(pathResult);
        } else {
            return strNativePath;
        }
    }

    std::string strBaseDir = Application::g_databaseDir.substr(0, Application::g_databaseDir.find("el2/database") - 1);
    if (strFilePath.find(strBaseDir) != std::string::npos) {
        strNativePath = strFilePath.substr(strFilePath.find(strBaseDir));
        return strNativePath;
    }

    if (strFilePath.find("cdvfile://") == 0) {
        std::string strPre = "cdvfile:/";
        strNativePath = strFilePath.substr(strFilePath.find(strPre) + strPre.length());
        return getNativeUrl(strNativePath);
    }
    for (int i = 0; i < s_vecFileSystems.size(); i++) {
        std::string strPre = "__cdvfile_" + s_vecFileSystems[i].m_strName + "__";
        if (strFilePath.find(strPre) != std::string::npos) {
            strNativePath = strFilePath.substr(strFilePath.find(strPre) + strPre.length());
            return strNativePath;
        }
        strPre = "localhost/" + s_vecFileSystems[i].m_strName;
        if (strFilePath.find(strPre) != std::string::npos) {
            if (strFilePath.find(strBaseDir) != std::string::npos) {
                strNativePath = strFilePath.substr(strFilePath.find(strPre) + strPre.length());
            } else {
                strNativePath = s_vecFileSystems[i].m_strRootUri;
                strNativePath += strFilePath.substr(strFilePath.find(strPre) + strPre.length());
            }
            return strNativePath;
        }
    }

    return strNativePath;
}

std::vector<CFileSystem::FileSystem> CFileSystem::getFileSystem()
{
    return s_vecFileSystems;
}