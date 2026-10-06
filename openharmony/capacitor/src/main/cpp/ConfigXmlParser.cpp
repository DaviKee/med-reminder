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

#include "ConfigXmlParser.h"
#include <rawfile/raw_file.h>
#include "Application.h"

void ConfigXmlParser::parseXml()
{
    RawFile* rawFile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, "config.xml");
    if (rawFile == nullptr) {
        return;
    }
    const int const_buf_size = 1024;
    char buf[const_buf_size + 1];
    std::string strBuf;
    int nSize = 0;
    do {
        nSize = OH_ResourceManager_ReadRawFile(rawFile, buf, const_buf_size);
        buf[nSize] = 0;
        strBuf += buf;
    } while (nSize > 0);
    OH_ResourceManager_CloseRawFile(rawFile);

    m_parser = new CXMLParser();
    m_parser->parse(strBuf.c_str());
    SXMLNode* top = m_parser->getTopNode();
    int nCount = top->getNodesCount();
    for (int i = 0; i < nCount; i++) {
        SXMLNode* node = top->getNode(i);
        std::string strContent = top->getContentText(i);
        if (node->m_strName == "feature") {
            SXMLNode::SXMLAttr* attr = node->getAttr("name");
            std::string service = attr->m_strValue;
            int paramCount = node->getNodesCount();
            bool load = false;
            std::string className = "";
            for (int j = 0; j < paramCount; j++) {
                if (node->getNode(j)->getAttr("name")->m_strValue == "onload" && node->getNode(j)->getAttr("value")) {
                    if (node->getNode(j)->getAttr("value")->m_strValue == "true") {
                        load = true;
                    }
                }
                if ((node->getNode(j)->getAttr("name")->m_strValue == "android-package" ||
                     node->getNode(j)->getAttr("name")->m_strValue == "ios-package" ||
                     node->getNode(j)->getAttr("name")->m_strValue == "harmony-package") &&
                    node->getNode(j)->getAttr("value")) {
                    className = node->getNode(j)->getAttr("value")->m_strValue;
                    if (className.find_last_of(".") != std::string::npos) {
                        className = className.substr(className.find_last_of(".") + 1);
                    }
                    SXMLNode::SXMLAttr* pOnload = node->getNode(j)->getAttr("onload");
                    if (pOnload != nullptr && pOnload->m_strValue == "true") {
                        load = true;
                    }
                }
            }
            PluginEntry pluginEntry(service, className, load);
            m_vecPluginEntry.push_back(pluginEntry);
        }
        if (node->m_strName == "allow-navigation" || node->m_strName == "allow-intent") {
            if (node->getAttr("href")) {
                SXMLNode::SXMLAttr* attr = node->getAttr("href");
                std::string strHref = attr->m_strValue;
                if (!strHref.empty()) {
                    std::vector<std::string>::iterator it = find(m_vecHostName.begin(), m_vecHostName.end(), strHref);
                    if (it == m_vecHostName.end()) {
                        m_vecHostName.push_back(strHref);
                    }
                }
            }
        }

        if (node->m_strName == "chcp") {
            int paramCount = node->getNodesCount();
            for (int j = 0; j < paramCount; j++) {
                SXMLNode* pSelfNode = node->getNode(j);
                if (pSelfNode->m_strName == "auto-download") {
                    if (pSelfNode->getAttr("enabled")) {
                        if (pSelfNode->getAttr("enabled")->m_strValue == "false") {
                            m_isAllowUpdatesAutoDownload = false;
                        } else {
                            m_isAllowUpdatesAutoDownload = true;
                        }
                    }
                }

                if (pSelfNode->m_strName == "auto-install") {
                    if (pSelfNode->getAttr("enabled")) {
                        if (pSelfNode->getAttr("enabled")->m_strValue == "false") {
                            m_isAllowUpdatesAutoInstall = false;
                        } else {
                            m_isAllowUpdatesAutoInstall = true;
                        }
                    }
                }

                if (pSelfNode->m_strName == "native-interface") {
                    if (pSelfNode->getAttr("version")) {
                        m_nActiveInterfaceVersion = atoi(pSelfNode->getAttr("version")->m_strValue.c_str());
                    }
                }

                if (pSelfNode->m_strName == "config-file") {
                    if (pSelfNode->getAttr("url")) {
                        m_strChcpConfigUrl = pSelfNode->getAttr("url")->m_strValue.c_str();
                    }
                }
            }
        }
        if (node->m_strName == "preference") {
            if (node->getAttr("name")) {
                SXMLNode::SXMLAttr* attr = node->getAttr("name");
                SXMLNode::SXMLAttr* value = node->getAttr("value");
                if (attr != nullptr && value != nullptr) {
                    m_preferences->set(attr->m_strValue, value->m_strValue);
                }
                if (attr != nullptr) {
                    std::string strName = attr->m_strValue;
                    if (strName == "WECHATAPPID") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strWechatAppId = strValue;
                        }
                    }
                    if (strName == "StatusBarOverlaysWebView") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_isStatusBarOverlaysWebView = false;
                            if (strValue == "true") {
                                m_isStatusBarOverlaysWebView = true;
                            }
                        }
                    }
                    if (strName == "StatusBarBackgroundColor") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strStatusBarBackgroundColor = strValue;
                        }
                    }
                    if (strName == "StatusBarStyle") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strStatusBarStyle = strValue;
                        }
                    }
                    if (strName == "StatusBarTextColor") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strStatusTextColor = strValue;
                        }
                    }

                    if (strName == "CameraImageCompress") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            if (strValue == "true") {
                                m_isCameraImageCompress = true;
                            }
                        }
                        attr = node->getAttr("showToastText");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strCameraCompressShowToast = strValue;
                        }

                        attr = node->getAttr("maxCompressSize");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            float fValue = atof(strValue.c_str());
                            if (fValue > 0.01) {
                                m_lngCompressImageSize = fValue * 1024 * 1024;
                            }
                        }
                    }

                    if (strName == "NavigationBarBackgroundColor") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strNavigationBarBackgroundColor = strValue;
                        }
                    }

                    if (strName == "NavigationBarFontColor") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strNavigationBarFontColor = strValue;
                        }
                    }

                    if (strName == "NavigationBarFontAlign") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strNavigationBarFontAlign = strValue;
                        }
                    }

                    if (strName == "NavigationBarLight") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strNavigationBarLight = strValue;
                        }
                    }

                    if (strName == "Hostname") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strIonicHostName = strValue;
                        }
                    }

                    if (strName == "Scheme") {
                        attr = node->getAttr("value");
                        if (attr != nullptr) {
                            std::string strValue = attr->m_strValue;
                            m_strIonicScheme = strValue;
                        }
                    }
                }
            }
        }

        if (node->m_strName == "cordova-protocol-force") {
            if (node->getAttr("value")) {
                SXMLNode::SXMLAttr* attr = node->getAttr("value");
                if (attr != nullptr) {
                    std::string strValue = attr->m_strValue;
                    m_vecProtocolUrl.push_back(strValue);
                }
            }
        }

        if (node->m_strName == "cordova-cache-duration") {
            if (node->getAttr("value")) {
                SXMLNode::SXMLAttr* attr = node->getAttr("value");
                if (attr != nullptr) {
                    std::string strValue = attr->m_strValue;
                    m_lngCordovaCacheDuration = atol(strValue.c_str());
                }
            }
        }

        if (node->m_strName == "client-auth-cert") {
            std::string strUrl = "";
            if (node->getAttr("url")) {
                SXMLNode::SXMLAttr* attr = node->getAttr("url");
                if (attr != nullptr) {
                    strUrl = attr->m_strValue;
                }
            }

            std::string strP12 = "";
            if (node->getAttr("p12")) {
                SXMLNode::SXMLAttr* attr = node->getAttr("p12");
                if (attr != nullptr) {
                    strP12 = attr->m_strValue;
                }
            }

            std::string strPasswd = "";
            if (node->getAttr("password")) {
                SXMLNode::SXMLAttr* attr = node->getAttr("password");
                if (attr != nullptr) {
                    strPasswd = attr->m_strValue;
                }
            }

            if (!strUrl.empty()) {
                std::pair<std::string, std::string> p12(strP12, strPasswd);
                m_mapClientAuthCertUrl[strUrl] = p12;
            }
        }
    }

    PluginEntry coreHarmony("CoreHarmony", "CoreHarmony", true);
    m_vecPluginEntry.push_back(coreHarmony);
}

ConfigXmlParser::ConfigXmlParser()
{
    m_preferences = new CordovaPreferences();
}

ConfigXmlParser::~ConfigXmlParser()
{
    if (m_parser) {
        delete m_parser;
    }
    if (m_preferences) {
        delete m_preferences;
    }
}

CordovaPreferences* ConfigXmlParser::getCordovaPreferences()
{
    return m_preferences;
}

std::vector<PluginEntry>* ConfigXmlParser::getPluginEntry()
{
    return &m_vecPluginEntry;
}

std::vector<std::string>* ConfigXmlParser::getHostName()
{
    return &m_vecHostName;
}

std::string ConfigXmlParser::getChcpConfigUrl()
{
    return m_strChcpConfigUrl;
}

bool ConfigXmlParser::getAllAllowUpdatesAutoDownload()
{
    return m_isAllowUpdatesAutoDownload;
}

bool ConfigXmlParser::getAllowUpdatesAutoInstall()
{
    return m_isAllowUpdatesAutoInstall;
}

int ConfigXmlParser::getActiveInterfaceVersion()
{
    return m_nActiveInterfaceVersion;
}

void ConfigXmlParser::setProtocolUrl(const std::vector<std::string>& vecProtocol)
{
    for (int i = 0; i < vecProtocol.size(); i++) {
        m_vecProtocolUrl.push_back(vecProtocol[i]);
    }
}

std::vector<std::string>* ConfigXmlParser::getProtocolUrl()
{
    return &m_vecProtocolUrl;
}

void ConfigXmlParser::getCameraImageCompress(bool& isCameraImageCompress,
                                             long& lngCompressImageSize,
                                             std::string& strCameraCompressShowToast)
{
    isCameraImageCompress = m_isCameraImageCompress;
    lngCompressImageSize = m_lngCompressImageSize;
    strCameraCompressShowToast = m_strCameraCompressShowToast;
}

void ConfigXmlParser::setCordovaCacheDuration(const long lngCordovaCacheDuration)
{
    m_lngCordovaCacheDuration = lngCordovaCacheDuration;
}

long ConfigXmlParser::getCordovaCacheDuration()
{
    return m_lngCordovaCacheDuration;
}

std::string ConfigXmlParser::getIonicHostName()
{
    return m_strIonicHostName;
}

std::string ConfigXmlParser::getIonicScheme()
{
    return m_strIonicScheme;
}

void ConfigXmlParser::setClientCert(const std::string& strUrl)
{
    std::pair<std::string, std::string> p12("", "");
    m_mapClientAuthCertUrl[strUrl] = p12;
}

bool ConfigXmlParser::getClientCert(const std::string& strUrl, std::string& strP12, std::string& strPassword)
{
    if (m_mapClientAuthCertUrl.find(strUrl) != m_mapClientAuthCertUrl.end()) {
        std::pair<std::string, std::string> p12 = m_mapClientAuthCertUrl[strUrl];
        strP12 = p12.first;
        strPassword = p12.second;
        return true;
    }
    return false;
}