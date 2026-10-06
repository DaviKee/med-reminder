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

#include "SSLSocket.h"
#include <algorithm>
#include <openssl/pkcs12.h>
#include <openssl/provider.h>

BIO* CSSLSocket::s_traceBio = BIO_new_fp(stdout, BIO_NOCLOSE | BIO_FP_TEXT);
CRYPTO_RWLOCK* CSSLSocket::s_lock = CRYPTO_THREAD_lock_new();
LHASH_OF(SOnlySessionTicket) * CSSLSocket::s_onlySessionTicket = lh_SOnlySessionTicket_new(
    OnlySessionTicket_generate_hash<SOnlySessionTicket>, OnlySessionTicket_cmp<SOnlySessionTicket>);
LHASH_OF(SSessionTicket) * CSSLSocket::s_sessionTicket =
    lh_SSessionTicket_new(OnlySessionTicket_generate_hash<SSessionTicket>, OnlySessionTicket_cmp<SSessionTicket>);
LHASH_OF(SSessionAppData) * CSSLSocket::s_sessionAppData =
    lh_SSessionAppData_new(OnlySessionTicket_generate_hash<SSessionAppData>, OnlySessionTicket_cmp<SSessionAppData>);
LHASH_OF(SClientSessionFileName) * CSSLSocket::s_sessionFileName = lh_SClientSessionFileName_new(
    OnlySessionTicket_generate_hash<SClientSessionFileName>, OnlySessionTicket_cmp<SClientSessionFileName>);
LHASH_OF(SSSLPsk) * CSSLSocket::s_psk = lh_SSSLPsk_new(OnlySessionTicket_generate_hash<SSSLPsk>,
                                                       OnlySessionTicket_cmp<SSSLPsk>);

CSSLSocket::CSSLSocket() {}

CSSLSocket::~CSSLSocket() {}

void CSSLSocket::log(CLog::Level enLevenl, const char* pFunName, const char* pFormat, ...)
{
    va_list argList;
    va_start(argList, pFormat);
    if (CLog::s_pLog != nullptr) {
        CLog::s_pLog->log(enLevenl, "SSLSocket", pFunName, pFormat, argList);
    }
}

int CSSLSocket::sslVerifyCallBack(int nVerifyOk, X509_STORE_CTX* p509Ctx)
{
    if (nVerifyOk == 0) {
        int nErrorCode = X509_STORE_CTX_get_error(p509Ctx);
        if (nErrorCode != X509_V_OK) {
            log(CLog::Error, "sslVerifyCallBack", "nErrorCode{%d}", nErrorCode);
        }
    }
    log(CLog::Info, "sslVerifyCallBack", "nVerifyOk{%d}", nVerifyOk);
    return nVerifyOk;
}

int CSSLSocket::sslErrorCallback(const char* pErrorInfo, size_t uLength, void* pFunName)
{
    char* pFn = static_cast<char*>(pFunName);
    log(CLog::Error, pFn, "%s", pErrorInfo);
    return 0;
}

int CSSLSocket::sslClientHelloCallBack(SSL* ssl, int* al, void* arg)
{
    SOnlySessionTicket sessTicket;
    sessTicket.m_p = SSL_get_SSL_CTX(ssl);
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLClientHelloCallBack", "CRYPTO_THREAD_read_lock(s_lock)");
        return SSL_CLIENT_HELLO_ERROR;
    }
    SOnlySessionTicket* pSt = lh_SOnlySessionTicket_retrieve(s_onlySessionTicket, &sessTicket);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSt != nullptr && pSt->m_nIsOnlySessionTicket) {
        unsigned int nVersion = SSL_client_hello_get0_legacy_version(ssl);
        int nIsSupportVersion = SSL_client_hello_get0_ext(ssl, TLSEXT_TYPE_supported_versions, nullptr, nullptr);
        int nIsSessionTicket = SSL_client_hello_get0_ext(ssl, TLSEXT_TYPE_session_ticket, nullptr, nullptr);
        if (nVersion < TLS1_2_VERSION || (nVersion == TLS1_2_VERSION && !nIsSupportVersion)) {
            if (!nIsSessionTicket) {
                return SSL_CLIENT_HELLO_ERROR;
            }
        }
    }
    return SSL_CLIENT_HELLO_SUCCESS;
}

int CSSLSocket::sslTLSExtTicketKeyEvpCallback(
    SSL* ssl, unsigned char key_name[16], unsigned char* iv, EVP_CIPHER_CTX* ctx, EVP_MAC_CTX* hctx, int enc)
{
    SSessionTicket sessTicket;
    sessTicket.m_p = SSL_get_SSL_CTX(ssl);
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLTLSExtTicketKeyEvpCallback", "CRYPTO_THREAD_read_lock(s_lock)");
        return 0;
    }
    SSessionTicket* pSt = lh_SSessionTicket_retrieve(s_sessionTicket, &sessTicket);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSt == nullptr) {
        log(CLog::Error, "SSLTLSExtTicketKeyEvpCallback", "lh_SSessionTicket_retrieve(s_sessionTicket, &sessTicket)");
        return 0;
    }

    OSSL_PARAM params[3] = {OSSL_PARAM_END, OSSL_PARAM_END, OSSL_PARAM_END};
    if (enc) {
        std::copy_n(pSt->m_szKeyName, const_max_key_name, key_name);
        if (!EVP_EncryptInit_ex(
                ctx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>(pSt->m_szAesKey), iv)) {
            ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("EVP_EncryptInit_ex"));
            return 0;
        }

        params[0] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, pSt->m_szHmackey, strlen(pSt->m_szHmackey));
        params[1] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, const_cast<char*>("sha256"), 0);
        if (!EVP_MAC_CTX_set_params(hctx, params)) {
            ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("EVP_MAC_CTX_set_params"));
            return 0;
        }
    } else {
        char szBuf[17];
        std::fill(szBuf, szBuf + sizeof(szBuf), 0);
        strncpy(szBuf, reinterpret_cast<char*>(key_name), 16);
        log(CLog::Info, "SSLTLSExtTicketKeyEvpCallback", "key_name{%s}", szBuf);
        if (memcmp(key_name, pSt->m_szKeyName, const_max_key_name) != 0) {
            log(CLog::Error, "SSLTLSExtTicketKeyEvpCallback", "memcmp(key_name, s_szKeyName, 16) != 0");
            return 0;
        }
        params[0] = OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, pSt->m_szHmackey, strlen(pSt->m_szHmackey));
        params[1] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, const_cast<char*>("sha256"), 0);
        if (!EVP_MAC_CTX_set_params(hctx, params)) {
            ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("EVP_MAC_CTX_set_params"));
            return 0;
        }

        if (!EVP_DecryptInit_ex(
                ctx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>(pSt->m_szAesKey), iv)) {
            ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("EVP_DecryptInit_ex"));
            return 0;
        }
    }

    return 1;
}

int CSSLSocket::sslGenerateSessionAppData(SSL* ssl, void* arg)
{
    SSessionAppData sessAppData;
    sessAppData.m_p = ssl;
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLGenerateSessionAppData", "CRYPTO_THREAD_read_lock(s_lock)");
        return 1;
    }
    SSessionAppData* pSt = lh_SSessionAppData_retrieve(s_sessionAppData, &sessAppData);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSt == nullptr) {
        return 1;
    }
    SSL_SESSION* ss = SSL_get0_session(ssl);
    SSL_SESSION_set1_ticket_appdata(ss, pSt->m_szAppData, strlen(pSt->m_szAppData));
    return 1;
}

int CSSLSocket::sslDecryptSessionAppData(
    SSL* ssl, SSL_SESSION* ss, const unsigned char* keyname, size_t keyname_len, SSL_TICKET_STATUS status, void* arg)
{
    if (status == SSL_TICKET_SUCCESS || status == SSL_TICKET_SUCCESS_RENEW) {
        char* pBuf = nullptr;
        size_t nLength = 0;
        SSL_SESSION_get0_ticket_appdata(ss, reinterpret_cast<void**>(&pBuf), &nLength);
        if (pBuf != nullptr) {
            if (nLength < const_max_sess_app_data) {
                SSessionAppData* pSt = static_cast<SSessionAppData*>(OPENSSL_zalloc(sizeof(SSessionAppData)));
                pSt->m_p = ssl;
                std::copy_n(pBuf, nLength, pSt->m_szAppData);
                if (!CRYPTO_THREAD_write_lock(s_lock)) {
                    log(CLog::Error, "SSLDecryptSessionAppData", "CRYPTO_THREAD_write_lock(s_lock)");
                    return SSL_TICKET_RETURN_IGNORE;
                }
                SSessionAppData* pTmp = lh_SSessionAppData_insert(s_sessionAppData, pSt);
                if (pTmp != nullptr) {
                    if (strcmp(pTmp->m_szAppData, pSt->m_szAppData) != 0) {
                        log(CLog::Warn,
                            "SSLDecryptSessionAppData",
                            "strcmp(pTmp->m_szAppData{%s}, pSt->m_szAppData{%s}) != 0",
                            pTmp->m_szAppData,
                            pSt->m_szAppData);
                    }
                    OPENSSL_free(pTmp);
                }
                CRYPTO_THREAD_unlock(s_lock);
            }
        }

        if (status == SSL_TICKET_SUCCESS) {
            return SSL_TICKET_RETURN_USE;
        }

        if (status == SSL_TICKET_SUCCESS_RENEW) {
            return SSL_TICKET_RETURN_USE_RENEW;
        }
    } else if (status == SSL_TICKET_NO_DECRYPT) {
        return SSL_TICKET_RETURN_IGNORE_RENEW;
    }

    return SSL_TICKET_RETURN_IGNORE;
}

int CSSLSocket::sslSaveClientSessionCallBack(SSL* ssl, SSL_SESSION* session)
{
    SClientSessionFileName sessFileName;
    sessFileName.m_p = SSL_get_SSL_CTX(ssl);
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLSaveClientSessionCallBack", "CRYPTO_THREAD_read_lock(s_lock)");
        return 1;
    }
    SClientSessionFileName* pSt = lh_SClientSessionFileName_retrieve(s_sessionFileName, &sessFileName);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSt == nullptr) {
        log(CLog::Error,
            "SSLSaveClientSessionCallBack",
            "lh_SClientSessionFileName_retrieve(s_sessionFileName, &sessFileName)");
        return 0;
    }

    BIO* bio = BIO_new_file(pSt->m_szFileName, "w");
    if (bio == nullptr) {
        log(CLog::Warn, "SSLSaveClientSessionCallBack", "lBIO_new_file(pSt->m_szFileName{%s}, w)", pSt->m_szFileName);
        return 0;
    }
    PEM_write_bio_SSL_SESSION(bio, session);
    BIO_free(bio);
    return 0;
}

void CSSLSocket::sslAppInfoCallback(const SSL* ssl, int nWhere, int nRet)
{
    const char* str;
    if (nWhere & SSL_ST_CONNECT) {
        str = "SSL_connect";
    } else if (nWhere & SSL_ST_ACCEPT) {
        str = "SSL_accept";
    } else {
        str = "undefined";
    }

    if (nWhere & SSL_CB_LOOP) {
        log(CLog::Warn, "SSLSaveClientSessionCallBack", "%s:%s", str, SSL_state_string_long(ssl));
    } else if (nWhere & SSL_CB_ALERT) {
        str = (nWhere & SSL_CB_READ) ? "read" : "write";
        log(CLog::Warn,
            "SSLSaveClientSessionCallBack",
            "SSL3 alert %s:%s:%s",
            str,
            SSL_alert_type_string_long(nRet),
            SSL_alert_desc_string_long(nRet));
    } else if (nWhere & SSL_CB_EXIT) {
        if (nRet == 0) {
            log(CLog::Warn, "SSLSaveClientSessionCallBack", "%s failed", SSL_state_string_long(ssl));
        } else if (nRet < 0) {
            log(CLog::Warn, "SSLSaveClientSessionCallBack", "%s:%s error", str, SSL_state_string_long(ssl));
        }
    }
}

int CSSLSocket::sslPskFindSessionCallBack(SSL* ssl,
                                          const unsigned char* identity,
                                          size_t identity_len,
                                          SSL_SESSION** sess)
{
    SSSLPsk psk;
    psk.m_p = ssl;
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLPskFindSessionCallBack", "CRYPTO_THREAD_read_lock(s_lock)");
        return 0;
    }
    SSSLPsk* pSt = lh_SSSLPsk_retrieve(s_psk, &psk);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSt == nullptr) {
        log(CLog::Error, "SSLPskFindSessionCallBack", "lh_SSSLPsk_retrieve(s_psk, &psk)");
        return 0;
    }

    if (memcmp(pSt->m_szId, identity, identity_len) != 0) {
        log(CLog::Error,
            "SSLPskFindSessionCallBack",
            "memcmp(pSt->m_szId{%s}, identity, identity_len) != 0",
            pSt->m_szId);
        return 0;
    }

    const unsigned char tls13_aes128gcmsha256_id[] = {0x13, 0x01};
    const SSL_CIPHER* cipher = SSL_CIPHER_find(ssl, tls13_aes128gcmsha256_id);
    if (cipher == nullptr) {
        log(CLog::Error, "SSLPskFindSessionCallBack", "cipher == nullptr");
        return 0;
    }

    if (strlen(pSt->m_szPsk) > PSK_MAX_PSK_LEN || strlen(pSt->m_szId) > PSK_MAX_IDENTITY_LEN) {
        log(CLog::Error,
            "SSLPskFindSessionCallBack",
            "strlen(pSt->m_szPsk){%d} > PSK_MAX_PSK_LEN{%d} ||  strlen(pSt->m_szId){%d}> PSK_MAX_IDENTITY_LEN{%d}",
            strlen(pSt->m_szPsk),
            PSK_MAX_PSK_LEN,
            strlen(pSt->m_szId),
            PSK_MAX_IDENTITY_LEN);
        return 0;
    }

    *sess = SSL_SESSION_new();
    if (*sess == nullptr ||
        !SSL_SESSION_set1_master_key(
            *sess, reinterpret_cast<const unsigned char*>(pSt->m_szPsk), strlen(pSt->m_szPsk)) ||
        !SSL_SESSION_set_cipher(*sess, cipher) || !SSL_SESSION_set_protocol_version(*sess, TLS1_3_VERSION)) {
        ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("SSLPskFindSessionCallBack"));
        return 0;
    }

    SSL_SESSION_set_max_early_data(*sess, pSt->m_nMaxEarlyData);
    return 1;
}

int CSSLSocket::sslUseSessionCallback(
    SSL* ssl, const EVP_MD* md, const unsigned char** id, size_t* idlen, SSL_SESSION** sess)
{
    SSSLPsk psk;
    psk.m_p = ssl;
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLUseSessionCallback", "CRYPTO_THREAD_read_lock(s_lock)");
        return 0;
    }
    SSSLPsk* pSt = lh_SSSLPsk_retrieve(s_psk, &psk);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSt == nullptr) {
        log(CLog::Error, "SSLUseSessionCallback", "lh_SSSLPsk_retrieve(s_psk, &psk)");
        return 0;
    }

    *id = reinterpret_cast<const unsigned char*>(pSt->m_szId);
    *idlen = strlen(pSt->m_szId);

    const unsigned char tls13_aes128gcmsha256_id[] = {0x13, 0x01};
    const SSL_CIPHER* cipher = SSL_CIPHER_find(ssl, tls13_aes128gcmsha256_id);
    if (cipher == nullptr) {
        log(CLog::Error, "SSLUseSessionCallback", "cipher == nullptr");
        return 0;
    }

    if (strlen(pSt->m_szPsk) > PSK_MAX_PSK_LEN || strlen(pSt->m_szId) > PSK_MAX_IDENTITY_LEN) {
        log(CLog::Error,
            "SSLUseSessionCallback",
            "strlen(pSt->m_szPsk){%d} > PSK_MAX_PSK_LEN{%d} ||  strlen(pSt->m_szId){%d}> PSK_MAX_IDENTITY_LEN{%d}",
            strlen(pSt->m_szPsk),
            PSK_MAX_PSK_LEN,
            strlen(pSt->m_szId),
            PSK_MAX_IDENTITY_LEN);
        return 0;
    }

    *sess = SSL_SESSION_new();
    if (*sess == nullptr ||
        !SSL_SESSION_set1_master_key(
            *sess, reinterpret_cast<const unsigned char*>(pSt->m_szPsk), strlen(pSt->m_szPsk)) ||
        !SSL_SESSION_set_cipher(*sess, cipher) || !SSL_SESSION_set_protocol_version(*sess, TLS1_3_VERSION)) {
        ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("SSLUseSessionCallback"));
        return 0;
    }

    SSL_SESSION_set_max_early_data(*sess, pSt->m_nMaxEarlyData);
    return 1;
}

SSL_CTX* CSSLSocket::sslCTXInit(const bool isServer,
                                const bool isVerify,
                                const int nMinTlsVersion,
                                const int isOnlySessionTicket,
                                const int nSessionTimeOut)
{
    SSL_CTX* ctx = nullptr;
    if (isServer) {
        ctx = SSL_CTX_new(TLS_server_method());
    } else {
        ctx = SSL_CTX_new(TLS_client_method());
    }

    if (ctx == nullptr) {
        ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("SSL_CTX_new"));
        return nullptr;
    }
    if (isVerify) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, sslVerifyCallBack);
    } else {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    }

    if (isServer && isVerify) {
        int nIdContext = 1;
        SSL_CTX_set_session_id_context(ctx, reinterpret_cast<const unsigned char*>(&nIdContext), sizeof(nIdContext));
    }

    SSL_CTX_set_min_proto_version(ctx, nMinTlsVersion);
    SSL_CTX_set_timeout(ctx, nSessionTimeOut);
    if (isOnlySessionTicket) {
        SOnlySessionTicket* pSt = static_cast<SOnlySessionTicket*>(OPENSSL_zalloc(sizeof(SOnlySessionTicket)));
        pSt->m_p = ctx;
        pSt->m_nIsOnlySessionTicket = isOnlySessionTicket;
        if (!CRYPTO_THREAD_write_lock(s_lock)) {
            log(CLog::Error, "SSLCTXInit", "CRYPTO_THREAD_write_lock(s_lock)");
            return nullptr;
        }
        SOnlySessionTicket* pTmp = lh_SOnlySessionTicket_insert(s_onlySessionTicket, pSt);
        if (pTmp != nullptr) {
            OPENSSL_free(pTmp);
        }
        CRYPTO_THREAD_unlock(s_lock);
    }

    if (isServer && isOnlySessionTicket) {
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    }

    if (!isServer) {
        SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    }

    SSL_CTX_set_timeout(ctx, nSessionTimeOut);
    SSL_CTX_set_client_hello_cb(ctx, CSSLSocket::sslClientHelloCallBack, nullptr);
    return ctx;
}

bool CSSLSocket::sslCTXSetCertificate(SSL_CTX* ctx, const char* pRootCert, const char* pChainCert, const char* pPKey)
{
    if (ctx == nullptr) {
        log(CLog::Error, "SSLCTXSetCertificate", "ctx == nullptr");
        return false;
    }

    if (pRootCert != nullptr && !SSL_CTX_load_verify_locations(ctx, pRootCert, nullptr)) {
        ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("SSL_CTX_load_verify_locations"));
        return false;
    }
    if (pChainCert != nullptr && !SSL_CTX_use_certificate_chain_file(ctx, pChainCert)) {
        ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("SSL_CTX_use_certificate_file"));
        return false;
    }

    if (pPKey != nullptr && !SSL_CTX_use_PrivateKey_file(ctx, pPKey, SSL_FILETYPE_PEM)) {
        ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("SSL_CTX_use_PrivateKey_file"));
        return false;
    }

    if (pChainCert != nullptr && pPKey != nullptr && !SSL_CTX_check_private_key(ctx)) {
        ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("SSL_CTX_check_private_key"));
        return false;
    }

    return true;
}

bool CSSLSocket::sslCTXSetSessionTicketName(SSL_CTX* ctx,
                                            const unsigned char szKeyName[16],
                                            const char* pAesKey,
                                            const char* pHMackey)
{
    if (pAesKey == nullptr || pHMackey == nullptr || szKeyName == nullptr) {
        log(CLog::Error,
            "SSLCTXSetSessionTicketName",
            "pAesKey == nullptr || pHMackey == nullptr || szkeyName == nullptr");
        return false;
    }

    if (strlen(pAesKey) >= const_max_key_length || strlen(pHMackey) >= const_max_key_length) {
        log(CLog::Error,
            "SSLCTXSetSessionTicketName",
            "strlen(pAesKey){%d} > const_max_key_length || strlen(pHMackey){%d} > const_max_key_length{%d}",
            strlen(pAesKey),
            strlen(pHMackey),
            const_max_key_length);
        return false;
    }

    SSessionTicket* pSt = static_cast<SSessionTicket*>(OPENSSL_zalloc(sizeof(SSessionTicket)));
    pSt->m_p = ctx;
    std::copy_n(szKeyName, 16, pSt->m_szKeyName);
    snprintf(pSt->m_szAesKey, sizeof(pSt->m_szAesKey), "%s", pAesKey);
    snprintf(pSt->m_szHmackey, sizeof(pSt->m_szHmackey), "%s", pHMackey);
    if (!CRYPTO_THREAD_write_lock(s_lock)) {
        log(CLog::Error, "SSLCTXSetSessionTicketName", "CRYPTO_THREAD_write_lock(s_lock)");
        return false;
    }
    SSessionTicket* pTmp = lh_SSessionTicket_insert(s_sessionTicket, pSt);
    if (pTmp != nullptr) {
        OPENSSL_free(pTmp);
    }
    CRYPTO_THREAD_unlock(s_lock);
    SSL_CTX_set_tlsext_ticket_key_evp_cb(ctx, CSSLSocket::sslTLSExtTicketKeyEvpCallback);
    return true;
}

bool CSSLSocket::sslCTXSetSessionAppDataFunction(SSL_CTX* ctx)
{
    return SSL_CTX_set_session_ticket_cb(ctx, sslGenerateSessionAppData, sslDecryptSessionAppData, nullptr);
}

bool CSSLSocket::sslCTXSetClientSessionSavePathName(SSL_CTX* ctx, const char* pFilePathName)
{
    if (strlen(pFilePathName) >= const_max_file_name) {
        log(CLog::Error,
            "SSLCTXSetClientSessionSavePathName",
            "strlen(pFilePathName){%d} >= const_max_file_name{%d}",
            strlen(pFilePathName),
            const_max_file_name);
        return false;
    }
    SClientSessionFileName* pSt = static_cast<SClientSessionFileName*>(OPENSSL_zalloc(sizeof(SClientSessionFileName)));
    pSt->m_p = ctx;
    snprintf(pSt->m_szFileName, sizeof(pSt->m_szFileName), "%s", pFilePathName);
    if (!CRYPTO_THREAD_write_lock(s_lock)) {
        log(CLog::Error, "SSLCTXSetClientSessionSavePathName", "CRYPTO_THREAD_write_lock(s_lock)");
        return false;
    }
    SClientSessionFileName* pTmp = lh_SClientSessionFileName_insert(s_sessionFileName, pSt);
    if (pTmp != nullptr) {
        OPENSSL_free(pTmp);
    }
    CRYPTO_THREAD_unlock(s_lock);
    SSL_CTX_sess_set_new_cb(ctx, sslSaveClientSessionCallBack);
    return true;
}

SSL* CSSLSocket::sslInit(SSL_CTX* ctx,
                         int nSocket,
                         const bool isDebug,
                         const int nHandshakeTimeOut,
                         const int nMaxEarlyData,
                         const int nFragment)
{
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, nSocket);

    struct timeval tv;
    tv.tv_sec = nHandshakeTimeOut;
    tv.tv_usec = 0;
    CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    CSocket::setSockOpt(nSocket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLCTXInit", "CRYPTO_THREAD_read_lock(s_lock)");
        return nullptr;
    }
    SOnlySessionTicket onlySessTicket;
    onlySessTicket.m_p = ctx;
    SOnlySessionTicket* pOst = lh_SOnlySessionTicket_retrieve(s_onlySessionTicket, &onlySessTicket);
    if (pOst != nullptr && pOst->m_nIsOnlySessionTicket == 1) {
        SSL_set_options(ssl, SSL_OP_NO_ANTI_REPLAY);
    }
    CRYPTO_THREAD_unlock(s_lock);

    SSL_set_max_early_data(ssl, nMaxEarlyData);
    SSL_set_num_tickets(ssl, 1);
    SSL_set_max_send_fragment(ssl, nFragment);
    SSL_set_split_send_fragment(ssl, nFragment);

    if (isDebug) {
        SSL_set_msg_callback(ssl, SSL_trace);
        SSL_set_msg_callback_arg(ssl, s_traceBio);
        SSL_set_info_callback(ssl, sslAppInfoCallback);
    }

    return ssl;
}

bool CSSLSocket::sslSetClientSession(SSL* ssl)
{
    SClientSessionFileName sessFileName;
    sessFileName.m_p = SSL_get_SSL_CTX(ssl);
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLSetClientSession", "CRYPTO_THREAD_read_lock(s_lock)");
        return false;
    }

    SSSLPsk psk;
    psk.m_p = ssl;
    SSSLPsk* pPsk = lh_SSSLPsk_retrieve(s_psk, &psk);
    if (pPsk != nullptr) {
        CRYPTO_THREAD_unlock(s_lock);
        log(CLog::Warn, "SSLSetClientSession", "Unsupported set session for psk");
        return true;
    }

    SClientSessionFileName* pSt = lh_SClientSessionFileName_retrieve(s_sessionFileName, &sessFileName);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSt == nullptr) {
        log(CLog::Warn, "SSLSetClientSession", "lh_SClientSessionFileName_retrieve(s_sessionFileName, &sessFileName)");
        return true;
    }

    BIO* bio = BIO_new_file(pSt->m_szFileName, "r");
    if (bio == nullptr) {
        log(CLog::Warn, "SSLSetClientSession", "lBIO_new_file(pSt->m_szFileName{%s}, w)", pSt->m_szFileName);
        return true;
    }
    SSL_SESSION* session = PEM_read_bio_SSL_SESSION(bio, nullptr, 0, nullptr);
    if (session != nullptr) {
        SSL_set_session(ssl, session);
        SSL_SESSION_free(session);
    }
    BIO_free(bio);
    return true;
}

bool CSSLSocket::sslSetSessionAppData(SSL* ssl, const char* pAppData)
{
    if (strlen(pAppData) > const_max_sess_app_data) {
        log(CLog::Error,
            "SSLSetSessionAppData",
            "strlen(pAppData){%d} > const_max_sess_app_data{%d}",
            strlen(pAppData),
            const_max_sess_app_data);
        return false;
    }

    if (!CRYPTO_THREAD_write_lock(s_lock)) {
        log(CLog::Error, "SSLSetSessionAppData", "CRYPTO_THREAD_write_lock(s_lock)");
        return false;
    }

    SSessionTicket sessionTicket;
    sessionTicket.m_p = SSL_get_SSL_CTX(ssl);
    SSessionTicket* pSt = static_cast<SSessionTicket*>(lh_SSessionTicket_retrieve(s_sessionTicket, &sessionTicket));
    if (pSt != nullptr) {
        CRYPTO_THREAD_unlock(s_lock);
        log(CLog::Warn, "SSLSetSessionAppData", "Unsupported appData for seted ke_name and key");
        return true;
    }

    SSSLPsk psk;
    psk.m_p = ssl;
    SSSLPsk* pPsk = lh_SSSLPsk_retrieve(s_psk, &psk);
    if (pPsk != nullptr) {
        CRYPTO_THREAD_unlock(s_lock);
        log(CLog::Warn, "SSLSetSessionAppData", "Unsupported set session for psk");
        return true;
    }

    SSessionAppData* pSA = static_cast<SSessionAppData*>(OPENSSL_zalloc(sizeof(SSessionAppData)));
    pSA->m_p = ssl;
    snprintf(pSA->m_szAppData, sizeof(pSA->m_szAppData), "%s", pAppData);
    SSessionAppData* pTmp = lh_SSessionAppData_insert(s_sessionAppData, pSA);
    if (pTmp != nullptr) {
        OPENSSL_free(pTmp);
    }
    CRYPTO_THREAD_unlock(s_lock);

    if (SSL_is_init_finished(ssl)) {
        SSL_new_session_ticket(ssl);
        SSL_do_handshake(ssl);
    }
    return true;
}

bool CSSLSocket::sslGetSessionAppData(SSL* ssl, char* pAppData, int nDataSize)
{
    if (!CSSLSocket::sslIsSessionReused(ssl)) {
        log(CLog::Warn, "SSLGetSessionAppData", "CSSLSocket::sslIsSessionReused(ssl) == false");
        return true;
    }

    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "SSLGetSessionAppData", "CRYPTO_THREAD_read_lock(s_lock)");
        return false;
    }

    SSessionTicket sessionTicket;
    sessionTicket.m_p = SSL_get_SSL_CTX(ssl);
    SSessionTicket* pSt = static_cast<SSessionTicket*>(lh_SSessionTicket_retrieve(s_sessionTicket, &sessionTicket));
    if (pSt != nullptr) {
        CRYPTO_THREAD_unlock(s_lock);
        log(CLog::Warn, "SSLGetSessionAppData", "Unsupported appData because seted ke_name and key");
        return true;
    }

    SSSLPsk psk;
    psk.m_p = ssl;
    SSSLPsk* pPsk = lh_SSSLPsk_retrieve(s_psk, &psk);
    if (pPsk != nullptr) {
        CRYPTO_THREAD_unlock(s_lock);
        log(CLog::Warn, "SSLGetSessionAppData", "Unsupported set session for psk");
        return true;
    }

    SSessionAppData sessAppData;
    sessAppData.m_p = ssl;
    SSessionAppData* pSa = lh_SSessionAppData_retrieve(s_sessionAppData, &sessAppData);
    CRYPTO_THREAD_unlock(s_lock);
    if (pSa == nullptr) {
        log(CLog::Warn, "SSLGetSessionAppData", "App data without seted");
        return true;
    }
    snprintf(pAppData, nDataSize, "%s", pSa->m_szAppData);
    return true;
}

bool CSSLSocket::sslSetPsk(SSL* ssl, const char* pPskId, const char* pPskValue, const int nMaxEarlyData)
{
    if (strlen(pPskId) >= const_max_key_name || strlen(pPskValue) >= const_max_key_length) {
        log(CLog::Error,
            "SSLSetPsk",
            "strlen(pPskId){%d} >= const_max_key_name{%d} || strlen(pPskValue){%d} >= const_max_key_length{%d}",
            strlen(pPskId),
            const_max_key_name,
            strlen(pPskValue),
            const_max_key_length);
        return false;
    }

    SSSLPsk* pSt = static_cast<SSSLPsk*>(OPENSSL_zalloc(sizeof(SSSLPsk)));
    pSt->m_p = ssl;
    snprintf(pSt->m_szId, sizeof(pSt->m_szId), "%s", pPskId);
    snprintf(pSt->m_szPsk, sizeof(pSt->m_szPsk), "%s", pPskValue);
    pSt->m_nMaxEarlyData = nMaxEarlyData;
    if (!CRYPTO_THREAD_write_lock(s_lock)) {
        log(CLog::Error, "SSLSetPsk", "CRYPTO_THREAD_write_lock(s_lock)");
        return false;
    }
    SSSLPsk* pTmp = lh_SSSLPsk_insert(s_psk, pSt);
    if (pTmp != nullptr) {
        OPENSSL_free(pTmp);
    }
    CRYPTO_THREAD_unlock(s_lock);

    SSL_set_session_id_context(ssl, nullptr, 0);

    if (SSL_is_server(ssl)) {
        SSL_set_psk_find_session_callback(ssl, sslPskFindSessionCallBack);
    } else {
        SSL_set_psk_use_session_callback(ssl, sslUseSessionCallback);
    }
    return true;
}

bool CSSLSocket::sslConnect(SSL* ssl)
{
    while (true) {
        if (SSL_connect(ssl) <= 0) {
            int nErrno = errno;
            if (BIO_get_flags(SSL_get_rbio(ssl)) & BIO_FLAGS_SHOULD_RETRY) {
                log(CLog::Warn, "sslConnect", "shuld retry:{%d}", nErrno);
            }

            if (nErrno == EINTR) {
                continue;
            }

            int nErrCode = SSL_get_error(ssl, 0);
            if (nErrCode == SSL_ERROR_WANT_READ) {
                log(CLog::Warn, "sslConnect", "nErrCode{%d} == SSL_ERROR_WANT_READ", nErrCode);
                return false;
            }

            if (nErrCode == SSL_ERROR_WANT_WRITE) {
                log(CLog::Warn, "sslConnect", "nErrCode{%d} == SSL_ERROR_WANT_WRITE", nErrCode);
                return false;
            }
            ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("ssl_connect"));
            return false;
        }
        break;
    }
    return true;
}

bool CSSLSocket::sslAccept(SSL* ssl)
{
    while (true) {
        if (SSL_accept(ssl) <= 0) {
            int nErrno = errno;
            if (BIO_get_flags(SSL_get_rbio(ssl)) & BIO_FLAGS_SHOULD_RETRY) {
                log(CLog::Warn, "sslAccept", "shuld retry:{%d}", nErrno);
            }

            if (nErrno == EINTR) {
                continue;
            }

            int nErrCode = SSL_get_error(ssl, 0);
            if (nErrCode == SSL_ERROR_WANT_CLIENT_HELLO_CB) {
                log(CLog::Warn, "sslAccept", "nErrCode{%d} == SSL_ERROR_WANT_CLIENT_HELLO_CB", nErrCode);
                continue;
            }
            if (nErrCode == SSL_ERROR_WANT_READ) {
                log(CLog::Warn, "sslAccept", "nErrCode{%d} == SSL_ERROR_WANT_READ", nErrCode);
                return false;
            }

            if (nErrCode == SSL_ERROR_WANT_WRITE) {
                log(CLog::Warn, "sslAccept", "nErrCode{%d} == SSL_ERROR_WANT_WRITE", nErrCode);
                return false;
            }

            ERR_print_errors_cb(CSSLSocket::sslErrorCallback, const_cast<char*>("ssl_accept"));
            return false;
        }
        break;
    }
    return true;
}

bool CSSLSocket::sslWrite(SSL* ssl, const char* pBuf, int& nSize)
{
    while (true) {
        nSize = SSL_write(ssl, pBuf, nSize);
        if (nSize <= 0) {
            int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "sslWrite", "write EINTR");
                continue;
            }

            int nErrCode = SSL_get_error(ssl, 0);
            log(CLog::Error,
                "sslWrite",
                " SSLWrite(ssl{%p}, pBuf{%p}, nSize{%d}){error:%d, sslError:%d, errno:%d}",
                ssl,
                pBuf,
                nSize,
                pErrMsg,
                nErrCode,
                nErrNo);
            return false;
        }
        break;
    }
    return true;
}

void CSSLSocket::sslHasPending(SSL** ssl, SSL** retSsl, int& nCount)
{
    const int const_count = nCount;
    nCount = 0;
    for (int i = 0; i < const_count; i++) {
        if (SSL_has_pending(ssl[i])) {
            retSsl[nCount] = ssl[i];
            nCount++;
        }
    }
    return;
}

bool CSSLSocket::sslRead(SSL* ssl, char* pBuf, int& nSize, bool& isClose)
{
    const int nMaxSize = nSize;
    nSize = 0;
    isClose = false;
    int nReadPos = 0;
    while (true) {
        int nReadSize = SSL_read(ssl, pBuf + nReadPos, nMaxSize - nReadPos);
        if (nReadSize <= 0) {
            int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "sslRead", "read  EINTR");
                continue;
            }

            int nReadError = SSL_get_error(ssl, 0);
            isClose = true;
            nSize = nReadPos;
            log(CLog::Warn,
                "sslRead",
                "sslRead(ssl{%p}, pBuf, nSize){error:%d, sslErro:%d,errno:%d}",
                ssl,
                pErrMsg,
                nReadError,
                nErrNo);
            return false;
        }
        nReadPos += nReadSize;
        if (SSL_has_pending(ssl) && nMaxSize > nReadPos) {
            continue;
        }
        nSize = nReadPos;
        break;
    }
    return true;
}

bool CSSLSocket::sslWriteEarlyData(SSL* ssl, const char* pBuf, int& nSize)
{
    if (SSL_is_init_finished(ssl)) {
        log(CLog::Error, "sslWriteEarlyData", "ssl_is_init_finished(ssl)");
        return false;
    }

    SSSLPsk psk;
    psk.m_p = ssl;
    if (!CRYPTO_THREAD_read_lock(s_lock)) {
        log(CLog::Error, "sslWriteEarlyData", "CRYPTO_THREAD_read_lock(s_lock)");
        return false;
    }
    SSSLPsk* pPsk = lh_SSSLPsk_retrieve(s_psk, &psk);
    CRYPTO_THREAD_unlock(s_lock);

    int nMaxEarlyData = 0;
    if (pPsk != nullptr) {
        nMaxEarlyData = pPsk->m_nMaxEarlyData;
    } else {
        if (SSL_is_server(ssl)) {
            nMaxEarlyData = SSL_get_max_early_data(ssl);
        } else {
            SSL_SESSION* session = SSL_get_session(ssl);
            if (session != nullptr) {
                nMaxEarlyData = SSL_SESSION_get_max_early_data(session);
            }
        }
    }

    if (nMaxEarlyData < nSize) {
        log(CLog::Warn, "sslWriteEarlyData", "nMaxEarlyData{%d} < nSize{%d}", nMaxEarlyData, nSize);
        return true;
    }

    size_t uSize = nSize;
    size_t uSendSize = nSize;
    nSize = 0;
    while (true) {
        if (!SSL_write_early_data(ssl, pBuf, uSize, &uSendSize)) {
            int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "sslWriteEarlyData", "read  EINTR");
                continue;
            }
            int nErrCode = SSL_get_error(ssl, 0);
            log(CLog::Error,
                "sslWriteEarlyData",
                "ssl_write_early_data(ssl{%p}, pBuf, uSize{%u}, nSize){error:%d, sslErro:%d,errno:%d}",
                ssl,
                uSize,
                pErrMsg,
                nErrCode,
                nErrNo);
            return false;
        }
        nSize = static_cast<int>(uSendSize);
        break;
    }
    return true;
}

bool CSSLSocket::sslReadEarlyData(SSL* ssl, char* pBuf, int& nSize)
{
    if (SSL_is_init_finished(ssl)) {
        log(CLog::Error, "sslReadEarlyData", "ssl_is_init_finished(ssl)");
        return false;
    }

    if (!SSL_is_server(ssl)) {
        log(CLog::Error, "sslReadEarlyData", "!SSL_is_server(ssl)");
        return false;
    }

    size_t uSize = nSize;
    nSize = 0;
    size_t uRecvSize = 0;
    while (true) {
        int nReadState = SSL_read_early_data(ssl, pBuf, uSize, &uRecvSize);
        if (nReadState == SSL_READ_EARLY_DATA_ERROR) {
            int nErrNo = errno;
            const char* pErrMsg = strerror(nErrNo);
            if (nErrNo == EINTR) {
                log(CLog::Warn, "sslReadEarlyData", "read  EINTR");
                continue;
            }
            int nErrCode = SSL_get_error(ssl, 0);
            log(CLog::Error,
                "sslReadEarlyData",
                "sslReadEarlyData(ssl{%p}, pBuf, uSize{%u}, nSize){error:%d, sslErro:%d,errno:%d}",
                ssl,
                uSize,
                pErrMsg,
                nErrCode,
                nErrNo);
            return false;
        } else if (nReadState == SSL_READ_EARLY_DATA_FINISH) {
            nSize = 0;
            break;
        } else if (nReadState == SSL_READ_EARLY_DATA_SUCCESS) {
            nSize = static_cast<int>(uRecvSize);
            break;
        } else {
            log(CLog::Error,
                "sslReadEarlyData",
                "sslReadEarlyData(ssl{%p}, pBuf, uSize{%u}, nSize){nReadState:%d}",
                ssl,
                uSize,
                nReadState);
            return false;
        }
    }
    return true;
}

bool CSSLSocket::sslIsSessionReused(SSL* ssl)
{
    return SSL_session_reused(ssl);
}

bool CSSLSocket::sslCTXFree(SSL_CTX* ctx)
{
    if (!CRYPTO_THREAD_write_lock(s_lock)) {
        log(CLog::Error, "sslCTXFree", "CRYPTO_THREAD_write_lock(s_lock)");
        return false;
    }

    SOnlySessionTicket onlySessTicket;
    onlySessTicket.m_p = ctx;
    SOnlySessionTicket* pOst = lh_SOnlySessionTicket_delete(s_onlySessionTicket, &onlySessTicket);
    if (pOst != nullptr) {
        OPENSSL_free(pOst);
    }

    SSessionTicket sessTicket;
    sessTicket.m_p = ctx;
    SSessionTicket* pSt = lh_SSessionTicket_delete(s_sessionTicket, &sessTicket);
    if (pSt != nullptr) {
        OPENSSL_free(pSt);
    }

    SClientSessionFileName sessFileName;
    sessFileName.m_p = ctx;
    SClientSessionFileName* pSfn = lh_SClientSessionFileName_delete(s_sessionFileName, &sessFileName);
    if (pSfn != nullptr) {
        OPENSSL_free(pSfn);
    }

    CRYPTO_THREAD_unlock(s_lock);

    SSL_CTX_free(ctx);
    return true;
}

bool CSSLSocket::sslShutdown(SSL* ssl)
{
    for (int i = 0; i < 2; i++) {
        int nShutState = SSL_shutdown(ssl);
        if (nShutState == 0) {
            continue;
        }
        if (nShutState == 1) {
            return nShutState;
        }
    }

    int nErrCode = SSL_get_error(ssl, 0);
    log(CLog::Error, "sslShutdown", "ssl_shutdown(ssl){ nErrCode:%d}", nErrCode);
    return false;
}

bool CSSLSocket::sslFree(SSL* ssl)
{
    if (!CRYPTO_THREAD_write_lock(s_lock)) {
        log(CLog::Error, "sslFree", "CRYPTO_THREAD_write_lock(s_lock)");
        return false;
    }

    SSessionAppData appData;
    appData.m_p = ssl;
    SSessionAppData* pAd = lh_SSessionAppData_delete(s_sessionAppData, &appData);
    if (pAd != nullptr) {
        OPENSSL_free(pAd);
    }

    SSSLPsk psk;
    psk.m_p = ssl;
    SSSLPsk* pPsk = lh_SSSLPsk_delete(s_psk, &psk);
    if (pPsk != nullptr) {
        OPENSSL_free(pPsk);
    }

    CRYPTO_THREAD_unlock(s_lock);

    SSL_free(ssl);
    return true;
}

int CSSLSocket::parsePKCS12(
    const char* p12Data, const int nLength, const char* password, const char* pKeyFile, const char* pChainFile)
{
    BIO* p12_bio = nullptr;
    PKCS12* p12 = nullptr;
    EVP_PKEY* pkey = nullptr;
    X509* cert = nullptr;
    STACK_OF(X509)* ca = nullptr;
    BIO *bioKey = nullptr, *bioCert = nullptr;
    int ret = 0;

    // 1. 打开并读取PKCS12文件
    p12_bio = BIO_new_mem_buf(p12Data, nLength);
    if (!p12_bio) {
        log(CLog::Error, "parsePKCS12", "BIO_new_mem_buf(p12Data, strlen(p12Data))");
        goto end;
    }

    p12 = d2i_PKCS12_bio(p12_bio, nullptr);
    if (!p12) {
        log(CLog::Error, "parsePKCS12", "d2i_PKCS12_bio(p12_bio, nullptr)");
        goto end;
    }

    // 2. 解析PKCS12内容
    if (!PKCS12_parse(p12, password, &pkey, &cert, &ca)) {
        unsigned long lngErrorCode = ERR_get_error();
        char* pErrorInfo = ERR_error_string(lngErrorCode, nullptr);
        log(CLog::Error,
            "parsePKCS12",
            "PKCS12_parse(p12, password, &pkey, &cert, &ca):ErrorCode{%ld},ErrorInfo{%s}",
            lngErrorCode,
            pErrorInfo);
        goto end;
    }

    // 3. 保存私钥到单独文件
    bioKey = BIO_new_file(pKeyFile, "w");
    if (!bioKey) {
        log(CLog::Error, "parsePKCS12", "BIO_new_file(pKeyFile, w)");
        goto end;
    }
    if (!PEM_write_bio_PrivateKey(bioKey, pkey, nullptr, nullptr, 0, nullptr, nullptr)) {
        log(CLog::Error,
            "parsePKCS12",
            "PEM_write_bio_PrivateKey(bioKey, pkey, nullptr, nullptr, 0, nullptr, nullptr)");
        goto end;
    }

    // 4. 验证私钥
    if (!pkey || !cert || !X509_check_private_key(cert, pkey)) {
        log(CLog::Error, "parsePKCS12", "Invalid key/cert pair");
        goto end;
    }

    // 4. 创建合并的证书文件（证书+证书链）
    bioCert = BIO_new_file(pChainFile, "w");
    if (!bioCert) {
        log(CLog::Error, "parsePKCS12", "BIO_new_file(pChainFile, w)");
        goto end;
    }

    // 先写入终端证书
    if (!PEM_write_bio_X509(bioCert, cert)) {
        log(CLog::Error, "parsePKCS12", "PEM_write_bio_X509(combined_cert_bio, cert)");
        goto end;
    }

    // 然后写入所有CA证书（证书链）
    if (ca && sk_X509_num(ca) > 0) {
        for (int i = 0; i < sk_X509_num(ca); i++) {
            X509* ca_cert = sk_X509_value(ca, i);
            if (!PEM_write_bio_X509(bioCert, ca_cert)) {
                log(CLog::Error, "parsePKCS12", "PEM_write_bio_X509(combined_cert_bio, ca_cert)");
                goto end;
            }
        }
    }
    ret = 1;
end:
    // 清理资源
    if (p12_bio)
        BIO_free(p12_bio);
    if (p12)
        PKCS12_free(p12);
    if (pkey)
        EVP_PKEY_free(pkey);
    if (cert)
        X509_free(cert);
    if (ca)
        sk_X509_pop_free(ca, X509_free);
    if (bioKey)
        BIO_free(bioKey);
    if (bioCert)
        BIO_free(bioCert);
    return ret;
}