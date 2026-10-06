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
#include "rawfile_request.h"
#include "CordovaViewController.h"
#include "hilog/log.h"
#include "Application.h"
#include <bundle/native_interface_bundle.h>
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "openssl/err.h"
#include "openssl/bn.h"
#include "openssl/asn1.h"
#include "openssl/x509v3.h"
#include "openssl/x509_vfy.h"
#include "FileSystem.h"

const unsigned int CordovaViewController::LOG_PRINT_DOMAIN = 0xFF00;

CordovaViewController::CordovaViewController()
{
    m_queue = nullptr;
    m_pluginManager = nullptr;
    m_bridge = nullptr;
    m_xmlParser = nullptr;
    m_capacitorPluginManager = NULL;
    m_capacitorMessageQueue = NULL;
}

CordovaViewController::~CordovaViewController() {}

void CordovaViewController::loadConfig()
{
    m_xmlParser->parseXml();
}

void CordovaViewController::init(const std::string& strWebTag)
{
    if (m_xmlParser == nullptr) {
        m_xmlParser = new ConfigXmlParser();
        loadConfig();
        m_queue = new NativeToJsMessageQueue();
        m_pluginManager = new PluginManager(m_queue);
        m_pluginManager->init(m_xmlParser->getPluginEntry());
        m_bridge = new CordovaBridge(m_pluginManager, m_queue);

        CFileSystem::initFileSystem();
        CordovaPlugin::initThreadSafeFunction();
        Application::g_memPool = new CMemPool();
        ((CMemPool*)Application::g_memPool)->init(1, 1024 * 1024 * 2); // 默认初始化1页内存，大小为2M
        Application::g_httpGroupRunner = new CHttpGroupRunner();
        ((CHttpGroupRunner*)Application::g_httpGroupRunner)->initGroup(2, 5); // 默认初始化2组，每组5个线程
        // m_checkCertificate.Start(NULL);

        Plugin::initThreadSafeFunction();
        m_capacitorPluginManager = new Capacitor::PluginManager();
        m_capacitorMessageQueue = new MessageQueue();
    }

    if (m_mapWebTagJsApi.find(strWebTag) == m_mapWebTagJsApi.end()) {
        CordovaExposedJsApi* jsAPI = new CordovaExposedJsApi(m_bridge, strWebTag);
        CordovaWebViewEngine* engine = new CordovaWebViewEngine(jsAPI, strWebTag);
        m_mapWebTagJsApi[strWebTag] = jsAPI;
        m_mapWebTagEngine[strWebTag] = engine;
        SetComponentCallback(engine, strWebTag);
    }

    if (m_mapCapacitorBridge.find(strWebTag) == m_mapCapacitorBridge.end()) {
        BridgeController* pBridgeController =
            new BridgeController(strWebTag, m_capacitorPluginManager, m_capacitorMessageQueue);
        m_mapCapacitorBridge[strWebTag] = pBridgeController;
    }
}

CordovaPreferences* CordovaViewController::getCordovaPreferences()
{
    return m_xmlParser->getCordovaPreferences();
}

void CordovaViewController::loadUrl(const std::string& strWebTag)
{
    init(strWebTag);
}

void CordovaViewController::SetComponentCallback(CordovaWebViewEngine* engine, const std::string& strWebTag)
{
    if (!ARKWEB_MEMBER_MISSING(Application::g_component, onControllerAttached)) {
        Application::g_component->onControllerAttached(
            strWebTag.c_str(), onWebControllerAttached, reinterpret_cast<void*>(engine));
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ArkWeb", "m_component onControllerAttached func not exist");
    }

    if (!ARKWEB_MEMBER_MISSING(Application::g_component, onPageBegin)) {
        Application::g_component->onPageBegin(strWebTag.c_str(), onWebPageBegin, reinterpret_cast<void*>(engine));
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ArkWeb", "m_component onPageBegin func not exist");
    }

    if (!ARKWEB_MEMBER_MISSING(Application::g_component, onPageEnd)) {
        Application::g_component->onPageEnd(strWebTag.c_str(), onWebPageEnd, reinterpret_cast<void*>(engine));
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ArkWeb", "m_component onPageEnd func not exist");
    }

    if (!ARKWEB_MEMBER_MISSING(Application::g_component, onDestroy)) {
        Application::g_component->onDestroy(strWebTag.c_str(), onWebDestroy, reinterpret_cast<void*>(engine));
    } else {
        OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "ArkWeb", "m_component onDestroy func not exist");
    }
}

void CordovaViewController::onWebControllerAttached(const char* webTag, void* userData)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk ValidCallback webTag: %{public}s", webTag);
    if (!userData) {
        OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk ValidCallback userData is nullptr");
        return;
    }
    CordovaWebViewEngine* queue = (CordovaWebViewEngine*)userData;
    queue->exposeJsInterface();

    Bridge* pBridge = ((CordovaViewController*)Application::g_cordovaViewController)->getCapacitorBridge(webTag);
    pBridge->getMessageHandler()->exposeJsInterface();

    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "ndk RegisterJavaScriptProxy end");
}

void CordovaViewController::onWebPageBegin(const char* webTag, void* userData)
{
    ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->onPageStart(webTag);
    ((CordovaViewController*)Application::g_cordovaViewController)->getCapacitorPluginManager()->onPageStart(webTag);
    return;
}

void CordovaViewController::onWebPageEnd(const char* webTag, void* userData)
{
    Application::clearResource(webTag);
    ((CordovaViewController*)Application::g_cordovaViewController)->getPluginManager()->onPageEnd(webTag);
    ((CordovaViewController*)Application::g_cordovaViewController)->getCapacitorPluginManager()->onEnd(webTag);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "web{%{public}s} onWebPageEnd", webTag);
    return;
}

CordovaWebViewEngine* CordovaViewController::getCordovaWebViewEngine(const std::string& strWebTag)
{
    if (m_mapWebTagEngine.find(strWebTag) != m_mapWebTagEngine.end()) {
        return m_mapWebTagEngine[strWebTag];
    }
    return nullptr;
}

void CordovaViewController::onWebDestroy(const char* webTag, void* userData)
{

    Application::removeSchemeHandlerWebTag(webTag);
    Application::delFunctionRefresh(webTag);
    (reinterpret_cast<CordovaViewController*>(Application::g_cordovaViewController))
        ->getPluginManager()
        ->onDestroy(webTag);
    (reinterpret_cast<CordovaViewController*>(Application::g_cordovaViewController))
        ->getCapacitorPluginManager()
        ->onDestroy(webTag);
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "ArkWeb", "web{%{public}s} onWebDestroy", webTag);
    return;
}

void CordovaViewController::execPluginResult(const std::string& strWebTag,
                                             const std::string& pluginName,
                                             const std::string& strAction,
                                             const std::string& strArgs)
{
    if (m_pluginManager) {
        m_pluginManager->exec(strWebTag, pluginName, strAction, strArgs);
    }

    if (m_mapCapacitorBridge.find(strWebTag) != m_mapCapacitorBridge.end()) {
        m_mapCapacitorBridge[strWebTag]->getBridge()->callPluginMethod(pluginName, strAction, strArgs);
    }
}

void CordovaViewController::addJavaScript(const std::string& strWebTag, const std::string& statement)
{
    if (m_queue) {
        m_queue->addJavaScript(strWebTag, statement);
        m_queue->setPaused(false);
    }
}

std::vector<std::string>* CordovaViewController::getHostName()
{
    CordovaPlugin* pPlugin = m_pluginManager->getPlugin("Whitelist");
    if (pPlugin == nullptr) {
        return nullptr;
    }
    return pPlugin->getHostName();
}

std::vector<std::string>* CordovaViewController::getConfigHostName()
{
    return m_xmlParser->getHostName();
}

static int verify_cb(int ok, X509_STORE_CTX* ctx)
{
    return ok;
}

PluginManager* CordovaViewController::getPluginManager()
{
    return m_pluginManager;
}

void CordovaViewController::CheckCertificates::execute(void*)
{
    OH_NativeBundle_ApplicationInfo application = OH_NativeBundle_GetCurrentApplicationInfo();
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xff00, "ArkWeb", "bundleName:%{public}s", application.bundleName);
    m_strBundleName = application.bundleName;

    char* appId = OH_NativeBundle_GetAppId();
    std::string strAppId = appId;
    if (strAppId.find_first_of("_") != std::string::npos) {
        strAppId = strAppId.substr(0, strAppId.find_first_of("_"));
    }
    m_strAppId = strAppId;
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xff00, "ArkWeb", "appId:%{public}s", strAppId.c_str());

    char* appIdf = OH_NativeBundle_GetAppIdentifier();
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xff00, "ArkWeb", "appIdentifier:%{public}s", appIdf);
    std::string strAppIdentifier = appIdf;

    if (m_strBundleName != "com.example.myapplication" && m_strAppId != "com.example.myapplication" &&
        !strAppIdentifier.empty()) {
        RawFile* rawfile = OH_ResourceManager_OpenRawFile(Application::g_resourceManager, "cordova.crt");
        if (rawfile == NULL) {
            OH_LOG_Print(
                LOG_APP,
                LOG_ERROR,
                0xff00,
                "ArkWeb",
                "No certificate found，Please apply for the cordova.cer certificate from the author(magongshou888(v))");
            exit(0);
        }

        X509_STORE* store = X509_STORE_new();
        std::string strCa =
            "-----BEGIN CERTIFICATE-----\r\n"
            "MIIEBzCCAu+gAwIBAgIUHxtwpbxYpd/9YSyJwMo1CbW4smswDQYJKoZIhvcNAQEL\r\n"
            "BQAwgZExCzAJBgNVBAYTAkNOMREwDwYDVQQIDAhaaGVKaWFuZzERMA8GA1UEBwwI\r\n"
            "SGFuZ1pob3UxDjAMBgNVBAoMBXRvbmdlMQ4wDAYDVQQLDAV0b25nZTEXMBUGA1UE\r\n"
            "AwwObWFnb25nc2hvdS5jb20xIzAhBgkqhkiG9w0BCQEWFGNoZW5saWh1aWFiY0Ax\r\n"
            "NjMuY29tMCAXDTI0MDkyNDAzMDU0M1oYDzIxMjQwODMxMDMwNTQzWjCBkTELMAkG\r\n"
            "A1UEBhMCQ04xETAPBgNVBAgMCFpoZUppYW5nMREwDwYDVQQHDAhIYW5nWmhvdTEO\r\n"
            "MAwGA1UECgwFdG9uZ2UxDjAMBgNVBAsMBXRvbmdlMRcwFQYDVQQDDA5tYWdvbmdz\r\n"
            "aG91LmNvbTEjMCEGCSqGSIb3DQEJARYUY2hlbmxpaHVpYWJjQDE2My5jb20wggEi\r\n"
            "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCtBxHdTxWVmN9iLWDwJzRrNRef\r\n"
            "YXdbONRmyA2RCqgHkfSVjc8TvH3NRkw+VH0PqfeblAkOaRFbkhSjQzffyMQciQB7\r\n"
            "p0+NVmnW4OLNHbZJSWLvwBdB4oTvxQzIg0vvT7ZA0UavJ9V5F7dfJAh5IsC8AUJM\r\n"
            "v3rJMwZRBFfUREcWEiEbuuOPfLndm/PEIR4zgAhFuYzyL06SPU5WvRAlu0JbGcye\r\n"
            "4ppcs6vinmYcRJ15kzkA76Jjv0hJsklKx07f373XY2kUiyHi7izG55nnADRjEy57\r\n"
            "2BJSWFiHJbKbENhBTXKZYQx54cI8m/NMPT9BKYmj3fm5qsr+jA8t9SF2750fAgMB\r\n"
            "AAGjUzBRMB0GA1UdDgQWBBTynquaFyghMYbJ/SQyTe0GIW81JjAfBgNVHSMEGDAW\r\n"
            "gBTynquaFyghMYbJ/SQyTe0GIW81JjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3\r\n"
            "DQEBCwUAA4IBAQByqGDSAV9pt+YxlEiXfUdTXzUSrv2tz/UbLGPRNuDHtmiuaVAu\r\n"
            "v2XUJP33hvRuW12Wox9qNFuMHqahXawL/enX8R0vVJPXYaA7v/Nn8OXk1ei68HM7\r\n"
            "fxyBjF0Xb1ZKSQZ9n+ZWFIcWMfngESOOmL0UDqIj7cwRX7N/cbpvPmI2ifmgCBZw\r\n"
            "5K6gSZV/UGY+pLGe2CBFNckhxO56dshbomYmyGchP/8BbdlsP98JfBfPA60Oz5nc\r\n"
            "ksVK2WOV5rB1T05bMfl7dRb6mCNN8pXxg3sa7ApeMOg3ocvK7lKwtDCqbljeG+Ao\r\n"
            "gjmNEYkSnyY3fNJACNljKqQJRbiW5S6+ruJI\r\n"
            "-----END CERTIFICATE-----\r\n";

        BIO* caBio = BIO_new(BIO_s_mem());
        BIO_write(caBio, strCa.c_str(), strCa.length());
        X509* ca = PEM_read_bio_X509(caBio, NULL, NULL, NULL);
        X509_STORE_add_cert(store, ca);
        X509_STORE_set_verify_cb(store, verify_cb);

        BIO* cordovaBio = BIO_new(BIO_s_mem());
        const int blockSize = 1024;
        char buffer[blockSize];
        const long lngSize = OH_ResourceManager_GetRawFileSize(rawfile);
        long consumed = 0;
        while (true) {
            int ret = OH_ResourceManager_ReadRawFile(rawfile, buffer, blockSize);
            if (ret == 0 || consumed == lngSize) {
                break;
            }
            consumed += ret;
            OH_ResourceManager_SeekRawFile(rawfile, consumed, 0);
            BIO_write(cordovaBio, buffer, ret);
        }
        OH_ResourceManager_CloseRawFile(rawfile);
        X509* cordova = PEM_read_bio_X509(cordovaBio, NULL, NULL, NULL);

        if (!checkCert(cordova)) {
            exit(0);
        }

        X509_STORE_CTX* ctx = X509_STORE_CTX_new();
        X509_STORE_CTX_init(ctx, store, cordova, NULL);

        int result = X509_verify_cert(ctx);
        if (result != 1) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, 0xff00, "ArkWeb", "verify cert failed");
            exit(0);
        }

        X509_STORE_free(store);
        BIO_free(caBio);
        BIO_free(cordovaBio);
        X509_free(ca);
        X509_free(cordova);
        X509_STORE_CTX_free(ctx);
        return;
    }
    return;
}

bool CordovaViewController::CheckCertificates::checkCert(void* pX509)
{
    X509* cert = (X509*)pX509;
    long lngVersion = X509_get_version(cert);
    if (lngVersion != X509_VERSION_3) {
        return false;
    }
    char szX509Buf[1024];
    X509_NAME* pName = X509_get_subject_name(cert);
    std::fill(szX509Buf, szX509Buf + sizeof(szX509Buf), 0);
    X509_NAME_get_text_by_NID(pName, NID_commonName, szX509Buf, sizeof(szX509Buf));
    std::string strCn = szX509Buf;
    if (strCn != m_strBundleName && strCn != m_strAppId) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, 0xff00, "ArkWeb", "Certificate and BundleName are inconsistent");
        return false;
    }

    X509_NAME* pIssuer = X509_get_issuer_name(cert);
    std::fill(szX509Buf, szX509Buf + sizeof(szX509Buf), 0);
    X509_NAME_get_text_by_NID(pIssuer, NID_commonName, szX509Buf, sizeof(szX509Buf));
    std::string strIssuer = szX509Buf;
    if (strIssuer != "magongshou.com") {
        OH_LOG_Print(LOG_APP, LOG_ERROR, 0xff00, "ArkWeb", "The certificate issuer is incorrect");
        return false;
    }

    const ASN1_TIME* pBefore = X509_get0_notBefore(cert);
    struct tm startDate;
    ASN1_TIME_to_tm(pBefore, &startDate);
    SDateTime startDt;
    startDt.m_nDate = (1900 + startDate.tm_year) * 10000 + (startDate.tm_mon + 1) * 100 + startDate.tm_mday;
    startDt.m_nTime = (8 + startDate.tm_hour) * 10000 + startDate.tm_min * 100 + startDate.tm_sec;

    const ASN1_TIME* pLast = X509_get0_notAfter(cert);
    struct tm endDate;
    ASN1_TIME_to_tm(pLast, &endDate);
    SDateTime endDt;
    endDt.m_nDate = (1900 + endDate.tm_year) * 10000 + (endDate.tm_mon + 1) * 100 + endDate.tm_mday;
    endDt.m_nTime = (8 + endDate.tm_hour) * 10000 + endDate.tm_min + endDate.tm_sec;

    time_t timep;
    time(&timep);
    struct tm* pCur = gmtime(&timep);
    SDateTime curDt;
    curDt.m_nDate = (1900 + pCur->tm_year) * 10000 + (pCur->tm_mon + 1) * 100 + pCur->tm_mday;
    curDt.m_nTime = (8 + pCur->tm_hour) * 10000 + pCur->tm_min + pCur->tm_sec;

    if (curDt.m_nDate < startDt.m_nDate) {
        OH_LOG_Print(
            LOG_APP, LOG_ERROR, 0xff00, "ArkWeb", "The starting validity period of the certificate is incorrect");
        return false;
    }

    if (curDt.m_nDate > endDt.m_nDate) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, 0xff00, "ArkWeb", "The expiration date of the certificate is incorrect");
        return false;
    }
    return true;
}

std::string CordovaViewController::getChcpConfigUrl()
{
    return m_xmlParser->getChcpConfigUrl();
}

bool CordovaViewController::getAllAllowUpdatesAutoDownload()
{
    return m_xmlParser->getAllAllowUpdatesAutoDownload();
}

bool CordovaViewController::getAllowUpdatesAutoInstall()
{
    return m_xmlParser->getAllowUpdatesAutoInstall();
}

int CordovaViewController::getActiveInterfaceVersion()
{
    return m_xmlParser->getActiveInterfaceVersion();
}

std::vector<PluginEntry>* CordovaViewController::getPluginEntry()
{
    return m_xmlParser->getPluginEntry();
}

void CordovaViewController::instantiateTsPlugin(const std::string& strWebTag, cJSON* pJson)
{
    m_pluginManager->instantiateTsPlugin(strWebTag, pJson);
}

void CordovaViewController::InitCapacitorTsPlugin(const std::string& strWebTag, const std::string& strJson)
{
    m_capacitorPluginManager->setTsPluginInfo(strWebTag, strJson);
}

void CordovaViewController::setProtocolUrl(const std::vector<std::string>& vecProtocol)
{
    m_xmlParser->setProtocolUrl(vecProtocol);
}

std::vector<std::string>* CordovaViewController::getProtocolUrl()
{
    return m_xmlParser->getProtocolUrl();
}

void CordovaViewController::getCameraImageCompress(bool& isCameraImageCompress,
                                                   long& lngCompressImageSize,
                                                   std::string& strCameraCompressShowToast)
{
    m_xmlParser->getCameraImageCompress(isCameraImageCompress, lngCompressImageSize, strCameraCompressShowToast);
}

void CordovaViewController::setCordovaCacheDuration(const long lngCordovaCacheDuration)
{
    m_xmlParser->setCordovaCacheDuration(lngCordovaCacheDuration);
}

long CordovaViewController::getCordovaCacheDuration()
{
    return m_xmlParser->getCordovaCacheDuration();
}

std::string CordovaViewController::getIonicHostName()
{
    return m_xmlParser->getIonicHostName();
}

std::string CordovaViewController::getIonicScheme()
{
    return m_xmlParser->getIonicScheme();
}

void CordovaViewController::setClientCert(const std::string& strUrl)
{
    m_xmlParser->setClientCert(strUrl);
}

bool CordovaViewController::getClientCert(const std::string& strUrl, std::string& strP12, std::string& strPassword)
{
    return m_xmlParser->getClientCert(strUrl, strP12, strPassword);
}

Capacitor::PluginManager* CordovaViewController::getCapacitorPluginManager()
{
    return m_capacitorPluginManager;
}

Bridge* CordovaViewController::getCapacitorBridge(const std::string& strWebTag)
{
    if (m_mapCapacitorBridge.find(strWebTag) != m_mapCapacitorBridge.end()) {
        return m_mapCapacitorBridge[strWebTag]->getBridge();
    }
    return nullptr;
}