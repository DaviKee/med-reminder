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

#include "SystemCookieManager.h"
#include "Application.h"
#include "HttpUrl.h"
#include <hilog/log.h>

#undef LOG_TAG
#define LOG_TAG "ss-handler"

std::map<std::string, std::string> SystemCookieManager::parseCookie(const std::string& strValue)
{
    std::vector<std::string> vecCookies;
    HttpUrl::splitString(strValue, ";", vecCookies);
    std::map<std::string, std::string> mapKeyToValue;
    for (int i = 0; i < vecCookies.size(); i++) {
        std::vector<std::string> vecCookie;
        HttpUrl::splitString(vecCookies[i], "=", vecCookie);
        if (vecCookie.size() == 2) {
            std::string strKey = HttpUrl::trim(vecCookie[0]);
            std::string strValue = HttpUrl::trim(vecCookie[1]);
            mapKeyToValue[strKey] = strValue;
        }
    }
    return mapKeyToValue;
}

void SystemCookieManager::resetCookie(const std::string& strUrl, std::string& strCookie, std::string& strCookieUrl)
{
    std::string strDomain = "domain";
    strCookieUrl = strUrl;
    if (strcasestr(strCookie.c_str(), strDomain.c_str()) != nullptr) {
        char* pStartPos = strcasestr(strCookie.c_str(), strDomain.c_str());
        int nStartPos = pStartPos - strCookie.c_str();
        int nEndPos = nStartPos;
        while (pStartPos[0] != 0) {
            if (pStartPos[0] == ';') {
                nEndPos++;
                break;
            }
            nEndPos++;
            pStartPos++;
        }
        std::string strCookieDomain = strCookie.substr(nStartPos, nEndPos - nStartPos);
        strCookie = strCookie.replace(nStartPos, nEndPos - nStartPos, "");
        strCookie = strCookie.erase(0, strCookie.find_first_not_of(" "));
        strCookie = strCookie.erase(strCookie.find_last_not_of(" ") + 1);
        if (strCookie.find(";") == strCookie.length() - 1) {
            strCookie = strCookie.substr(0, strCookie.length() - 1);
        }
        if (strCookieDomain.find(";") != std::string::npos) {
            strCookieDomain = strCookieDomain.substr(0, strCookieDomain.find(";"));
        }
        if (strCookieDomain.find("=") != std::string::npos) {
            if (strUrl.find("https") == 0) {
                strCookieUrl = "https://";
            } else {
                strCookieUrl = "http://";
            }
            strCookieUrl += strCookieDomain.substr(strCookieDomain.find("=") + 1);
        }
    }
}

void SystemCookieManager::setCookie(const std::string& strHttpUrl, const std::vector<std::string>& vecCordovaCookie)
{
    for (int i = 0; i < vecCordovaCookie.size(); i++) {
        if (Application::g_cookieManage != nullptr &&
            !ARKWEB_MEMBER_MISSING(Application::g_cookieManage, configCookieSync)) {
            std::string strCookie = vecCordovaCookie[i].c_str();
            std::string strSetCookie = "Set-Cookie:";
            if (strcasestr(strCookie.c_str(), strSetCookie.c_str()) != nullptr) {
                strCookie = strCookie.substr(strSetCookie.length());
            }

            strCookie = strCookie.substr(strCookie.find_first_not_of(" "));
            if (strHttpUrl.find("https") == 0) {
                std::string strSameSite = "SameSite";
                if (strcasestr(strCookie.c_str(), strSameSite.c_str()) != nullptr) {
                    int startPos = strcasestr(strCookie.c_str(), strSameSite.c_str()) - strCookie.c_str();
                    int endPos = strCookie.find(";", startPos);
                    if (endPos != std::string::npos) {
                        if (strcasestr(strCookie.c_str(), "secure") != nullptr) {
                            strCookie = strCookie.replace(startPos, endPos - startPos, "SameSite=None");
                        } else {
                            strCookie = strCookie.replace(startPos, endPos - startPos + 1, "SameSite=None; Secure");
                        }
                    } else {
                        strCookie = strCookie.replace(startPos, strCookie.length() - startPos, "SameSite=None; Secure");
                    }
                } else {
                    strCookie += "; SameSite=None; Secure";
                }
            }
            std::string strCookDomain = "";
            resetCookie(strHttpUrl, strCookie, strCookDomain);
            int errorCode =
                Application::g_cookieManage->configCookieSync(strCookDomain.c_str(),
                                                              strCookie.c_str(),
                                                              false,
                                                              vecCordovaCookie[i].find("HttpOnly") > 0 ? true : false);
            if (errorCode == ARKWEB_SUCCESS) {
                OH_LOG_INFO(LOG_APP,
                            "Cookie set Success domain:%{public}s cookie:%{public}s",
                            strCookDomain.c_str(),
                            strCookie.c_str());
            } else {
                OH_LOG_ERROR(LOG_APP,
                             "Cookie set Error domain:%{public}s ErrorCode:%{public}d, cookie:%{public}s",
                             strCookDomain.c_str(),
                             errorCode,
                             strCookie.c_str());
            }
        } else {
            OH_LOG_ERROR(LOG_APP,
                         "Application::g_cookieManage == nullptr || ARKWEB_MEMBER_MISSING(Application::g_cookieManage, "
                         "configCookieSync)");
        }
    }
}

std::string SystemCookieManager::getCookie(const std::string& strHttpUrl, const std::string& strValue)
{
    std::map<std::string, std::string> mapKeyValue = parseCookie(strValue);
    std::map<std::string, std::string> mapCookies;
    if (Application::g_cookieManage != nullptr && !ARKWEB_MEMBER_MISSING(Application::g_cookieManage, existCookies)) {
        if (Application::g_cookieManage->existCookies(false)) {
            if (!ARKWEB_MEMBER_MISSING(Application::g_cookieManage, fetchCookieSync)) {
                char* pCookieValue = nullptr;
                Application::g_cookieManage->fetchCookieSync(strHttpUrl.c_str(), false, true, &pCookieValue);
                if (pCookieValue != nullptr && strlen(pCookieValue) > 0) {
                    std::string strCookies = pCookieValue;
                    std::string strCookie = "Set-Cookie:";
                    size_t pos;
                    while ((pos = strCookies.find(strCookie)) != std::string::npos) {
                        strCookies.erase(pos, strCookie.length());
                    }
                    mapCookies = parseCookie(strCookies);
                }
                if (pCookieValue != nullptr) {
                    OH_ArkWeb_ReleaseString(pCookieValue);
                }
            }
        }
    }

    for (auto it = mapCookies.begin(); it != mapCookies.end(); it++) {
        if (mapKeyValue.find(it->first) == mapKeyValue.end()) {
            mapKeyValue[it->first] = it->second;
        }
    }

    std::string strRet;
    for (auto it = mapKeyValue.begin(); it != mapKeyValue.end(); it++) {
        if (!strRet.empty()) {
            strRet += "; ";
        }
        strRet += (it->first + "=" + it->second);
    }
    return strRet;
}