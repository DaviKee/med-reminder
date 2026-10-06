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

#ifndef RAWFILE_REQUEST_H
#define RAWFILE_REQUEST_H

#include <map>
#include <mutex>
#include <string>

#include <rawfile/raw_file_manager.h>
#include "web/arkweb_scheme_handler.h"
#include "web/arkweb_net_error_list.h"
#include "Thread.h"

/**
 * @brief A class for handling resource loading and dynamic requests.
 *      This class primarily handles all cross-origin requests. In principle, same-origin requests are handled by the
 * WebView itself. If the caller configures a domain in config.xml, it will also be processed by Cordova. For example:
 *      <cordova-protocol-force value="www.huawei.com" />
 *      If the caller dynamically invokes the following function on the ArkTS side, the configured domain will also be
 * processed by Cordova: SetCordovaProtocolUrl("www.huawei.com");
 */
class RawfileRequest {
    /**Timeout for connecting to the server.*/
    static const int CONNECT_TIME_OUT = 60; // ss
    /**Request URL format：https//www.example.com:8080*/
    std::string m_strSchemeDomainPort;

public:
    /**
     * @brief Constructor for the request.
     * @param[in]  resourceRequest Pointer to the request object.
     * @param[in]  resourceHandler Pointer to the response handler object.
     * @param[in]  resourceManager Pointer to the resource manager object.
     * @param[in]  schemeHandler Pointer to the scheme handler object.
     * @parma[in] SchemeDomainPort
     */
    RawfileRequest(const ArkWeb_ResourceRequest* resourceRequest,
                   const ArkWeb_ResourceHandler* resourceHandler,
                   const NativeResourceManager* resourceManager,
                   const ArkWeb_SchemeHandler* schemeHandler,
                   const std::string& strSchemeDomainPort);
    /**
     * @brief Destructor. Resource cleanup is handled in the Stop function before destruction.
     */
    ~RawfileRequest();

    /**
     * @brief Gets SchemeDomainPort
     * @return std::string
     */
    inline std::string getSchemeDomainPort() const
    {
        return m_strSchemeDomainPort;
    }

    /**
     * @brief Checks if the requested URL is in the whitelist. This is a static function for use by other classes.
     * @param[in]  url Requested URL.
     * @return Whether the URL is in the whitelist: true if whitelisted, false otherwise.
     */
    static bool isAllowUrl(const std::string& strUrl);
    /**
     * @brief Starts processing the web request.
     */
    void start();
    /**
     * @brief Stops the request. This function is triggered when the user closes or destroys the web instance.
     */
    void stop();
    /**
     * @brief Processes the request.
     * @param[in]  pMaxBuffer Pointer to the memory buffer used for processing the request.
     * @param[in]  nBufSize Size of the buffer.
     * @param[in]  nCountOfTask  The index of the task taken from the queue (for logging purposes, helps developers
     * track task counts)
     */
    void readRawfileDataOnWorkerThread(void* pMaxBuffer, const int nBufSize, const int nCountOfTask);
    /**
     * @brief Handles the completion of HTTP headers for the response.
     */
    void didReceiveResponse();
    /**
     * @brief Handles the completion of the response body data. This function can be called multiple times.
     * @param[in]  buffer Pointer to the input data buffer.
     * @param[in]  bufLen Length of the buffer.
     */
    void didReceiveData(const uint8_t* buffer, int64_t bufLen);
    /**
     * @brief Handles the completion of the response.
     */
    void didFinish();
    /**
     * @brief Handles request errors.
     * @param[in]  errorCode Error code.
     */
    void didFailWithError(ArkWeb_NetError errorCode);
    /**
     * @brief Sets a 4XX error.
     */
    void response4XX(const int nHttpCode);
    /**
     * @brief Checks whether the current resource loading or dynamic task is completed.
     */
    bool isTaskFinished();
    /** Buffer for the request body data carried by the WebView during request processing.*/
    std::vector<unsigned char> m_vecRequestHttpBody;

private:
    /**
     * @brief Gets the requested URL.
     * @param[out]  url Output pointer to the URL address. Must be released by calling OH_ArkWeb_ReleaseString
     * @return return result: true if successful, false if failed.
     */
    bool webResourceRequest_GetUrl(char** url);
    /**
     * @brief Gets the request method.
     * @param[out]  method Output pointer to the method address. Must be released by calling OH_ArkWeb_ReleaseString.
     * @return return result:true if successful, false if failed.
     */
    bool webResourceRequest_GetMethod(char** method);
    /**
     * @brief Gets the resource type of the request.
     * @param[out]  resourceType Outputs the resource type of the request.
     * @return return result: true if successful, false if failed.
     */
    bool webResourceRequest_GetResourceType(int& resourceType);
    /**
     * @brief Gets the HTTP headers of the requested resource.
     * @param[out]  headerList Outputs the HTTP headers of the request. Must be released by calling
     * OH_ArkWebRequestHeaderList_Destroy.
     * @return return result: true if successful, false if failed.
     */
    bool webResourceRequest_GetRequestHeaders(ArkWeb_RequestHeaderList** headerList);
    /**
     * @brief Sets the status code for the response.
     * @param[in]  nStatus The status code to set.
     * @return return result: -1 indicates the request ended prematurely and no setting is needed; 0 indicates success.
     * Refer to ArkWeb_NetError codes for other values.
     */
    int webResponse_SetStatus(const int nStatus);
    /**
     * @brief Sets the HTTP header for the response.
     * @param[in]  strName Name of the HTTP header.
     * @param[in]  strName Value of the HTTP header.
     * @return return result: -1 indicates the request ended prematurely and no setting is needed; 0 indicates success.
     */
    int webResponse_SetHeaderByName(const std::string& strName, const std::string& strValue);
    /**
     * @brief Sets the status text for the response.
     * @param[in]  strText The status text.
     * @return return result: -1 indicates the request ended prematurely and no setting is needed; 0 indicates success.
     */
    int webResponse_SetStatusText(const std::string& strText);
    /**
     * @brief Sets the MIME type for the response.
     * @param[in]  strText The MIME type.
     * @return return result: -1 indicates the request ended prematurely and no setting is needed; 0 indicates success.
     */
    int webResponse_SetMimeType(const std::string& strText);
    /**
     * @brief Sets the character set for the response body.
     * @param[in]  strCharset The character set.
     * @return return result: -1 indicates the request ended prematurely and no setting is needed; 0 indicates success.
     */
    int webResponse_SetCharset(const std::string& strCharset);
    /** Pointer to the request object of the current request.*/
    const ArkWeb_ResourceRequest* m_resourceRequest{nullptr};
    /** Pointer to the response handler object of the current request.*/
    const ArkWeb_ResourceHandler* m_resourceHandler{nullptr};
    /** Pointer to the resource manager object used by the current request.*/
    const NativeResourceManager* m_resourceManager{nullptr};
    /** Pointer to the scheme handler object for the current request.*/
    const ArkWeb_SchemeHandler* m_schemeHandler{nullptr};
    /** Pointer to the response object to be returned for this request.*/
    ArkWeb_Response* m_response{nullptr};
    /** Request completion flag, set in multi-threaded environments.*/
    std::atomic<bool> m_stopped{false};
    /** Request completion flag, set in multi-threaded environments.*/
    std::atomic<bool> m_finished{false};
    /** Local sandbox file or rawfile loaded for this request.*/
    std::string m_rawfilePath;
    /** Byte stream pointing to the body of this request.*/
    ArkWeb_HttpBodyStream* m_stream{nullptr};
    /** Thread-safe lock for secure control of request termination checks.*/
    std::mutex m_mutex;
};

/**
 * @brief Defines a grouped thread pool where the domain name of the request serves as the group
 * Group identifiers are dynamically adjusted based on real-time request tasks.
 * Primarily addresses multi-domain network access, ensuring that network speed variations between domains do not
 * interfere with each other.
 */
class CHttpGroupRunner {
    /**
     * @brief Thread pool for handling requests.
     * The thread pool dynamically sets the number of threads at startup, with the set number being the minimum.
     * When Cordova processes request tasks, if there are no idle threads in the pool,
     * new threads are created to serve tasks to improve loading efficiency.
     * There are no tasks waiting in the queue for execution.
     */
    class HttpRunner : public CThreadPool {
        /** Memory buffer size for thread execution input, set in the InitThreadData function.*/
        int m_nBufSize{0};

    public:
        /**
         * @brief Function to execute tasks within a thread.
         * @param[in]  arg Input data pointer.
         * @param[in]  nCountOfTask Index of the task processed from the queue.
         * @param[in]  pTmp Pointer returned by the InitThreadData function during thread initialization.
         */
        void execute(const void* arg, const int nCountOfTask, void* pTmp = nullptr) override
        {
            RawfileRequest* pHttp = static_cast<RawfileRequest*>(const_cast<void*>(arg));
            pHttp->readRawfileDataOnWorkerThread(pTmp, m_nBufSize, nCountOfTask);
        }

        /**
         * @brief Initialization function executed before the thread starts.
         * @return Returns an initialized pointer (typically a buffer allocated for thread execution).
         */
        void* initThreadData() override;
        /**
         * @brief Function executed after the thread ends.
         * @param[in]  arg Pointer returned by the initialization function (typically used to destroy the buffer
         * allocated before thread execution).
         */
        void deInitThreadData(void* pArg) override;
    };

    std::mutex m_mutex;
    /**
     * Maximum number of domains for real-time requests. Exceeding this limit will block new requests.
     * The real-time request domain limit is 100, with a maximum of 20 threads per group and a critical threshold of
     * 2000 threads (rarely reached).
     */
    static const int MAX_URL_BOUNDARY_RUN_TIME = 100;
    std::queue<HttpRunner*> m_queGroupHttp;
    std::map<std::string, HttpRunner*> m_mapUrlToHttpRunner;

public:
    void initGroup(const int nGroupCount, const int nThreadCount);
    bool addTask(const RawfileRequest* rawfileRequest);
};
#endif // RAWFILE_REQUEST_H