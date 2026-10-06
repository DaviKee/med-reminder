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

#include "HttpUrl.h"
#include "openssl/md5.h"
#include <hilog/log.h>
#include <web/arkweb_scheme_handler.h>
std::map<std::string, std::string> HttpUrl::s_mapMimeType = {{"html", "text/html"},
                                                             {"htm", "text/html"},
                                                             {"shtml", "text/html"},
                                                             {"css", "text/css"},
                                                             {"xml", "text/xml"},
                                                             {"json", "application/json"},
                                                             {"csv", "application/csv"},
                                                             {"gif", "image/gif"},
                                                             {"jpeg", "image/jpeg"},
                                                             {"jpg", "image/jpeg"},
                                                             {"js", "application/x-javascript"},
                                                             {"atom", "application/atom+xml"},
                                                             {"rss", "application/rss+xml"},
                                                             {"mml", "text/mathml"},
                                                             {"txt", "text/plain"},
                                                             {"jad", "text/vnd.sun.j2me.app-descriptor"},
                                                             {"wml", "text/vnd.wap.wml"},
                                                             {"htc", "text/x-component"},
                                                             {"png", "image/png"},
                                                             {"tif", "image/tiff"},
                                                             {"tiff", "image/tiff"},
                                                             {"wbmp", "image/vnd.wap.wbmp"},
                                                             {"ico", "image/x-icon"},
                                                             {"jng", "image/x-jng"},
                                                             {"bmp", "image/x-ms-bmp"},
                                                             {"svg", "image/svg+xml"},
                                                             {"svgz", "image/svg+xml"},
                                                             {"webp", "image/webp"},
                                                             {"jar", "application/java-archive"},
                                                             {"war", "application/java-archive"},
                                                             {"ear", "application/java-archive"},
                                                             {"hqx", "application/mac-binhex40"},
                                                             {"doc ", "application/msword"},
                                                             {"pdf", "application/pdf"},
                                                             {"ps", "application/postscript"},
                                                             {"eps", "application/postscript"},
                                                             {"ai", "application/postscript"},
                                                             {"rtf", "application/rtf"},
                                                             {"xls", "application/vnd.ms-excel"},
                                                             {"ppt", "application/vnd.ms-powerpoint"},
                                                             {"wmlc", "application/vnd.wap.wmlc"},
                                                             {"kml", "application/vnd.google-earth.kml+xml"},
                                                             {"kmz", "application/vnd.google-earth.kmz"},
                                                             {"7z", "application/x-7z-compressed"},
                                                             {"cco", "application/x-cocoa"},
                                                             {"jardiff", "application/x-java-archive-diff"},
                                                             {"jnlp", "application/x-java-jnlp-file"},
                                                             {"run", "application/x-makeself"},
                                                             {"pl", "application/x-perl"},
                                                             {"pm", "application/x-perl"},
                                                             {"prc", "application/x-pilot"},
                                                             {"pdb", "application/x-pilot"},
                                                             {"rar", "application/x-rar-compressed"},
                                                             {"rpm", "application/x-redhat-package-manager"},
                                                             {"sea", "application/x-sea"},
                                                             {"swf", "application/x-shockwave-flash"},
                                                             {"sit", "application/x-stuffit"},
                                                             {"tcl", "application/x-tcl"},
                                                             {"tk", "application/x-tcl"},
                                                             {"der", "application/x-x509-ca-cert"},
                                                             {"pem", "application/x-x509-ca-cert"},
                                                             {"crt", "application/x-x509-ca-cert"},
                                                             {"xpi", "application/x-xpinstall"},
                                                             {"xhtml", "application/xhtml+xml"},
                                                             {"zip", "application/zip"},
                                                             {"wgz", "application/x-nokia-widget"},
                                                             {"bin", "application/octet-stream"},
                                                             {"exe", "application/octet-stream"},
                                                             {"dll", "application/octet-stream"},
                                                             {"deb", "application/octet-stream"},
                                                             {"dmg", "application/octet-stream"},
                                                             {"eot", "application/octet-stream"},
                                                             {"iso", "application/octet-stream"},
                                                             {"img", "application/octet-stream"},
                                                             {"msi", "application/octet-stream"},
                                                             {"msp", "application/octet-stream"},
                                                             {"msm", "application/octet-stream"},
                                                             {"mid", "audio/midi"},
                                                             {"midi", "audio/midi"},
                                                             {"kar", "audio/midi"},
                                                             {"mp3", "audio/mpeg"},
                                                             {"opus", "audio/opus"},
                                                             {"wav", "audio/wav"},
                                                             {"ogg", "audio/ogg"},
                                                             {"oga", "audio/ogg"},
                                                             {"m4a", "audio/x-m4a"},
                                                             {"weba", "audio/webm"},
                                                             {"aac", "audio/aac"},
                                                             {"flac", "audio/flac"},
                                                             {"ra", "audio/x-realaudio"},
                                                             {"3gpp", "video/3gpp"},
                                                             {"3gp", "video/3gpp"},
                                                             {"mp4", "video/mp4"},
                                                             {"ogv", "video/ogg"},
                                                             {"mpeg", "video/mpeg"},
                                                             {"mpg", "video/mpeg"},
                                                             {"mov", "video/quicktime"},
                                                             {"mkv", "video/x-matroska"},
                                                             {"webm", "video/webm"},
                                                             {"flv", "video/x-flv"},
                                                             {"m4v", "video/x-m4v"},
                                                             {"mng", "video/x-mng"},
                                                             {"asx", "video/x-ms-asf"},
                                                             {"asf", "video/x-ms-asf"},
                                                             {"wmv", "video/x-ms-wmv"},
                                                             {"avi", "video/x-msvideo"},
                                                             {"ts", "video/MP2T"},
                                                             {"m3u8", "application/x-mpegURL"},
                                                             {"apk", "application/vnd.android.package-archive"},
                                                             {"woff", "font/woff"},
                                                             {"woff2", "font/woff2"},
                                                             {"ttf", "font/ttf"},
                                                             {"eot", "font/eot"},
                                                             {"otf", "font/otf"}};

std::string HttpUrl::getMimeType(const std::string& strFilePath)
{
    std::string strLower = toLower(strFilePath);
    if (strLower.find_last_of(".") != std::string::npos) {
        std::string strEnd = strLower.substr(strLower.find_last_of(".") + 1);
        if (strEnd.find("?") != std::string::npos) {
            strEnd = strEnd.substr(0, strEnd.find("?"));
        }
        if (strEnd.find("#") != std::string::npos) {
            strEnd = strEnd.substr(0, strEnd.find("#"));
        }
        if (s_mapMimeType.find(strEnd) != s_mapMimeType.end()) {
            return s_mapMimeType[strEnd];
        } else {
            OH_LOG_Print(
                LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "HttpUrl", "Unsupported mime %{public}s", strEnd.c_str());
        }
    }
    return "application/octet-stream";
}

std::string HttpUrl::getMimeType(const int resourceType, const std::string& strUrlPath)
{
    if (resourceType == MAIN_FRAME) {
        std::string strMime = getMimeType(strUrlPath);
        if (strMime == "application/octet-stream") {
            strMime = "text/html";
        }
        return strMime;
    } else if (resourceType == SUB_FRAME) {
        std::string strMime = getMimeType(strUrlPath);
        if (strMime == "application/octet-stream") {
            strMime = "text/html";
        }
        return strMime;
    } else if (resourceType == STYLE_SHEET) {
        return "text/css";
    } else if (resourceType == SCRIPT) {
        return "text/javascript";
    } else if (resourceType == IMAGE) {
        std::string strMime = getMimeType(strUrlPath);
        if (strMime == "application/octet-stream") {
            strMime = "image/jpeg";
        }
        return strMime;
    } else if (resourceType == FONT_RESOURCE) {
        std::string strMime = getMimeType(strUrlPath);
        if (strMime == "application/octet-stream") {
            strMime = "font/ttf";
        }
        return strMime;
    } else if (resourceType == FAVICON) {
        return "image/x-icon";
    } else if (resourceType == MEDIA) {
        std::string strMime = getMimeType(strUrlPath);
        if (strMime == "application/octet-stream") {
            strMime = "video/mp4";
        }
        return strMime;
    }

    return "application/octet-stream";
}

std::string HttpUrl::trim(const std::string& strValue)
{
    std::string strRet = strValue;
    strRet.erase(0, strRet.find_first_not_of(" "));
    strRet.erase(strRet.find_last_not_of(" ") + 1);
    strRet.erase(0, strRet.find_first_not_of("\n"));
    strRet.erase(strRet.find_last_not_of("\n") + 1);
    strRet.erase(0, strRet.find_first_not_of("\r"));
    strRet.erase(strRet.find_last_not_of("\r") + 1);
    return strRet;
}

bool HttpUrl::endsWithEqual(const std::string& str, const std::string& suffix)
{
    if (str.length() < suffix.length()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

void HttpUrl::splitString(const std::string& strString, const std::string& split, std::vector<std::string>& vecString)
{
    int pos = -1;
    std::string strSrcTmp = strString;
    if (strString.empty()) {
        return;
    }
    std::string strTmp;
    while (true) {
        pos = strSrcTmp.find(split);
        if (pos < 0) {
            break;
        }
        strTmp = strSrcTmp.substr(0, pos);
        vecString.push_back(strTmp);
        strSrcTmp = strSrcTmp.substr(pos + 1);
    }
    vecString.push_back(strSrcTmp);
    return;
}

std::string HttpUrl::generateMD5(const std::string& strIn)
{
    MD5_CTX context;
    MD5_Init(&context);                                  // 初始化 MD5 上下文
    MD5_Update(&context, strIn.c_str(), strIn.length()); // 添加输入数据
    unsigned char md5Hash[MD5_DIGEST_LENGTH];
    char md5Value[2 * MD5_DIGEST_LENGTH + 1];
    MD5_Final(md5Hash, &context);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(md5Value + i + i, "%02x", md5Hash[i]); // 以十六进制格式打印每个字节
    }
    return md5Value;
}

std::string HttpUrl::toLower(const std::string& str)
{
    std::string strRet;
    for (int i = 0; i < str.length(); i++) {
        char c = tolower(str[i]);
        strRet += c;
    }
    return strRet;
}
