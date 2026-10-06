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

#include "rawfile_request.h"
#include "CordovaViewController.h"
#include "HttpCode.h"
#include "cJSON.h"
#include "hilog/log.h"
#include "MemPool.h"
#include "rawfile/raw_file.h"
#include "rawfile/raw_file_manager.h"
#include "Application.h"
#include "ConnPool.h"
#include "FileCache.h"
#include "FileSystem.h"
#include "HttpUrl.h"
#include "SystemCookieManager.h"
#include <cstddef>
#include <map>
#include <regex>
#include <sys/stat.h>
#include <network/netmanager/net_connection.h>
#include <network/netmanager/net_connection_type.h>

#undef LOG_TAG
#define LOG_TAG "ss-handler"

namespace {
// HttpBodyStream的读回调。
void ReadCallback(const ArkWeb_HttpBodyStream* httpBodyStream, uint8_t* buffer, int bytesRead)
{
    OH_LOG_INFO(LOG_APP, "read http body back.");
    RawfileRequest* rawfileRequest = static_cast<RawfileRequest*>(OH_ArkWebHttpBodyStream_GetUserData(httpBodyStream));
    if (bytesRead > 0) {
        rawfileRequest->m_vecRequestHttpBody.insert(
            rawfileRequest->m_vecRequestHttpBody.end(), buffer, buffer + bytesRead);
    }
    bool isEof = OH_ArkWebHttpBodyStream_IsEof(httpBodyStream);
    if (!isEof) {
        std::fill(buffer, buffer + (static_cast<CMemPool*>(Application::g_memPool))->getPageSize(), 0);
        OH_ArkWebHttpBodyStream_Read(
            httpBodyStream, buffer, (static_cast<CMemPool*>(Application::g_memPool))->getPageSize());
    } else {
        (static_cast<CMemPool*>(Application::g_memPool))->freePage(reinterpret_cast<char*>(buffer));
        if (rawfileRequest) {
            if (!(static_cast<CHttpGroupRunner*>(Application::g_httpGroupRunner))->addTask(rawfileRequest)) {
                OH_LOG_ERROR(LOG_APP, "Thread reaches boundary, adding task failed");
                rawfileRequest->response4XX(HttpCode::FORBIDDEN);
            }
        }
    }
}


// ArkWeb_HttpBodyStream的初始化回调。
void InitCallback(const ArkWeb_HttpBodyStream* httpBodyStream, ArkWeb_NetError result)
{
    OH_LOG_INFO(LOG_APP, "init http body stream done %{public}d.", result);
    bool isChunked = OH_ArkWebHttpBodyStream_IsChunked(httpBodyStream);
    OH_LOG_INFO(LOG_APP, "http body stream is chunked %{public}d.", isChunked);

    std::vector<SMemPage> vecMemPage;
    (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1);
    if (vecMemPage.size() < 1) {
        OH_LOG_ERROR(LOG_APP, "(static_cast<CMemPool*>(Application::g_memPool))->MallocPage(vecMemPage, 1) == nullptr");
        return;
    }
    unsigned char* buffer = reinterpret_cast<unsigned char*>(vecMemPage[0].m_point);
    std::fill(buffer, buffer + (static_cast<CMemPool*>(Application::g_memPool))->getPageSize(), 0);
    OH_ArkWebHttpBodyStream_Read(
        httpBodyStream, buffer, (static_cast<CMemPool*>(Application::g_memPool))->getPageSize());
}
} // namespace

RawfileRequest::RawfileRequest(const ArkWeb_ResourceRequest* resourceRequest,
                               const ArkWeb_ResourceHandler* resourceHandler,
                               const NativeResourceManager* resourceManager,
                               const ArkWeb_SchemeHandler* schemeHandler,
                               const std::string& strSchemeDomainPort)
    : m_resourceRequest(resourceRequest), m_resourceHandler(resourceHandler), m_resourceManager(resourceManager),
      m_schemeHandler(schemeHandler), m_strSchemeDomainPort(strSchemeDomainPort), m_response(nullptr)
{
    OH_ArkWeb_CreateResponse(&m_response);
}

RawfileRequest::~RawfileRequest() {}

void RawfileRequest::start()
{
    OH_LOG_INFO(LOG_APP, "start a webview request.");
    if (Application::g_strTmpUrl == "" || !Application::isValidSchemeHandler(m_schemeHandler)) {
        return;
    }

    OH_ArkWebResourceRequest_GetHttpBodyStream(m_resourceRequest, &m_stream);
    if (m_stream) {
        OH_LOG_INFO(LOG_APP, "have http body stream");
        OH_ArkWebHttpBodyStream_SetUserData(m_stream, this);
        OH_ArkWebHttpBodyStream_SetReadCallback(m_stream, ReadCallback);
        OH_ArkWebHttpBodyStream_Init(m_stream, InitCallback);
    } else {
        if (!(static_cast<CHttpGroupRunner*>(Application::g_httpGroupRunner))->addTask(this)) {
            OH_LOG_ERROR(LOG_APP, "Thread reaches boundary, adding task failed");
            response4XX(HttpCode::FORBIDDEN);
        }
    }
}

/*
 *此函数主要完成四个功能，此函数不上全局锁，已提高效率，另外此函数在可变线程池中运行
 *1，加载rawfile文件，通常情况下本地的网页放在此目录下
 *2，加载沙箱路径的本地文件，域名为localhost或者scheme为cdvfile
 *3，加载在线资源，例如js，css、img和font等
 *4，发送动态请求数据
 */
void RawfileRequest::readRawfileDataOnWorkerThread(void* pMaxBuffer, const int nBufSize, const int nCountOfTask)
{
    OH_LOG_INFO(LOG_APP, "Processing task... %{public}d", nCountOfTask);
    // 前端webview已经destroy，不再发送请求
    if (Application::g_strTmpUrl == "" || !Application::isValidSchemeHandler(m_schemeHandler) ||
        pMaxBuffer == nullptr) {
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }

    // 1，解析url
    char* url = nullptr;
    if (!webResourceRequest_GetUrl(&url)) {
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }
    std::string urlPath = url;
    if (urlPath.find("#") != std::string::npos) {
        urlPath = urlPath.substr(0, urlPath.find("#"));
    }
    std::string strBasePath = Application::getBasePath(m_schemeHandler);
    if (!strBasePath.empty() && urlPath.find(Application::g_strTmpUrl) != std::string::npos) {
        int nPos = urlPath.find("/", urlPath.find("//") + 2);
        if (nPos != std::string::npos) {
            if (strBasePath.front() == '/') {
                strBasePath = strBasePath.substr(1);
            }
            if (strBasePath.back() != '/') {
                strBasePath += "/";
            }
            urlPath.insert(nPos + 1, strBasePath);
        }
        if (!urlPath.empty() && urlPath.back() == '/') {
            urlPath += "index.html";
        }
    }
    
    urlPath = Application::getResourceObj(m_schemeHandler, urlPath);
    std::string requestAddress = "";
    OH_ArkWeb_ReleaseString(url);
    urlPath = urlPath.erase(0, urlPath.find_first_not_of(" "));
    urlPath = urlPath.erase(urlPath.find_last_not_of(" ") + 1);
    if (urlPath.find("//") != std::string::npos) {
        requestAddress = urlPath.substr(urlPath.find("//") + 2);
    }
    if (urlPath.find(Application::g_strTmpUrl) != std::string::npos) {
        std::size_t position = urlPath.find(Application::g_strTmpUrl) + Application::g_strTmpUrl.length();
        if (position != std::string::npos) {
            m_rawfilePath = urlPath.substr(position + 1);
            int afterPos = m_rawfilePath.find("#");
            if (afterPos != std::string::npos) {
                m_rawfilePath = m_rawfilePath.substr(0, afterPos);
            }
            afterPos = m_rawfilePath.find("?");
            if (afterPos != std::string::npos) {
                m_rawfilePath = m_rawfilePath.substr(0, afterPos);
            }
        }
    }

    // 2,解析http/https协议类型
    int nPort = 0;
    bool isHttps = false;
    bool isFile = false;
    if (urlPath.find("https") == 0) {
        isHttps = true;
        nPort = 443;
    } else if (urlPath.find("http") == 0) {
        isHttps = false;
        nPort = 80;
    } else if (urlPath.find("cdvfile") == 0) {
        isFile = true;
    } else {
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }

    // 3解析域名修改请求地址
    std::string strDomainAndPort;
    std::string strDomain;
    if (isFile == false && urlPath.find("//") != std::string::npos) {
        strDomain = urlPath.substr(urlPath.find("//") + 2);
        strDomainAndPort = strDomain;
        if (strDomain.find("/") != std::string::npos) {
            strDomain = strDomain.substr(0, strDomain.find("/"));
            strDomainAndPort = strDomain;
        }
        if (strDomain.find(":") != std::string::npos) {
            std::string strPort = strDomain.substr(strDomain.find(":") + 1);
            strDomain = strDomain.substr(0, strDomain.find(":"));
            nPort = atoi(strPort.c_str());
        }
        requestAddress = requestAddress.substr(strDomainAndPort.length());
    }

    // 如果是双向认证获取客户端持有的证书
    std::string strClientCertPathName;
    std::string strClientKeyPathName;
    std::string strP12File;
    std::string strPassword;
    if ((static_cast<CordovaViewController*>(Application::g_cordovaViewController))
            ->getClientCert(strDomainAndPort, strP12File, strPassword)) {
        std::string strTempUrl = strDomainAndPort;
        if (strTempUrl.find(":") != std::string::npos) {
            strTempUrl = strTempUrl.replace(strTempUrl.find(":"), 1, "_");
        }
        std::string strTempCertPath = Application::g_strCachePath + "/client_cert/" + strTempUrl + "/client_cert.cer";
        std::string strTempKeyPath = Application::g_strCachePath + "/client_cert/" + strTempUrl + "/client_key.key";
        if (!FileCache::IsFile(strTempCertPath, "") && !FileCache::IsFile(strTempKeyPath, "")) {
            RawFile* rawfile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, strP12File.c_str());
            std::vector<char> vecP12Content;
            if (rawfile != nullptr) {
                const int blockSize = 4096;
                char szBuf[blockSize];
                while (true) {
                    std::fill(szBuf, szBuf + blockSize, 0);
                    int ret = OH_ResourceManager_ReadRawFile(rawfile, szBuf, blockSize);
                    if (ret == 0) {
                        break;
                    }
                    vecP12Content.insert(vecP12Content.end(), szBuf, szBuf + ret);
                }
                OH_ResourceManager_CloseRawFile(rawfile);
            }

            if (!vecP12Content.empty()) {
                if (!FileCache::createDirectory(Application::g_strCachePath + "/client_cert/" + strTempUrl)) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "FileCache::createDirectory(Application::g_strCachePath{%{public}s}/strTempUrl{%{public}s})",
                        Application::g_strCachePath.c_str(),
                        strTempUrl.c_str());
                } else if (CSSLSocket::parsePKCS12(static_cast<char*>(&vecP12Content[0]),
                                                   vecP12Content.size(),
                                                   strPassword.c_str(),
                                                   strTempKeyPath.c_str(),
                                                   strTempCertPath.c_str()) &&
                           FileCache::IsFile(strTempKeyPath, "") && FileCache::IsFile(strTempCertPath, "")) {
                    strClientCertPathName = strTempCertPath;
                    strClientKeyPathName = strTempKeyPath;
                    OH_LOG_INFO(LOG_APP,
                                "P12 Parse success:cert:%{public}s,key:%{public}s",
                                strClientCertPathName.c_str(),
                                strClientKeyPathName.c_str());
                }
            }
        } else {
            strClientCertPathName = strTempCertPath;
            strClientKeyPathName = strTempKeyPath;
        }
    }

    std::string strCachePath = Application::g_strCachePath;
    if (strDomain == "localhost") { // 访问本地文件
        strCachePath = "";
        std::string strBaseDir =
            Application::g_databaseDir.substr(0, Application::g_databaseDir.find("el2/database") - 1);
        if (requestAddress.find(strBaseDir) != 0) { // cdv格式的文件
            requestAddress = CFileSystem::getNativeUrl(strDomain + requestAddress);
        }
        if (requestAddress.find("?") != std::string::npos) { // 源文件路径
            requestAddress = requestAddress.substr(0, requestAddress.find("?"));
        }
    }

    if (isFile) { // 访问本地文件
        strCachePath = "";
        if (requestAddress.find("/") != 0) {
            requestAddress = "/" + requestAddress;
        }
        requestAddress = CFileSystem::getNativeUrl(requestAddress);
        if (requestAddress.find("?") != std::string::npos) {
            requestAddress = requestAddress.substr(0, requestAddress.find("?"));
        }
    }

    // 4,解析method
    char* method = nullptr;
    if (!webResourceRequest_GetMethod(&method)) {
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }
    std::string strMethod = method;
    OH_ArkWeb_ReleaseString(method);


    // 5,解析请求类型
    int32_t resourceType = -1;
    if (!webResourceRequest_GetResourceType(resourceType) || resourceType == -1) {
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }

    // 6,解析header
    std::map<std::string, std::string> mapHeader;
    ArkWeb_RequestHeaderList* headerList = nullptr;
    if (!webResourceRequest_GetRequestHeaders(&headerList) || headerList == nullptr) {
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }
    int headerListSize = OH_ArkWebRequestHeaderList_GetSize(headerList);
    for (int i = 0; i < headerListSize; i++) {
        char* key;
        char* value;
        OH_ArkWebRequestHeaderList_GetHeader(headerList, i, &key, &value);
        mapHeader[key] = value;
        OH_ArkWeb_ReleaseString(key);
        OH_ArkWeb_ReleaseString(value);
    }
    OH_ArkWebRequestHeaderList_Destroy(headerList);

    // 7,查找是否设置了代理
    bool isHttpProxy = false;
    NetConn_HttpProxy httpProxy;
    std::fill(httpProxy.host, httpProxy.host + sizeof(httpProxy.host), 0);
    httpProxy.port = 0;
    if (m_rawfilePath.empty() && !isFile && strDomain != "localhost" && strDomain != Application::g_strTmpUrl &&
        OH_NetConn_GetDefaultHttpProxy(&httpProxy) == 0) {
        if (strlen(httpProxy.host) > 0 && httpProxy.port != 0) {
            isHttpProxy = true;
            isHttps = false;
        }
    }

    // 8,判断是否设置了withCredentials，true:预检请求跨域不得返回*
    bool isAllowCredentials = Application::getIsAllowCredentials(m_schemeHandler);
    std::string strCustomHttpHeaders = Application::getCustomHttpHeaders(m_schemeHandler);
    std::string strOrigin = "https://" + Application::g_strTmpUrl;
    if (mapHeader.find("Origin") != mapHeader.end()) {
        strOrigin = mapHeader["Origin"];
    }
    OH_LOG_INFO(LOG_APP, "create m_response(%{public}p)", m_response);

    /*
     *9,处理预检请求
     * 拦截并代服务器处理预检请求，以便绕过所有的跨域请求
     * 由于前后分离开发，前端开发人员无法控制和配置后端
     * 通过此方法是伪装跨域请求为同源
     * withCredentials=true时，设置跨域请求
     */
    int nArkWebRetCode = 0;
    if (strMethod == "OPTIONS") {
        if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::NO_CONTENT)) != ARKWEB_NET_OK) { // 预检请求状态码是204
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponseSetStatus(204) == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        if (isAllowCredentials) {
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Origin", strOrigin)) !=
                ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Origin\", \"%{public}s\") == %{public}d",
                        strOrigin.c_str(),
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName(
                            "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName( \"Access-Control-Allow-Methods\", \"GET, POST, PUT, "
                                 "DELETE, OPTIONS\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Headers",
                                                                     strCustomHttpHeaders)) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Headers\", \"%{public}s\") == %{public}d",
                        strCustomHttpHeaders.c_str(),
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Credentials", "true")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Credentials\", \"true\") == %{public}d",
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
        } else {
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Origin", "*")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Origin\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Methods", "*")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName( \"Access-Control-Allow-Methods\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Headers", "*")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Headers\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Credentials", "false")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Credentials\", \"false\") == %{public}d",
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
        }

        if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Max-Age", "86400")) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName( \"Access-Control-Max-Age\", \"86400\") == %{public}d",
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Keep-Alive", "timeout=2, max=60")) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"Keep-Alive\", \"timeout=2, max=60\") == %{public}d",
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Connection", "Keep-Alive")) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName( \"Connection\", \"Keep-Alive\") == %{public}d",
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        } else if ((nArkWebRetCode = webResponse_SetStatusText("OK")) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponse_SetStatusText(\"OK\") == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        didReceiveResponse();
        didFinish();
        return;
    }

    // 不是获取本地文件，也不是白名单允许的
    if (!isFile && !isAllowUrl(urlPath)) {
        response4XX(HttpCode::FORBIDDEN);
        return;
    }

    // 获取部分文件内容，计算开始字节和结束字节
    long lngRangeStart = 0; // 部分返回开始字节
    long lngRangeEnd = 0;   // 部分返回结束字节
    if (mapHeader.find("Range") != mapHeader.end()) {
        std::string strRangeValue = mapHeader["Range"];
        OH_LOG_INFO(LOG_APP, "Range:%{public}s", strRangeValue.c_str());
        if (strRangeValue.find("bytes=") != std::string::npos) {
            strRangeValue = strRangeValue.substr(6);
            if (strRangeValue.find("-") != std::string::npos) {
                std::string strRangeStart = strRangeValue.substr(0, strRangeValue.find("-"));
                std::string strRangeEnd = strRangeValue.substr(strRangeValue.find("-") + 1);
                lngRangeStart = atol(strRangeStart.c_str());
                lngRangeEnd = atol(strRangeEnd.c_str());
            }
        }
    }

    /*
     *10，发送请求获取资源
     *加载本地资源：
     *   域名为localhost从本地沙箱路径加载文件
     *   scheme是cdvfile从本地沙箱路径加载文件
     *其他为从服务器获取资源，如果服务器资源本地已经缓存，且缓存未过期，从本地缓存加载文件，不发送请求
     */
    std::string strHttpUrl = isHttps ? ("https://" + strDomainAndPort) : ("http://" + strDomainAndPort);
    if ((m_rawfilePath.empty() && (resourceType == MAIN_FRAME || resourceType == SUB_FRAME ||
                                   resourceType == STYLE_SHEET || resourceType == SCRIPT || resourceType == IMAGE ||
                                   resourceType == FONT_RESOURCE || resourceType == FAVICON)) ||
        strDomain == "localhost" || isFile) {
        /*
         *strRequestFileName为本地缓存的文件
         *沙箱路径文件，缓存时间不限制
         *网络加载的文件缓存时间默认24小时，自定义缓存时间在config.xml里面配置cordova-cache-duration参数，见文档说明
         */
        std::string strRequestFileName = "";
        if (isFile || strDomain == "localhost") {
            strRequestFileName = requestAddress; // 本地沙箱路径文件名
        } else {
            strRequestFileName = "/" + HttpUrl::generateMD5(urlPath); // 网络请求本地缓存文件名,文件名为请求url的md5值
        }

        // 在获取在线资源时，http请求body有内容，清除本地缓存，从服务器加载
        if (!isFile && strDomain != "localhost" && m_vecRequestHttpBody.size() > 0 &&
            FileCache::IsFile(strRequestFileName, strCachePath)) {
            FileCache::deleteFile(strCachePath + strRequestFileName);
        }

        // 判断在线资源本地缓存是否已超期
        if (!isFile && strDomain != "localhost" && FileCache::IsFile(strRequestFileName, strCachePath)) {
            struct stat file_stat;
            std::string strFile = strCachePath + strRequestFileName;
            if (stat(strFile.c_str(), &file_stat) == 0) {
                time_t file_mtime = file_stat.st_mtime;
                time_t current_time;
                time(&current_time);
                double dbDiffSeconds = difftime(current_time, file_mtime);
                if (dbDiffSeconds > (static_cast<CordovaViewController*>(Application::g_cordovaViewController))
                                        ->getCordovaCacheDuration()) {
                    FileCache::deleteFile(strCachePath + strRequestFileName);
                }
            }
        }

        // 部分返回文件不全，删除文件缓存
        if (!isFile && strDomain != "localhost" && mapHeader.find("Range") != mapHeader.end() &&
            FileCache::IsFile(strRequestFileName, strCachePath)) {
            FileCache::deleteFile(strCachePath + strRequestFileName);
        }

        // 开始代理发送请求,如果域名是localhost后续在第14步从本地沙箱路径加载文件
        std::map<std::string, std::string> mapRespHeaders;
        if (!strDomain.empty() && strDomain != "localhost" && !FileCache::IsFile(strRequestFileName, strCachePath)) {
            std::string strRequest = strMethod + " " + requestAddress + " HTTP/1.1\r\n"; //%s %s HTTP/1.1\r\n

            /*
             *从分页式内存管理的中申请一页内存，用于设置http头，一页内存最小1M，
             *现代浏览器http头不会超过1M大小，因此内存不会溢出
             *使用内存池内存，不用频繁的申请和释放内存，系统更稳定，使用完后注意将此页内存放回池中，避免泄漏
             */
            std::vector<SMemPage> vecMemBuf;
            (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemBuf, 1);
            if (vecMemBuf.size() < 1) {
                OH_LOG_ERROR(LOG_APP,
                             "(static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemBuf, 1) == nullptr");
                return;
            }

            char* szBuf = vecMemBuf[0].m_point;
            const int const_buf_size = (static_cast<CMemPool*>(Application::g_memPool))->getPageSize();
            if (nPort == 80 || nPort == 443) {
                snprintf(szBuf, const_buf_size, "Host:%s\r\n", strDomain.c_str());
            } else {
                snprintf(szBuf, const_buf_size, "Host:%s:%d\r\n", strDomain.c_str(), nPort);
            }
            strRequest += szBuf;

            // 设置http头，同时剔除跨域http头
            // 剔除Sec-Fetch-*头，如果您的服务器部署了基于Fetch
            // Metadata的安全策略，请求会被拒绝，后续版本迭代中会像浏览器一样增加Sec-Fetch-*头
            std::string strCookie;
            for (std::map<std::string, std::string>::iterator iter = mapHeader.begin(); iter != mapHeader.end();
                 ++iter) {
                if (iter->first == "Origin" || iter->first == "Sec-Fetch-Dest" || iter->first == "Sec-Fetch-Mode" ||
                    iter->first == "Sec-Fetch-Site" || iter->first == "If-Modified-Since" ||
                    iter->first == "If-None-Match") {
                    continue;
                }
                if (strcasecmp(iter->first.c_str(), "cookie") == 0) {
                    strCookie = iter->second;
                    continue;
                }
                snprintf(szBuf, const_buf_size, "%s:%s\r\n", iter->first.c_str(), iter->second.c_str());
                strRequest += szBuf;
            }

            strCookie = SystemCookieManager::getCookie(urlPath, strCookie);
            if (!strCookie.empty()) {
                strRequest += "Cookie:";
                strRequest += strCookie;
                strRequest += "\r\n";
            }

            snprintf(szBuf, const_buf_size, "Connection:Keep-Alive\r\n");
            strRequest += szBuf;

            if (m_vecRequestHttpBody.size() > 0) {
                snprintf(szBuf, const_buf_size, "Keep-Alive:timeout=5, max=1000\r\n");
                strRequest += szBuf;
                snprintf(szBuf, const_buf_size, "Content-Length:%lu\r\n\r\n", m_vecRequestHttpBody.size());
                strRequest += szBuf;
                strRequest.insert(strRequest.length(),
                                  reinterpret_cast<const char*>(&m_vecRequestHttpBody[0]),
                                  m_vecRequestHttpBody.size());
            } else {
                snprintf(szBuf, const_buf_size, "Keep-Alive:timeout=5, max=1000\r\n\r\n");
                strRequest += szBuf;
            }
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemBuf);

            // 如果webview设置了代理，使用代理地址转发保持和webview一致
            if (isHttpProxy) {
                strDomain = httpProxy.host;
                nPort = httpProxy.port;
            }

            ConnPoolManage* connPoolManage = static_cast<ConnPoolManage*>(Application::g_connPoolManage);
            ConnPool* pool = connPoolManage->getConnPool(strDomain);
            if (pool == nullptr) {
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }

            // 开始实际发送请求
            std::fill(static_cast<char*>(pMaxBuffer), static_cast<char*>(pMaxBuffer) + nBufSize, 0);
            if (isHttps) {
                bool isClose = false;
                SSL* ssl = nullptr;
                if (!pool->getSSL(&ssl,
                                  nPort,
                                  CONNECT_TIME_OUT,
                                  TLS1_2_VERSION,
                                  "",
                                  strClientCertPathName,
                                  strClientKeyPathName)) {
                    OH_LOG_ERROR(LOG_APP,
                                 "pool->getSSL(&ssl, nPort, CONNECT_TIME_OUT,TLS1_2_VERSION, "
                                 ", strClientCertPathName{%{public}s}, strClientKeyPathName{%{public}s}) == false",
                                 strClientCertPathName.c_str(),
                                 strClientKeyPathName.c_str());
                } else if (!pool->sendWithTimeOut(ssl, strRequest, std::ref(m_stopped))) {
                    OH_LOG_ERROR(LOG_APP, "pool->sendWithTimeOut(ssl, strRequest) == false");
                } else if (!pool->recvHttpWithTimeOut(ssl,
                                                      strMethod,
                                                      strRequestFileName,
                                                      mapRespHeaders,
                                                      strCachePath,
                                                      pMaxBuffer,
                                                      nBufSize,
                                                      isClose,
                                                      std::ref(m_stopped))) {
                    OH_LOG_ERROR(LOG_APP,
                                 "pool->recvHttpWithTimeOut(ssl, strMethod, strRequestFileName, "
                                 "mapRespHeaders, pMaxBuffer, nBufSize, isReconnect) == false");
                }
                pool->freeSocket(ssl, true);
            } else {
                bool isClose = false;
                int nSocket = -1;
                if (!pool->getSocket(nSocket, nPort)) {
                    OH_LOG_ERROR(LOG_APP, "pool->getSocket(nSocket) == false");
                } else if (!pool->sendWithTimeOut(nSocket, strRequest, std::ref(m_stopped))) {
                    OH_LOG_ERROR(LOG_APP, "pool->sendWithTimeOut(nSocket, strRequest) == false");
                } else if (!pool->recvHttpWithTimeOut(nSocket,
                                                      strMethod,
                                                      strRequestFileName,
                                                      mapRespHeaders,
                                                      strCachePath,
                                                      pMaxBuffer,
                                                      nBufSize,
                                                      isClose,
                                                      std::ref(m_stopped))) {
                    OH_LOG_ERROR(LOG_APP,
                                 "pool->recvHttpWithTimeOut(nSocket, strMethod, strRequestFileName, "
                                 "pMaxBuffer, nBufSize, isReconnect) == false");
                }
                pool->freeSocket(nSocket, true);
            }

            if (m_stopped) {
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }

            int httpCode = pool->getHttpCode(static_cast<char*>(pMaxBuffer), strlen(static_cast<char*>(pMaxBuffer)));
            // 重定向返回，webview会自动重新发起新的请求
            if (HttpCode::isRedirection(httpCode)) {
                if ((nArkWebRetCode = webResponse_SetStatus(httpCode)) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetStatus(httpCode{%{public}d}) == %{public}d",
                                     httpCode,
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    return;
                }
                for (std::map<std::string, std::string>::iterator iter = mapRespHeaders.begin();
                     iter != mapRespHeaders.end();
                     ++iter) {
                    if ((nArkWebRetCode = webResponse_SetHeaderByName(iter->first, iter->second)) != ARKWEB_NET_OK) {
                        if (nArkWebRetCode) {
                            OH_LOG_ERROR(LOG_APP,
                                         "webResponse_SetHeaderByName(iter->first.c_str(){%{public}s}, "
                                         "iter->second.c_str(){%{public}s})) == %{public}d",
                                         iter->first.c_str(),
                                         iter->second.c_str(),
                                         nArkWebRetCode);
                        }
                        response4XX(HttpCode::BAD_REQUEST);
                        return;
                    }
                }
                didReceiveResponse();
                didFinish();
                return;
            }

            // 请求资源只有状态码200才会有正确的资源数据返回
            if (httpCode != HttpCode::OK) {
                response4XX(httpCode);
                return;
            }
        }

        // 服务端请求成功后，缓存到本地文件，或者直接就是本地文件
        if (!FileCache::IsFile(strRequestFileName, strCachePath)) {
            response4XX(HttpCode::NOT_FOUND);
            return;
        }

        // 设置httpcode
        if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::OK)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponse_SetStatus(HttpCode::OK{200}) == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        // 设置mime
        std::string strMimeType = HttpUrl::getMimeType(resourceType, urlPath);
        if ((nArkWebRetCode = webResponse_SetMimeType(strMimeType)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetMimeType(strMimeType{%{public}s}) == %{public}d",
                             strMimeType.c_str(),
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        // 自定义设置http头的相关跨域头，绕过webview的跨域拦截
        if (isAllowCredentials) {
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Origin", strOrigin)) !=
                ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Origin\", \"%{public}s\") == %{public}d",
                        strOrigin.c_str(),
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName(
                            "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName( \"Access-Control-Allow-Methods\", \"GET, POST, PUT, "
                                 "DELETE, OPTIONS\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Headers",
                                                                     strCustomHttpHeaders)) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Headers\", \"%{public}s\") == %{public}d",
                        strCustomHttpHeaders.c_str(),
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Credentials", "true")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Credentials\", \"true\") == %{public}d",
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
        } else {
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Origin", "*")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Origin\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Methods", "*")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Methods\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Headers", "*")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Headers\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Credentials", "false")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Credentials\", \"false\") == %{public}d",
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
        }

        /*
         *自定义设置response的http头
         *自定义处理服务器返回的Set-Cookie头
         */
        for (std::map<std::string, std::string>::iterator iter = mapRespHeaders.begin(); iter != mapRespHeaders.end();
             ++iter) {
            if (strcasecmp(iter->first.c_str(), "Access-Control-Allow-Origin") != 0 &&
                strcasecmp(iter->first.c_str(), "Access-Control-Allow-Methods") != 0 &&
                strcasecmp(iter->first.c_str(), "Access-Control-Allow-Headers") != 0 &&
                strcasecmp(iter->first.c_str(), "Access-Control-Allow-Credentials") != 0 &&
                strcasecmp(iter->first.c_str(), "Transfer-Encoding") != 0 &&
                strcasecmp(iter->first.c_str(), "Content-Encoding") != 0 &&
                strcasecmp(iter->first.c_str(), "Content-Length") != 0 &&
                strcasecmp(iter->first.c_str(), "Content-Type") != 0) {
                if ((nArkWebRetCode = webResponse_SetHeaderByName(iter->first, iter->second)) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetHeaderByName(iter->first.c_str(){%{public}s}, "
                                     "iter->second.c_str(){%{public}s}) == %{public}d",
                                     iter->first.c_str(),
                                     iter->second.c_str(),
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    return;
                }
            }

            if (strcasecmp(iter->first.c_str(), "Content-Type") == 0) {
                std::string strMimeType = iter->second;
                std::string strCharset = "";
                if (strMimeType.find(";") != std::string::npos) {
                    strCharset = strMimeType.substr(strMimeType.find(";") + 1);
                    strMimeType = strMimeType.substr(0, strMimeType.find(";"));
                }
                if ((nArkWebRetCode = webResponse_SetMimeType(strMimeType)) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetMimeType(strMimeType.c_str(){%{public}s})== %{public}d",
                                     strMimeType.c_str(),
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    return;
                }
                if (strCharset.find("charset=") != std::string::npos) {
                    strCharset = strCharset.substr(strCharset.find("=") + 1);
                }
                if (!strCharset.empty()) {
                    if ((nArkWebRetCode = webResponse_SetCharset(strCharset)) != ARKWEB_NET_OK) {
                        if (nArkWebRetCode) {
                            OH_LOG_ERROR(LOG_APP,
                                         "webResponse_SetCharset(strCharset.c_str(){%{public}s})== %{public}d",
                                         strCharset.c_str(),
                                         nArkWebRetCode);
                        }
                        response4XX(HttpCode::BAD_REQUEST);
                        return;
                    }
                }
            }

            if (strcasecmp(iter->first.c_str(), "Set-Cookie") == 0) {
                std::vector<std::string> vecCordovaCookie;
                vecCordovaCookie.push_back(iter->second);
                SystemCookieManager::setCookie(strHttpUrl, vecCordovaCookie);
            }
        }

        // 设置http的内容长度
        FILE* file = FileCache::openFile(strRequestFileName, strCachePath);
        if (file == nullptr) {
            response4XX(HttpCode::NOT_FOUND);
            return;
        }
        long fileSize = FileCache::getFileSize(file);
        char szBuf[256];
        snprintf(szBuf, sizeof(szBuf), "%ld", fileSize);
        if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Length", szBuf)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}s})== %{public}d",
                             szBuf,
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        // 设置返回的response的数据
        std::vector<SMemPage> vecMemPage;
        const int blockSize = (static_cast<CMemPool*>(Application::g_memPool))->getPageSize();
        long consumed = 0;
        long totalConsumed = fileSize;
        (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1);
        if (vecMemPage.size() < 1) {
            OH_LOG_ERROR(LOG_APP,
                         "(static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1) == nullptr");
            response4XX(HttpCode::BAD_REQUEST);
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            return;
        }

        // webview请求部分返回，
        if (mapHeader.find("Range") != mapHeader.end()) {
            // 部分返回设定状态码为206
            if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::PARTIAL_CONTENT)) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP, "webResponse_SetStatus(HttpCode::PARTIAL_CONTENT{206}) == %{public}d", nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                return;
            }

            // localhost访问的是本地文件，对文件进行拆分返回webview
            if (strDomain == "localhost" || isFile) {
                consumed = lngRangeStart > fileSize ? fileSize : lngRangeStart;
                if (lngRangeEnd == 0) {
                    lngRangeEnd = fileSize < blockSize ? fileSize : blockSize;
                    lngRangeEnd += consumed;
                }
                if (lngRangeEnd >= fileSize) {
                    lngRangeEnd = fileSize;
                }
                totalConsumed = lngRangeEnd;

                snprintf(szBuf, sizeof(szBuf), "%ld", lngRangeEnd - consumed);
                OH_LOG_INFO(LOG_APP, "Content-Length:%{public}d", lngRangeEnd - consumed);
                if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Length", szBuf)) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}s})== %{public}d",
                                     szBuf,
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                    return;
                }
                OH_LOG_INFO(LOG_APP, "Accept-Ranges:bytes", szBuf);
                if ((nArkWebRetCode = webResponse_SetHeaderByName("Accept-Ranges", "bytes")) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetHeaderByName(\"Content-Length\", \"bytes\"})== %{public}d",
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                    return;
                }
                snprintf(szBuf, sizeof(szBuf), "bytes %ld-%ld/%ld", lngRangeStart, lngRangeEnd - 1, fileSize);
                OH_LOG_INFO(LOG_APP, "Content-Range:%{public}s", szBuf);
                if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Range", szBuf)) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}s})== %{public}d",
                                     szBuf,
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                    return;
                }
            }
        }
        didReceiveResponse(); // 返回的http头设置完毕

        unsigned char* buffer = reinterpret_cast<unsigned char*>(vecMemPage[0].m_point);
        while (true) {
            int ret = FileCache::readFile(file, buffer, blockSize, consumed);
            OH_LOG_INFO(LOG_APP, "read cacheFile %{public}d bytes.", ret);
            if (ret == 0) {
                break;
            }
            if (consumed + ret > totalConsumed) {
                // 读取内容大于了要求返回的内容，只返回部分内容
                ret = totalConsumed - consumed;
            }
            consumed += ret;
            didReceiveData(buffer, ret);
            std::fill(buffer, buffer + blockSize, 0);
            if (consumed >= totalConsumed) {
                break;
            }
        }
        (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
        FileCache::closeFile(file);
        didFinish();
        return;
    }
    // 处理第10步结束（发送请求获取静态资源数）

    // 11 发送请求获取动态资源数据
    if (m_rawfilePath.empty() && (resourceType == XHR || resourceType == MEDIA)) {
        std::string strRequest = strMethod + " " + requestAddress + " HTTP/1.1\r\n"; //%s %s HTTP/1.1\r\n

        // 从内存池中取1页，设置http头
        std::vector<SMemPage> vecMemBuf;
        (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemBuf, 1);
        if (vecMemBuf.size() < 1) {
            OH_LOG_ERROR(LOG_APP,
                         "(static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemBuf, 1) == nullptr");
            return;
        }

        char* szBuf = vecMemBuf[0].m_point;
        const int const_buf_size = (static_cast<CMemPool*>(Application::g_memPool))->getPageSize();
        if (nPort == 80 || nPort == 443) {
            snprintf(szBuf, const_buf_size, "Host:%s\r\n", strDomain.c_str());
        } else {
            snprintf(szBuf, const_buf_size, "Host:%s:%d\r\n", strDomain.c_str(), nPort);
        }
        strRequest += szBuf;

        // 设置http头，剔除相关跨域头
        // 剔除Sec-Fetch-*头，如果您的服务器部署了基于Fetch
        // Metadata的安全策略，请求会被拒绝，后续版本迭代中会像浏览器一样增加Sec-Fetch-*头
        std::string strCookie;
        for (std::map<std::string, std::string>::iterator iter = mapHeader.begin(); iter != mapHeader.end(); ++iter) {
            if (iter->first == "Origin" || iter->first == "Sec-Fetch-Dest" || iter->first == "Sec-Fetch-Mode" ||
                iter->first == "Sec-Fetch-Site" || iter->first == "If-Modified-Since" ||
                iter->first == "If-None-Match") {
                continue;
            }
            if (strcasecmp(iter->first.c_str(), "cookie") == 0) {
                strCookie = iter->second;
                continue;
            }

            snprintf(szBuf, const_buf_size, "%s:%s\r\n", iter->first.c_str(), iter->second.c_str());
            strRequest += szBuf;
        }

        strCookie = SystemCookieManager::getCookie(urlPath, strCookie);
        if (!strCookie.empty()) {
            strRequest += "Cookie:";
            strRequest += strCookie;
            strRequest += "\r\n";
        }

        snprintf(szBuf, const_buf_size, "Connection:Keep-Alive\r\n");
        strRequest += szBuf;

        snprintf(szBuf, const_buf_size, "Keep-Alive:timeout=5, max=1000\r\n");
        strRequest += szBuf;

        snprintf(szBuf, const_buf_size, "Content-Length:%lu\r\n\r\n", m_vecRequestHttpBody.size());
        strRequest += szBuf;
        (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemBuf);

        // 设置http的body数据
        strRequest.insert(
            strRequest.length(), reinterpret_cast<const char*>(&m_vecRequestHttpBody[0]), m_vecRequestHttpBody.size());

        // 检查webview是否有代理，如果代理，发往代理服务器
        if (isHttpProxy) {
            strDomain = httpProxy.host;
            nPort = httpProxy.port;
        }
        ConnPoolManage* connPoolManage = static_cast<ConnPoolManage*>(Application::g_connPoolManage);
        ConnPool* pool = connPoolManage->getConnPool(strDomain);
        if (pool == nullptr) {
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        // 开始发往web服务器处理请求
        std::vector<SMemPage> vecMemPage;
        if (isHttps) {
            bool isClose = false;
            SSL* ssl = nullptr;
            if (!pool->getSSL(
                    &ssl, nPort, CONNECT_TIME_OUT, TLS1_2_VERSION, "", strClientCertPathName, strClientKeyPathName)) {
                OH_LOG_ERROR(LOG_APP,
                             "pool->getSSL(&ssl, nPort, CONNECT_TIME_OUT,TLS1_2_VERSION, "
                             ", strClientCertPathName{%{public}s}, strClientKeyPathName{%{public}s}) == false",
                             strClientCertPathName.c_str(),
                             strClientKeyPathName.c_str());
            } else if (!pool->sendWithTimeOut(ssl, strRequest, std::ref(m_stopped))) {
                OH_LOG_ERROR(LOG_APP, "pool->sendWithTimeOut(ssl, strRequest) == false");
            } else if (!pool->recvHttpWithTimeOut(
                           ssl, strMethod, vecMemPage, pMaxBuffer, nBufSize, isClose, std::ref(m_stopped))) {
                (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            }
            pool->freeSocket(ssl, true);
        } else {
            bool isClose = false;
            int nSocket = -1;
            if (!pool->getSocket(nSocket, nPort)) {
                OH_LOG_ERROR(LOG_APP, "pool->getSocket(&nSocket) == false");
            } else if (!pool->sendWithTimeOut(nSocket, strRequest, std::ref(m_stopped))) {
                OH_LOG_ERROR(LOG_APP, "pool->sendWithTimeOut(nSocket, strRequest) == false");
            } else if (!pool->recvHttpWithTimeOut(
                           nSocket, strMethod, vecMemPage, pMaxBuffer, nBufSize, isClose, std::ref(m_stopped))) {
                (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            }
            pool->freeSocket(nSocket, true);
        }

        if (m_stopped) {
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        if (vecMemPage.empty()) {
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        // 获取response的状态码
        int httpCode = pool->getHttpCode(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
        if (httpCode == 0) {
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        // 获取response的http头
        std::map<std::string, std::string> mapRespHeaders;
        if (!pool->getHttpHeaders(vecMemPage[0].m_point, vecMemPage[0].m_nCount, mapRespHeaders)) {
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        // 获取返回的Set-Cookie头，并设置cookie
        std::vector<std::string> vecCordovaCookie;
        pool->getCookies(vecMemPage[0].m_point, vecMemPage[0].m_nCount, vecCordovaCookie);
        SystemCookieManager::setCookie(strHttpUrl, vecCordovaCookie);


        /*
         *计算实际接收的数据长度
         *Content-Length=nAllCount(数据总长度)-nHeaderPos(http头结束位置)
         *Transfer-Encoding：必须经过内部数据处理后返回给webview，webview不会自行处理
         *   分段接收：Transfer-Encoding:chunked，内部已经处理，删除了分段标记(长度\r\n),返回到前端已经是实际长度
         *   数据压缩：Transfer-Encoding:gzip，内部已解压，返回给webview是实际解压后的数据
         *   Transfer-Encoding:compress 不支持
         *   Transfer-Encoding:deflate 不支持
         *Content-Encoding：无论数据是什么格式，原数据返回给webview，有webview处理
         *httpCode：206:原数据返回给webview
         */
        int nHeaderPos = pool->getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
        int nAllCount = (static_cast<CMemPool*>(Application::g_memPool))->getMemPageUseLength(vecMemPage);
        char szContentLength[64];
        snprintf(szContentLength, sizeof(szContentLength), "%d", nAllCount - nHeaderPos);
        if ((nArkWebRetCode = webResponse_SetStatus(httpCode)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(
                    LOG_APP, "webResponse_SetStatus(httpCode{%{public}d}) == %{public}d", httpCode, nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        if ((nArkWebRetCode = webResponse_SetCharset("UTF-8")) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponse_SetCharset(\"UTF-8\")== %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Length", szContentLength)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}d})== %{public}d",
                             szContentLength,
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        if (isAllowCredentials) {
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Origin", strOrigin)) !=
                ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Origin\", \"%{public}s\")== %{public}d",
                        strOrigin.c_str(),
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Methods",
                                                              "GET, POST, PUT, DELETE, OPTIONS")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Methods\", \"POST, GET, "
                                 "OPTIONS\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Headers", strCustomHttpHeaders)) !=
                ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Headers\", \"%{public}s\") == %{public}d",
                        strCustomHttpHeaders.c_str(),
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Credentials", "true")) !=
                ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Credentials\", \"true\") == %{public}d",
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
        } else {
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Origin", "*")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Origin\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Methods", "*")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Methods\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Headers", "*")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Allow-Headers\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            } else if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Allow-Credentials", "false")) !=
                       ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP,
                        "webResponse_SetHeaderByName(\"Access-Control-Allow-Credentials\", \"false\") == %{public}d",
                        nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
        }

        // 过滤跨域标记、Transfer-Encoding和Content-Length
        std::string isAcceptRanges = pool->getAcceptRanges(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
        for (std::map<std::string, std::string>::iterator iter = mapRespHeaders.begin(); iter != mapRespHeaders.end();
             ++iter) {
            if (strcasecmp(iter->first.c_str(), "Access-Control-Allow-Origin") != 0 &&
                strcasecmp(iter->first.c_str(), "Access-Control-Allow-Methods") != 0 &&
                strcasecmp(iter->first.c_str(), "Access-Control-Allow-Headers") != 0 &&
                strcasecmp(iter->first.c_str(), "Access-Control-Allow-Credentials") != 0 &&
                strcasecmp(iter->first.c_str(), "Transfer-Encoding") != 0 &&
                strcasecmp(iter->first.c_str(), "Content-Length") != 0) {
                if ((nArkWebRetCode = webResponse_SetHeaderByName(iter->first, iter->second)) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetHeaderByName(iter->first.c_str(){%{public}s}, "
                                     "iter->second.c_str(){%{public}s}) == %{public}d",
                                     iter->first.c_str(),
                                     iter->second.c_str(),
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    return;
                }
            }

            // 收到206部分返回，重新设定Content-Length使用响应头返回的数据长度
            if (strcasecmp(iter->first.c_str(), "Content-Length") == 0 && isAcceptRanges == "bytes" &&
                httpCode == HttpCode::PARTIAL_CONTENT) {
                if ((nArkWebRetCode = webResponse_SetHeaderByName(iter->first, iter->second)) != ARKWEB_NET_OK) {
                    if (nArkWebRetCode) {
                        OH_LOG_ERROR(LOG_APP,
                                     "webResponse_SetHeaderByName(iter->first.c_str(){%{public}s}, "
                                     "iter->second.c_str(){%{public}s}) == %{public}d",
                                     iter->first.c_str(),
                                     iter->second.c_str(),
                                     nArkWebRetCode);
                    }
                    response4XX(HttpCode::BAD_REQUEST);
                    return;
                }
            }
        }

        // Access-Control-Expose-Headers:跨域请求，允许暴露个给js的http头，如果服务器没有设置，cordova默认将所有头都暴露，否则按照服务器的设置返回
        if (mapRespHeaders.find("Access-Control-Expose-Headers") == mapRespHeaders.end() &&
            mapRespHeaders.find("access-control-expose-headers") == mapRespHeaders.end()) {
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Access-Control-Expose-Headers", "*")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Access-Control-Expose-Headers\", \"*\") == %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                return;
            }
        }

        // 响应头设置完毕，打印返回响应数据内存，最大打印2048个字符
        const int const_buf_size_tmp = 2048;
        char szBufTmp[const_buf_size_tmp];
        std::fill(szBufTmp, szBufTmp + const_buf_size_tmp, 0);
        std::copy_n(vecMemPage[0].m_point,
                    nHeaderPos > const_buf_size_tmp || nHeaderPos > vecMemPage[0].m_nCount ? const_buf_size_tmp
                                                                                           : nHeaderPos,
                    szBufTmp);
        szBufTmp[const_buf_size_tmp - 1] = 0;
        OH_LOG_INFO(LOG_APP, "response:%{public}s", szBufTmp);
        didReceiveResponse();


        // 设置返回的数据内容
        int nBodyPos = pool->getHttpHeader(vecMemPage[0].m_point, vecMemPage[0].m_nCount);
        for (int i = 0; i < vecMemPage.size(); i++) {
            char* pResData = nullptr;
            int nResLength = 0;
            if (i == 0) {
                pResData = vecMemPage[i].m_point + nBodyPos;
                nResLength = vecMemPage[i].m_nCount - nBodyPos;
            } else {
                pResData = vecMemPage[i].m_point;
                nResLength = vecMemPage[i].m_nCount;
            }
            didReceiveData(reinterpret_cast<unsigned char*>(pResData), nResLength);
        }
        (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
        didFinish();
        return;
    }
    // 处理第11步结束（发送请求获取动态资源数据）

    /*
     *12:根据虚拟域名加载本地沙箱文件或者从rawfile加载文件
     *   第10步是从本地沙箱路径或者web服务器端加载静态资源文件
     *   第11步是从web服务器，动态处理请求
     *   虽然第12步也是有加载本地沙箱文件，第10步也有加载本地沙箱文件，区别在于第12步是通过虚拟域名加载的，第10步是localhost域名或者cdvfile加载
     */
    if (m_rawfilePath.empty()) {
        response4XX(HttpCode::FORBIDDEN);
        return;
    }

    if (!m_resourceManager) {
        OH_LOG_ERROR(LOG_APP, "read rawfile error, resource manager is nullptr.");
        response4XX(HttpCode::FORBIDDEN);
        return;
    }

    std::string strMimeType = HttpUrl::getMimeType(m_rawfilePath);
    if ((nArkWebRetCode = webResponse_SetMimeType(strMimeType)) != ARKWEB_NET_OK) {
        if (nArkWebRetCode) {
            OH_LOG_ERROR(LOG_APP,
                         "webResponse_SetMimeType(strMimeType{%{public}s}) == %{public}d",
                         strMimeType.c_str(),
                         nArkWebRetCode);
        }
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }

    if ((nArkWebRetCode = webResponse_SetCharset("UTF-8")) != ARKWEB_NET_OK) {
        if (nArkWebRetCode) {
            OH_LOG_ERROR(LOG_APP, "webResponse_SetCharset(\"UTF-8\") == %{public}d", nArkWebRetCode);
        }
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }

    /*
     *rawfile资源目录是只读的，所有自定义webview或者热更新推送的代码，存在热更新目录下，热更新目录有插件设定
     *说明：从rawfile加载资源时，如果热更新目录下有目录结构和名字相对一致的，优先从热更新目录下加载
     */
    std::string strFilePath = Application::g_strHotCodeUpdateDirectory;
    if (!m_rawfilePath.empty() && !strFilePath.empty() && FileCache::IsFile("/" + m_rawfilePath, strFilePath)) {
        if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::OK)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponse_SetStatus(200) == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        if ((nArkWebRetCode = webResponse_SetCharset("UTF-8")) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponse_SetCharset(\"UTF-8\") == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        long len = FileCache::getFileSize(strFilePath + "/" + m_rawfilePath);
        if ((nArkWebRetCode = webResponse_SetHeaderByName("content-length", std::to_string(len))) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"content-length\", std::to_string(len).c_str(){%{public}d}, "
                             "false) == %{public}d",
                             std::to_string(len).c_str(),
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        didReceiveResponse();

        FILE* file = FileCache::openFile("/" + m_rawfilePath, strFilePath);
        if (file == nullptr) {
            OH_LOG_ERROR(LOG_APP,
                         "FileCache::openFile(m_rawfilePath{%{public}s}, strFilePath{%{public}s}) == nullptr",
                         m_rawfilePath.c_str(),
                         strFilePath.c_str());
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        std::vector<SMemPage> vecMemPage;
        const int blockSize = (static_cast<CMemPool*>(Application::g_memPool))->getPageSize();
        long consumed = 0;
        (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1);
        if (vecMemPage.size() < 1) {
            OH_LOG_ERROR(LOG_APP,
                         "(static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1) == nullptr");
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        unsigned char* buffer = reinterpret_cast<unsigned char*>(vecMemPage[0].m_point);
        while (true) {
            int ret = FileCache::readFile(file, buffer, blockSize, consumed);
            OH_LOG_INFO(LOG_APP, "read cacheFile %{public}d bytes. file:%{public}s", ret, m_rawfilePath.c_str());
            if (ret == 0) {
                break;
            }
            consumed += ret;
            didReceiveData(buffer, ret);
            std::fill(buffer, buffer + blockSize, 0);
        }
        FileCache::closeFile(file);
        (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
        didFinish();
        return;
    }

    /*从沙箱目录下加载文件
     *路径:https://www.example.com/data/storage/el2/base/file/index.html在当前步骤处理（第14步），现实开发中，此中情况比较少用
     *路径:cdvfile://data/storage/el2/base/file/index.html在第10步已处理
     *路径:https://localhost/data/storage/el2/base/file/index.html在第10步已处理
     *m_rawfilePath：文件路径包含文件名，前面并没有"/"，从本地沙箱路径需加上"/"
     */
    std::string strBaseDir = Application::g_databaseDir.substr(1, Application::g_databaseDir.find("el2/database") - 1);
    if (m_rawfilePath.find(strBaseDir) != std::string::npos) {
        strFilePath = "/";
        if (!FileCache::IsFile(m_rawfilePath, strFilePath)) {
            response4XX(HttpCode::NOT_FOUND);
            return;
        }

        if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::OK)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponse_SetStatus(200) == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        long fileSize = FileCache::getFileSize(strFilePath + m_rawfilePath);
        if ((nArkWebRetCode = webResponse_SetHeaderByName("content-length", std::to_string(fileSize).c_str())) !=
            ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"content-length\", std::to_string(len).c_str(){%{public}d}, "
                             "false) == %{public}d",
                             std::to_string(fileSize).c_str(),
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
        std::vector<SMemPage> vecMemPage;
        const int blockSize = (static_cast<CMemPool*>(Application::g_memPool))->getPageSize();
        long consumed = 0;
        long totalConsumed = fileSize;
        (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1);
        if (vecMemPage.size() < 1) {
            OH_LOG_ERROR(LOG_APP,
                         "(static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1) == nullptr");
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }

        if (mapHeader.find("Range") != mapHeader.end()) {
            // 部分返回设定状态码为206
            if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::PARTIAL_CONTENT)) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(
                        LOG_APP, "webResponse_SetStatus(HttpCode::PARTIAL_CONTENT{206}) == %{public}d", nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                return;
            }

            consumed = lngRangeStart > fileSize ? fileSize : lngRangeStart;
            if (lngRangeEnd == 0) {
                lngRangeEnd = fileSize < blockSize ? fileSize : blockSize;
                lngRangeEnd += consumed;
            }
            if (lngRangeEnd >= fileSize) {
                lngRangeEnd = fileSize;
            }
            totalConsumed = lngRangeEnd;
            char szBuf[256];
            snprintf(szBuf, sizeof(szBuf), "%ld", lngRangeEnd - consumed);
            OH_LOG_INFO(LOG_APP, "Content-Length:%{public}d", lngRangeEnd - consumed);
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Length", szBuf)) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}s})== %{public}d",
                                 szBuf,
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                return;
            }
            OH_LOG_INFO(LOG_APP, "Accept-Ranges:bytes", szBuf);
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Accept-Ranges", "bytes")) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Content-Length\", \"bytes\"})== %{public}d",
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                return;
            }
            snprintf(szBuf, sizeof(szBuf), "bytes %ld-%ld/%ld", lngRangeStart, lngRangeEnd - 1, fileSize);
            OH_LOG_INFO(LOG_APP, "Content-Range:%{public}s", szBuf);
            if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Range", szBuf)) != ARKWEB_NET_OK) {
                if (nArkWebRetCode) {
                    OH_LOG_ERROR(LOG_APP,
                                 "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}s})== %{public}d",
                                 szBuf,
                                 nArkWebRetCode);
                }
                response4XX(HttpCode::BAD_REQUEST);
                (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
                return;
            }
        }
        didReceiveResponse();

        FILE* file = FileCache::openFile(strFilePath + m_rawfilePath, "");
        if (file == nullptr) {
            OH_LOG_ERROR(LOG_APP,
                         "FileCache::openFile(m_rawfilePath{%{public}s}, strFilePath{%{public}s}) == nullptr",
                         m_rawfilePath.c_str(),
                         strFilePath.c_str());
            response4XX(HttpCode::NOT_FOUND);
            return;
        }

        unsigned char* buffer = reinterpret_cast<unsigned char*>(vecMemPage[0].m_point);
        while (true) {
            int ret = FileCache::readFile(file, buffer, blockSize, consumed);
            OH_LOG_INFO(LOG_APP, "read cacheFile %{public}d bytes. file:%{public}s", ret, m_rawfilePath.c_str());
            if (ret == 0) {
                break;
            }
            if (consumed + ret > totalConsumed) {
                // 读取内容大于了要求返回的内容，只返回部分内容
                ret = totalConsumed - consumed;
            }
            consumed += ret;
            didReceiveData(buffer, ret);
            std::fill(buffer, buffer + blockSize, 0);
            if (consumed >= totalConsumed) {
                break;
            }
        }
        FileCache::closeFile(file);
        (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
        didFinish();
        return;
    }

    // 从rawfile加载资源文件
    RawFile* rawfile = OH_ResourceManager_OpenRawFile(m_resourceManager, m_rawfilePath.c_str());
    if (!rawfile) {
        response4XX(HttpCode::NOT_FOUND);
        return;
    } else {
        if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::OK)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP, "webResponse_SetStatus(HttpCode::OK{200}) == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            return;
        }
    }

    long fileSize = OH_ResourceManager_GetRawFileSize(rawfile);
    if ((nArkWebRetCode = webResponse_SetHeaderByName("content-length", std::to_string(fileSize).c_str())) !=
        ARKWEB_NET_OK) {
        if (nArkWebRetCode != 0) {
            OH_LOG_ERROR(LOG_APP,
                         "webResponse_SetHeaderByName(\"content-length\", std::to_string(len).c_str(){%{public}d}) == "
                         "%{public}d",
                         std::to_string(fileSize).c_str(),
                         nArkWebRetCode);
        }
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }
    std::vector<SMemPage> vecMemPage;
    const int blockSize = (static_cast<CMemPool*>(Application::g_memPool))->getPageSize();
    long consumed = 0;
    long totalConsumed = fileSize;
    (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1);
    if (vecMemPage.size() < 1) {
        OH_LOG_ERROR(LOG_APP, "(static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemPage, 1) == nullptr");
        response4XX(HttpCode::BAD_REQUEST);
        return;
    }
    // 数据只需部分返回
    if (mapHeader.find("Range") != mapHeader.end()) {
        // 部分返回设定状态码为206
        if ((nArkWebRetCode = webResponse_SetStatus(HttpCode::PARTIAL_CONTENT)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(
                    LOG_APP, "webResponse_SetStatus(HttpCode::PARTIAL_CONTENT{206}) == %{public}d", nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            return;
        }

        consumed = lngRangeStart > fileSize ? fileSize : lngRangeStart;
        if (lngRangeEnd == 0) {
            lngRangeEnd = fileSize < blockSize ? fileSize : blockSize;
            lngRangeEnd += consumed;
        }
        if (lngRangeEnd >= fileSize) {
            lngRangeEnd = fileSize;
        }
        totalConsumed = lngRangeEnd;
        char szBuf[256];
        snprintf(szBuf, sizeof(szBuf), "%ld", lngRangeEnd - consumed);
        OH_LOG_INFO(LOG_APP, "Content-Length:%{public}d", lngRangeEnd - consumed);
        if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Length", szBuf)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}s})== %{public}d",
                             szBuf,
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            return;
        }
        OH_LOG_INFO(LOG_APP, "Accept-Ranges:bytes", szBuf);
        if ((nArkWebRetCode = webResponse_SetHeaderByName("Accept-Ranges", "bytes")) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"Content-Length\", \"bytes\"})== %{public}d",
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            return;
        }
        snprintf(szBuf, sizeof(szBuf), "bytes %ld-%ld/%ld", lngRangeStart, lngRangeEnd - 1, fileSize);
        OH_LOG_INFO(LOG_APP, "Content-Range:%{public}s", szBuf);
        if ((nArkWebRetCode = webResponse_SetHeaderByName("Content-Range", szBuf)) != ARKWEB_NET_OK) {
            if (nArkWebRetCode) {
                OH_LOG_ERROR(LOG_APP,
                             "webResponse_SetHeaderByName(\"Content-Length\", szBuf{%{public}s})== %{public}d",
                             szBuf,
                             nArkWebRetCode);
            }
            response4XX(HttpCode::BAD_REQUEST);
            (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
            return;
        }
    }
    didReceiveResponse();

    unsigned char* buffer = reinterpret_cast<unsigned char*>(vecMemPage[0].m_point);
    while (true) {
        OH_ResourceManager_SeekRawFile(rawfile, consumed, 0);
        int ret = OH_ResourceManager_ReadRawFile(rawfile, buffer, blockSize);
        OH_LOG_INFO(LOG_APP, "read rawfile %{public}d bytes. file:%{public}s", ret, m_rawfilePath.c_str());
        if (ret == 0) {
            break;
        }
        if (consumed + ret > totalConsumed) {
            // 读取内容大于了要求返回的内容，只返回部分内容
            ret = totalConsumed - consumed;
        }
        consumed += ret;
        didReceiveData(buffer, ret);
        std::fill(buffer, buffer + blockSize, 0);
        if (consumed >= totalConsumed) {
            break;
        }
    }
    (static_cast<CMemPool*>(Application::g_memPool))->freePage(vecMemPage);
    OH_ResourceManager_CloseRawFile(rawfile);
    didFinish();
    return;
}

// 用户取消访问和访问结束后DidFinish会调用stop函数
void RawfileRequest::stop()
{
    OH_LOG_INFO(LOG_APP, "stop the rawfile request. m_response(%{public}p)", m_response);
    std::lock_guard<std::mutex> guard(m_mutex);
    m_stopped = true;
    if (m_response) {
        OH_ArkWeb_DestroyResponse(m_response);
        m_response = nullptr;
    }

    if (m_stream) {
        OH_ArkWebResourceRequest_DestroyHttpBodyStream(m_stream);
        m_stream = nullptr;
    }

    OH_ArkWebResourceRequest_Destroy(m_resourceRequest);
    OH_ArkWebResourceHandler_Destroy(m_resourceHandler);
    m_resourceRequest = nullptr;
    m_resourceHandler = nullptr;
}

bool RawfileRequest::webResourceRequest_GetUrl(char** url)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        OH_ArkWebResourceRequest_GetUrl(m_resourceRequest, url);
        return true;
    }
    return false;
}

bool RawfileRequest::webResourceRequest_GetMethod(char** method)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        OH_ArkWebResourceRequest_GetMethod(m_resourceRequest, method);
        return true;
    }
    return false;
}

bool RawfileRequest::webResourceRequest_GetResourceType(int& resourceType)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        resourceType = OH_ArkWebResourceRequest_GetResourceType(m_resourceRequest);
        return true;
    }
    return false;
}

bool RawfileRequest::webResourceRequest_GetRequestHeaders(ArkWeb_RequestHeaderList** headerList)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        OH_ArkWebResourceRequest_GetRequestHeaders(m_resourceRequest, headerList);
        return true;
    }
    return false;
}

int RawfileRequest::webResponse_SetHeaderByName(const std::string& strName, const std::string& strValue)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped) {
        return OH_ArkWebResponse_SetHeaderByName(m_response, strName.c_str(), strValue.c_str(), true);
    }
    return -1;
}

int RawfileRequest::webResponse_SetStatusText(const std::string& strText)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped) {
        return OH_ArkWebResponse_SetStatusText(m_response, strText.c_str());
    }
    return -1;
}

int RawfileRequest::webResponse_SetStatus(const int nStatus)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped) {
        return OH_ArkWebResponse_SetStatus(m_response, nStatus);
    }
    return -1;
}

int RawfileRequest::webResponse_SetMimeType(const std::string& strText)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped) {
        return OH_ArkWebResponse_SetMimeType(m_response, strText.c_str());
    }
    return -1;
}

int RawfileRequest::webResponse_SetCharset(const std::string& strCharset)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped) {
        return OH_ArkWebResponse_SetCharset(m_response, strCharset.c_str());
    }
    return -1;
}

void RawfileRequest::didReceiveResponse()
{
    OH_LOG_INFO(LOG_APP, "did receive response.");
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        OH_ArkWebResourceHandler_DidReceiveResponse(m_resourceHandler, m_response);
    }
}

void RawfileRequest::didReceiveData(const uint8_t* buffer, int64_t bufLen)
{
    OH_LOG_INFO(LOG_APP, "did receive data.");
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        OH_ArkWebResourceHandler_DidReceiveData(m_resourceHandler, buffer, bufLen);
    }
}

void RawfileRequest::didFinish()
{
    OH_LOG_INFO(LOG_APP, "did finish.");
    std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        OH_ArkWebResourceHandler_DidFinish(m_resourceHandler);
    }
    m_finished = true;
}

void RawfileRequest::didFailWithError(ArkWeb_NetError errorCode)
{
    OH_LOG_INFO(LOG_APP, "did finish with error %{public}d.", errorCode);
    if (!m_stopped && Application::isValidSchemeHandler(m_schemeHandler)) {
        OH_ArkWebResourceHandler_DidFailWithError(m_resourceHandler, errorCode);
    }
}

void RawfileRequest::response4XX(const int nHttpCode)
{
    if (m_response != nullptr && m_resourceRequest != nullptr &&
        Application::isValidSchemeHandler(m_schemeHandler)) { // 用户提前取消访问response会destroy掉，不能返回404
        webResponse_SetStatus(nHttpCode);
        webResponse_SetHeaderByName("Access-Control-Allow-Origin", "*");
        webResponse_SetHeaderByName("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        webResponse_SetHeaderByName("Access-Control-Allow-Headers", "X-PINGOTHER, Content-Type");
        webResponse_SetHeaderByName("Content-Length", "0");
        didReceiveResponse();
    } else {
        didFailWithError(ARKWEB_ERR_CONNECTION_FAILED); // 报错处理后，不会触发webview的onPageEnd函数，因此建议报404错误
    }
    didFinish();
}

bool RawfileRequest::isAllowUrl(const std::string& strUrl)
{
    if (strUrl.find("//") == std::string::npos) {
        return false;
    }

    std::string strPath = strUrl.substr(strUrl.find("//") + 2);
    if (strPath.find(Application::g_strTmpUrl) == 0) {
        return true;
    }

    if (!(static_cast<CordovaViewController*>(Application::g_cordovaViewController))
             ->getPluginManager()
             ->shouldAllowRequest(strUrl)) {
        OH_LOG_ERROR(LOG_APP, "unsupported domain:%{public}s", strUrl.c_str());
        return false;
    }
    return true;
}

/*
 *用于判断是否可以销毁对象
 *只有stop和finish都为true是，才可以destroy实例对象
 */
bool RawfileRequest::isTaskFinished()
{
    return (m_stopped && m_finished);
}


// 从池中取出一页内存，供执行线程使用，避免申请内存，
void* CHttpGroupRunner::HttpRunner::initThreadData()
{
    std::vector<SMemPage> vecMemBuf;
    (static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemBuf, 1);
    if (vecMemBuf.size() < 1) {
        OH_LOG_ERROR(LOG_APP, "(static_cast<CMemPool*>(Application::g_memPool))->mallocPage(vecMemBuf, 1) == nullptr");
        return nullptr;
    }
    m_nBufSize = (static_cast<CMemPool*>(Application::g_memPool))->getPageSize();
    return vecMemBuf[0].m_point;
}
// 线程执行完毕，将内存页放回内存池中，避免内存泄漏，便于其他地方复用内存
void CHttpGroupRunner::HttpRunner::deInitThreadData(void* pArg)
{
    (static_cast<CMemPool*>(Application::g_memPool))->freePage(static_cast<char*>(pArg));
}

void CHttpGroupRunner::initGroup(const int nGroupCount, const int nThreadCount)
{
    for (int i = 0; i < nGroupCount; i++) {
        HttpRunner* pHttp = new HttpRunner();
        pHttp->setPoolSize(nThreadCount);
        pHttp->start(nullptr);
        m_queGroupHttp.push(pHttp);
    }
}

bool CHttpGroupRunner::addTask(const RawfileRequest* rawfileRequest)
{
    // 添加任务之前，先处理组的调度，把没有任务的池返回组的池中
    std::lock_guard<std::mutex> guard(m_mutex);
    for (auto it = m_mapUrlToHttpRunner.begin(); it != m_mapUrlToHttpRunner.end();) {
        if (it->second->getWaitingThreads() == it->second->getCurPoolSize()) {
            m_queGroupHttp.push(it->second);
            it = m_mapUrlToHttpRunner.erase(it);
        } else {
            it++;
        }
    }

    std::string strKey = rawfileRequest->getSchemeDomainPort();
    // url是否已经有正在处理的线程池，如果有直接往已有的池中添加任务
    if (m_mapUrlToHttpRunner.find(strKey) != m_mapUrlToHttpRunner.end()) {
        HttpRunner* pHttp = m_mapUrlToHttpRunner[strKey];
        pHttp->addTask(rawfileRequest);
        OH_LOG_Print(LOG_APP,
                     LOG_INFO,
                     0xFF00,
                     "AddTask",
                     "same domain number:%{public}d thread num:%{public}d",
                     m_mapUrlToHttpRunner.size(),
                     pHttp->getCurPoolSize());
        return true;
    }

    // 没有空闲的组了，需要创建一个新组的线程池
    if (m_queGroupHttp.empty() && m_mapUrlToHttpRunner.size() < MAX_URL_BOUNDARY_RUN_TIME) {
        HttpRunner* pHttp = new HttpRunner();
        pHttp->setPoolSize(2);
        pHttp->start(nullptr);
        m_queGroupHttp.push(pHttp);
    }

    if (!m_queGroupHttp.empty()) {
        HttpRunner* pHttp = m_queGroupHttp.front();
        m_queGroupHttp.pop();
        m_mapUrlToHttpRunner[strKey] = pHttp;
        pHttp->addTask(rawfileRequest);
        OH_LOG_Print(LOG_APP,
                     LOG_INFO,
                     0xFF00,
                     "AddTask",
                     "new domain number:%{public}d thread num:%{public}d",
                     m_mapUrlToHttpRunner.size(),
                     pHttp->getCurPoolSize());
        return true;
    }

    // 请求任务添加失败，已经达到边界了，该边界是同时正在请求的域名数量为MAX_URL_BOUNDARY_RUN_TIME个
    return false;
}