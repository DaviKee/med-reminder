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

#ifndef CSSLSOCKET_H
#define CSSLSOCKET_H

#include "Socket.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include <algorithm>
#include <openssl/core_names.h>
#include "openssl/lhash.h"


#ifndef OPENSSL_VERSION_MAJOR
#error not find openssl library
#elif OPENSSL_VERSION_MAJOR < 3
#error Openssl version must be greater than 3.0
#endif

/**
 * @brief SSL/TLS encrypted communication wrapper (requires OpenSSL 3.0 or higher).
 *      All functions are static, effectively adding a namespace.
 */

const int const_max_key_name = 16;
const int const_max_key_length = PSK_MAX_PSK_LEN;
const int const_max_sess_app_data = 64;
const int const_max_file_name = 512;

struct SOnlySessionTicket {
    void* m_p; // ctx
    int m_nIsOnlySessionTicket;
};

struct SSessionTicket {
    void* m_p; // ctx
    char m_szKeyName[const_max_key_name];
    char m_szAesKey[const_max_key_length];
    char m_szHmackey[const_max_key_length];
};

struct SSessionAppData {
    void* m_p; // ssl
    char m_szAppData[const_max_sess_app_data];
};

struct SClientSessionFileName {
    void* m_p; // ctx
    char m_szFileName[const_max_file_name];
};

struct SSSLPsk {
    void* m_p; // ssl
    char m_szId[const_max_key_name];
    char m_szPsk[const_max_key_length];
    int m_nMaxEarlyData;
};

template <class T> unsigned long OnlySessionTicket_generate_hash(const T* p)
{
    char szBuf[128];
    std::fill(szBuf, szBuf+sizeof(szBuf), 0);
    snprintf(szBuf, sizeof(szBuf), "%p", p->m_p);
    return OPENSSL_LH_strhash(szBuf);
}

template <class T> int OnlySessionTicket_cmp(const T* a, const T* b)
{
    return a->m_p != b->m_p;
}

DEFINE_LHASH_OF(SOnlySessionTicket);
DEFINE_LHASH_OF(SSessionTicket);
DEFINE_LHASH_OF(SSessionAppData);
DEFINE_LHASH_OF(SClientSessionFileName);
DEFINE_LHASH_OF(SSSLPsk);

class CSSLSocket {
private:
    static BIO* s_traceBio;
    static CRYPTO_RWLOCK* s_lock;
    static LHASH_OF(SOnlySessionTicket) * s_onlySessionTicket;
    static LHASH_OF(SSessionTicket) * s_sessionTicket;
    static LHASH_OF(SSessionAppData) * s_sessionAppData;
    static LHASH_OF(SClientSessionFileName) * s_sessionFileName;
    static LHASH_OF(SSSLPsk) * s_psk;

    static void log(CLog::Level enLevenl, const char* pFunName, const char* pFormat, ...);
    static int sslVerifyCallBack(int nVerifyOk, X509_STORE_CTX* p509Ctx);
    static int sslErrorCallback(const char* pErrInfo, size_t uLength, void* u);
    static int sslClientHelloCallBack(SSL* ssl, int* al, void* arg);
    static int sslTLSExtTicketKeyEvpCallback(
        SSL* ssl, unsigned char key_name[16], unsigned char* iv, EVP_CIPHER_CTX* ctx, EVP_MAC_CTX* hctx, int enc);
    static int sslGenerateSessionAppData(SSL* ssl, void* arg);
    static int sslDecryptSessionAppData(SSL* ssl,
                                        SSL_SESSION* ss,
                                        const unsigned char* keyname,
                                        size_t keyname_len,
                                        SSL_TICKET_STATUS status,
                                        void* arg);
    static int sslSaveClientSessionCallBack(SSL* ssl, SSL_SESSION* session);
    static void sslAppInfoCallback(const SSL* ssl, int nWhere, int nRet);
    static int
    sslPskFindSessionCallBack(SSL* ssl, const unsigned char* identity, size_t identity_len, SSL_SESSION** sess);
    static int
    sslUseSessionCallback(SSL* ssl, const EVP_MD* md, const unsigned char** id, size_t* idlen, SSL_SESSION** sess);

public:
    CSSLSocket();
    ~CSSLSocket();
    /**
     * @brief Creates an SSL context.
     * @param isServer rue for server, false for client.
     * @param isVerify 1: enable client verification of server certificate; 0: skip certificate verification.
     * @param nMinTlsVersion Minimum SSL/TLS version.
     * @param isOnlySessionTicket 1: use only session ticket communication (no certificate required, requires server
     * support); 0: disable session ticket.
     * @param nSessionTimeOut Session validity duration.
     * @return SSL_CTX* null if failed
     */
    static SSL_CTX* sslCTXInit(const bool isServer,
                               const bool isVerify = 1,
                               const int nMinTlsVersion = TLS1_3_VERSION,
                               const int isOnlySessionTicket = 1,
                               const int nSessionTimeOut = 5 * 24 * 60 * 60);
    /**
     * @brief Sets the certificate and key.
     * @param ctx SSL context.
     * @param pRootCert File path of the root certificate for client verification of the server.
     * @param pChainCert Certificate path for server verification of the client.
     * @param pPKey Key path for the client certificate.
     * @return true if success, false if failed
     */
    static bool sslCTXSetCertificate(SSL_CTX* ctx,
                                     const char* pRootCert,
                                     const char* pChainCert = nullptr,
                                     const char* pPKey = nullptr);
    /**
     * @brief Sets the session ticket name.
     * @param ctx CTX pointer
     * @param szKeyName  Key name.
     * @param pAesKey AES key
     * @param pHMackey HMAC key
     * @return true if success,false if failed
     */
    static bool sslCTXSetSessionTicketName(SSL_CTX* ctx,
                                           const unsigned char szKeyName[16],
                                           const char* pAesKey,
                                           const char* pHMackey);
    /**
     * @brief Sets the default handler for application data.
     * @param ctx CTX pointer
     * @return true if success,false if failed
     */
    static bool sslCTXSetSessionAppDataFunction(SSL_CTX* ctx);
    /**
     * @brief Sets file path of session
     * @param ctx CTX pointer
     * @param pFilePathName the file path name
     * @return true if success,false if failed
     */
    static bool sslCTXSetClientSessionSavePathName(SSL_CTX* ctx, const char* pFilePathName);
    /**
     * @brief create ssl for ctx
     * @param ctx ssl context
     * @param nSocket the file descriptor sockfd
     * @param isDebug true for debug,false is release
     * @param nHandshakeTimeOut
     * @param nMaxEarlyData
     * @param nFragment
     * @return null for failed
     */
    static SSL* sslInit(SSL_CTX* ctx,
                        int nSocket,
                        const bool isDebug = false,
                        const int nHandshakeTimeOut = 10,
                        const int nMaxEarlyData = 0,
                        const int nFragment = SSL3_RT_MAX_PLAIN_LENGTH);

    /**
     * @brief Sets ssl session
     * @param ssl SSL pointer.
     * @return true if success,false if failed
     */
    static bool sslSetClientSession(SSL* ssl);
    /**
     * @brief Sets app data
     * @param ssl SSL pointer.
     * @param pAppData
     * @return true if success, false if failed
     */
    static bool sslSetSessionAppData(SSL* ssl, const char* pAppData);
    /**
     * @brief Gets app data
     * @param ssl SSL pointer.
     * @param pAppData
     * @return true if success, false if failed
     */
    static bool sslGetSessionAppData(SSL* ssl, char* pAppData, int nDataSize);
    /**
     * @brief Sets psk
     * @param ssl SSL pointer.
     * @param pPskId
     * @param pPskValue
     * @param nMaxEarlyData
     * @return true if success, false if failed
     */
    static bool sslSetPsk(SSL* ssl, const char* pPskId, const char* pPskValue, const int nMaxEarlyData = 0);
    /**
     * @brief Connect server
     * @param ssl SSL pointer.
     * @return true if success, false if failed
     */
    static bool sslConnect(SSL* ssl);
    /**
     * @brief waiting client connect
     * @param ssl SSL pointer.
     * @return true if success, false if failed
     */
    static bool sslAccept(SSL* ssl);
    /**
     * @brief Sends data over SSL.
     * @param ssl SSL pointer.
     * @param pBuf Buffer containing data to send.
     * @param nSize Input: length of data to send; Output: length of data actually sent.
     * @return true if success, false if failed
     */
    static bool sslWrite(SSL* ssl, const char* pBuf, int& nSize);
    /**
     * @brief SSL Checks if a group of SSL connections has data cached.
     * @param ssl Array of SSL connections.
     * @param retSsl Array of SSL connections with cached data.
     * @param nCount Input: size of the group; Output: number of SSL connections with cached data.
     */
    static void sslHasPending(SSL** ssl, SSL** retSsl, int& nCount);
    /**
     * @brief Reads data from an SSL connection.
     * @param ssl SSL pointer.
     * @param pBuf Output buffer.
     * @param nSize Input: buffer length; Output: length of read content.
     * @param isClose Whether the connection is closed. true if close,false if not close
     * @return true if success, false if failed
     */
    static bool sslRead(SSL* ssl, char* pBuf, int& nSize, bool& isClose);
    /**
     * @brief Sends early data.
     * @param ssl SSL pointer
     * @param pBuf Input buffer.
     * @param nSize Input: buffer length; Output: length of sent content.
     * @return true if success, false if failed
     */
    static bool sslWriteEarlyData(SSL* ssl, const char* pBuf, int& nSize);
    /**
     * @brief Reads early data.
     * @param ssl SSL pointer.
     * @param pBuf Output buffer.
     * @param nSize Input: buffer length; Output: length of read content.
     * @return true if success, false if failed
     */
    static bool sslReadEarlyData(SSL* ssl, char* pBuf, int& nSize);
    /**
     * @brief Checks if the session is reused.
     * @param ssl ssl pointer
     * @return true if Reused, false if not Reused
     */
    static bool sslIsSessionReused(SSL* ssl);
    /**
     * @brief free ssl context
     * @param ctx ctx pointer
     * @return true if success, false if failed
     */
    static bool sslCTXFree(SSL_CTX* ctx);
    /**
     * @brief shutdown ssl
     * @param ssl ssl pointer
     * @return true if success, false if failed
     */
    static bool sslShutdown(SSL* ssl);
    /**
     * @brief free ssl
     * @param ssl ssl pointer
     * @return true if success, false if failed
     */
    static bool sslFree(SSL* ssl);
    /**
     * @brief parse certificate of p12
     * @param p12Data Certificate buffer content.
     * @param nLength Buffer length.
     * @param password p12 Password for the P12 certificate.
     * @param pKeyFile Output file path for the key.
     * @param pChainFile Output file path for the certificate and certificate chain.
     * @return 1 if success, 0 if failed
     */
    static int parsePKCS12(
        const char* p12Data, const int nLength, const char* password, const char* pKeyFile, const char* pChainFile);
};

#endif // CSSLSOCKET_H
