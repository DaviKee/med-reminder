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

#ifndef CORDOVA_HARMONY_SYSTEMCOOKIEMANAGER_H
#define CORDOVA_HARMONY_SYSTEMCOOKIEMANAGER_H

#include <string>
#include <map>

class SystemCookieManager {
    static std::map<std::string, std::string> parseCookie(const std::string& strValue);
    static void resetCookie(const std::string& strUrl, std::string& strCookie, std::string& strCookieUrl);

public:
    static void setCookie(const std::string& strUrl, const std::vector<std::string>& vecCordovaCookie);
    static std::string getCookie(const std::string& strUrl, const std::string& strValue);
};

#endif // CORDOVA_HARMONY_SYSTEMCOOKIEMANAGER_H
