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

#ifndef MYAPPLICATION_HTTPCODE_H
#define MYAPPLICATION_HTTPCODE_H

namespace HttpCode {

// Informational 1xx
constexpr int CONTINUE = 100;
constexpr int SWITCHING_PROTOCOLS = 101;
constexpr int PROCESSING = 102;
constexpr int EARLY_HINTS = 103;

// Success 2xx
constexpr int OK = 200;
constexpr int CREATED = 201;
constexpr int ACCEPTED = 202;
constexpr int NON_AUTHORITATIVE_INFORMATION = 203;
constexpr int NO_CONTENT = 204;
constexpr int RESET_CONTENT = 205;
constexpr int PARTIAL_CONTENT = 206;
constexpr int MULTI_STATUS = 207;
constexpr int ALREADY_REPORTED = 208;
constexpr int IM_USED = 226;

// Redirection 3xx
constexpr int MULTIPLE_CHOICES = 300;
constexpr int MOVED_PERMANENTLY = 301;
constexpr int FOUND = 302;
constexpr int SEE_OTHER = 303;
constexpr int NOT_MODIFIED = 304;
constexpr int USE_PROXY = 305;
constexpr int TEMPORARY_REDIRECT = 307;
constexpr int PERMANENT_REDIRECT = 308;

// Client Error 4xx
constexpr int BAD_REQUEST = 400;
constexpr int UNAUTHORIZED = 401;
constexpr int PAYMENT_REQUIRED = 402;
constexpr int FORBIDDEN = 403;
constexpr int NOT_FOUND = 404;
constexpr int METHOD_NOT_ALLOWED = 405;
constexpr int NOT_ACCEPTABLE = 406;
constexpr int PROXY_AUTHENTICATION_REQUIRED = 407;
constexpr int REQUEST_TIMEOUT = 408;
constexpr int CONFLICT = 409;
constexpr int GONE = 410;
constexpr int LENGTH_REQUIRED = 411;
constexpr int PRECONDITION_FAILED = 412;
constexpr int PAYLOAD_TOO_LARGE = 413;
constexpr int URI_TOO_LONG = 414;
constexpr int UNSUPPORTED_MEDIA_TYPE = 415;
constexpr int RANGE_NOT_SATISFIABLE = 416;
constexpr int EXPECTATION_FAILED = 417;
constexpr int IM_A_TEAPOT = 418;
constexpr int MISDIRECTED_REQUEST = 421;
constexpr int UNPROCESSABLE_ENTITY = 422;
constexpr int LOCKED = 423;
constexpr int FAILED_DEPENDENCY = 424;
constexpr int TOO_EARLY = 425;
constexpr int UPGRADE_REQUIRED = 426;
constexpr int PRECONDITION_REQUIRED = 428;
constexpr int TOO_MANY_REQUESTS = 429;
constexpr int REQUEST_HEADER_FIELDS_TOO_LARGE = 431;
constexpr int UNAVAILABLE_FOR_LEGAL_REASONS = 451;

// Server Error 5xx
constexpr int INTERNAL_SERVER_ERROR = 500;
constexpr int NOT_IMPLEMENTED = 501;
constexpr int BAD_GATEWAY = 502;
constexpr int SERVICE_UNAVAILABLE = 503;
constexpr int GATEWAY_TIMEOUT = 504;
constexpr int HTTP_VERSION_NOT_SUPPORTED = 505;
constexpr int VARIANT_ALSO_NEGOTIATES = 506;
constexpr int INSUFFICIENT_STORAGE = 507;
constexpr int LOOP_DETECTED = 508;
constexpr int NOT_EXTENDED = 510;
constexpr int NETWORK_AUTHENTICATION_REQUIRED = 511;

/**
 * @brief Checks if a status code is informational (1xx)
 */
constexpr bool isInformational(const int code)
{
    return code >= CONTINUE && code < OK;
}

/**
 * @brief Checks if a status code is successful (2xx)
 */
constexpr bool isSuccess(const int code)
{
    return code >= OK && code < MULTIPLE_CHOICES;
}

/**
 * @brief Checks if a status code is a redirection (3xx)
 */
constexpr bool isRedirection(const int code)
{
    return code >= MULTIPLE_CHOICES && code < BAD_REQUEST;
}

/**
 * @brief Checks if a status code is a client error (4xx)
 */
constexpr bool isClientError(const int code)
{
    return code >= BAD_REQUEST && code < INTERNAL_SERVER_ERROR;
}

/**
 * @brief Checks if a status code is a server error (5xx)
 */
constexpr bool isServerError(const int code)
{
    return code >= INTERNAL_SERVER_ERROR;
}

/**
 * @brief Checks if a status code indicates an error (4xx or 5xx)
 */
constexpr bool isError(const int code)
{
    return isClientError(code) || isServerError(code);
}

/**
 * @brief Gets the standard reason phrase for a status code
 */
constexpr const char* getReasonPhrase(const int code)
{
    switch (code) {
        // Informational
        case CONTINUE:
            return "Continue";
        case SWITCHING_PROTOCOLS:
            return "Switching Protocols";
        case PROCESSING:
            return "Processing";
        case EARLY_HINTS:
            return "Early Hints";

        // Success
        case OK:
            return "OK";
        case CREATED:
            return "Created";
        case ACCEPTED:
            return "Accepted";
        case NON_AUTHORITATIVE_INFORMATION:
            return "Non-Authoritative Information";
        case NO_CONTENT:
            return "No Content";
        case RESET_CONTENT:
            return "Reset Content";
        case PARTIAL_CONTENT:
            return "Partial Content";
        case MULTI_STATUS:
            return "Multi-Status";
        case ALREADY_REPORTED:
            return "Already Reported";
        case IM_USED:
            return "IM Used";

        // Redirection
        case MULTIPLE_CHOICES:
            return "Multiple Choices";
        case MOVED_PERMANENTLY:
            return "Moved Permanently";
        case FOUND:
            return "Found";
        case SEE_OTHER:
            return "See Other";
        case NOT_MODIFIED:
            return "Not Modified";
        case USE_PROXY:
            return "Use Proxy";
        case TEMPORARY_REDIRECT:
            return "Temporary Redirect";
        case PERMANENT_REDIRECT:
            return "Permanent Redirect";

        // Client Error
        case BAD_REQUEST:
            return "Bad Request";
        case UNAUTHORIZED:
            return "Unauthorized";
        case PAYMENT_REQUIRED:
            return "Payment Required";
        case FORBIDDEN:
            return "Forbidden";
        case NOT_FOUND:
            return "Not Found";
        case METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case NOT_ACCEPTABLE:
            return "Not Acceptable";
        case PROXY_AUTHENTICATION_REQUIRED:
            return "Proxy Authentication Required";
        case REQUEST_TIMEOUT:
            return "Request Timeout";
        case CONFLICT:
            return "Conflict";
        case GONE:
            return "Gone";
        case LENGTH_REQUIRED:
            return "Length Required";
        case PRECONDITION_FAILED:
            return "Precondition Failed";
        case PAYLOAD_TOO_LARGE:
            return "Payload Too Large";
        case URI_TOO_LONG:
            return "URI Too Long";
        case UNSUPPORTED_MEDIA_TYPE:
            return "Unsupported Media Type";
        case RANGE_NOT_SATISFIABLE:
            return "Range Not Satisfiable";
        case EXPECTATION_FAILED:
            return "Expectation Failed";
        case IM_A_TEAPOT:
            return "I'm a teapot";
        case MISDIRECTED_REQUEST:
            return "Misdirected Request";
        case UNPROCESSABLE_ENTITY:
            return "Unprocessable Entity";
        case LOCKED:
            return "Locked";
        case FAILED_DEPENDENCY:
            return "Failed Dependency";
        case TOO_EARLY:
            return "Too Early";
        case UPGRADE_REQUIRED:
            return "Upgrade Required";
        case PRECONDITION_REQUIRED:
            return "Precondition Required";
        case TOO_MANY_REQUESTS:
            return "Too Many Requests";
        case REQUEST_HEADER_FIELDS_TOO_LARGE:
            return "Request Header Fields Too Large";
        case UNAVAILABLE_FOR_LEGAL_REASONS:
            return "Unavailable For Legal Reasons";

        // Server Error
        case INTERNAL_SERVER_ERROR:
            return "Internal Server Error";
        case NOT_IMPLEMENTED:
            return "Not Implemented";
        case BAD_GATEWAY:
            return "Bad Gateway";
        case SERVICE_UNAVAILABLE:
            return "Service Unavailable";
        case GATEWAY_TIMEOUT:
            return "Gateway Timeout";
        case HTTP_VERSION_NOT_SUPPORTED:
            return "HTTP Version Not Supported";
        case VARIANT_ALSO_NEGOTIATES:
            return "Variant Also Negotiates";
        case INSUFFICIENT_STORAGE:
            return "Insufficient Storage";
        case LOOP_DETECTED:
            return "Loop Detected";
        case NOT_EXTENDED:
            return "Not Extended";
        case NETWORK_AUTHENTICATION_REQUIRED:
            return "Network Authentication Required";

        default:
            return "Unknown Status Code";
    }
}

} // namespace HttpCode
#endif // MYAPPLICATION_HTTPCODE_H
