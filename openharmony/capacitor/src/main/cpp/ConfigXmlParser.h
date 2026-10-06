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

#ifndef CONFIG_XML_PARSER_H
#define CONFIG_XML_PARSER_H
#include "PluginEntry.h"
#include "XMLParser.h"

/**
 * @brief parse config.xml
 */
class ConfigXmlParser {
    CordovaPreferences* m_preferences{nullptr};
    CXMLParser* m_parser;
    std::vector<std::string> m_vecHostName;
    std::vector<PluginEntry> m_vecPluginEntry;
    std::string m_strChcpConfigUrl;
    bool m_isAllowUpdatesAutoDownload{false};
    bool m_isAllowUpdatesAutoInstall{false};
    int m_nActiveInterfaceVersion{0};
    std::string m_strWechatAppId;
    std::vector<std::string> m_vecProtocolUrl;
    bool m_isStatusBarOverlaysWebView{false};
    std::string m_strStatusBarBackgroundColor;
    std::string m_strStatusTextColor;
    std::string m_strStatusBarStyle;
    bool m_isCameraImageCompress{false};
    long m_lngCompressImageSize{4 * 1024 * 1024};
    std::string m_strCameraCompressShowToast;
    std::string m_strNavigationBarBackgroundColor;
    std::string m_strNavigationBarFontColor;
    std::string m_strNavigationBarFontAlign;
    std::string m_strNavigationBarLight;
    long m_lngCordovaCacheDuration{24 * 60 * 60};
    std::string m_strIonicHostName;
    std::string m_strIonicScheme;
    std::string m_strMinTlsProtocol{"TLSv12"};
    std::map<std::string, std::pair<std::string, std::string> > m_mapClientAuthCertUrl;

public:
    ConfigXmlParser();
    ~ConfigXmlParser();
    void parseXml();
    std::vector<PluginEntry>* getPluginEntry();
    /**
     * @brief Gets Preferences of config.xml
     * @return Preferences pointer
     */
    CordovaPreferences* getCordovaPreferences();
    /**
     * @brief All functions below are used to retrieve corresponding values. In actual plugin development,
     *      you can directly use the Preferences pointer without calling these functions.
     */
    std::vector<std::string>* getHostName();
    void setProtocolUrl(const std::vector<std::string>& vecProtocol);
    std::vector<std::string>* getProtocolUrl();
    std::string getChcpConfigUrl();
    bool getAllAllowUpdatesAutoDownload();
    bool getAllowUpdatesAutoInstall();
    int getActiveInterfaceVersion();
    void getCameraImageCompress(bool& isCameraImageCompress,
                                long& lngCompressImageSize,
                                std::string& strCameraCompressShowToast);
    long getCordovaCacheDuration();
    void setCordovaCacheDuration(const long lngCordovaCacheDuration);
    std::string getIonicHostName();
    std::string getIonicScheme();
    void setClientCert(const std::string& strUrl);
    bool getClientCert(const std::string& strUrl, std::string& strP12, std::string& strPassword);
};
#endif