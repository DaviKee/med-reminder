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

#ifndef CORDOVA_PREFERENCES_H
#define CORDOVA_PREFERENCES_H

#include "XMLParser.h"

#include <mutex>
#include <string>
#include <map>

/**
 * @brief Preferences configuration from config.xml.
 * Only one object is defined on the C++ side.
 */
class CordovaPreferences {
    std::mutex m_mutex;
    std::map<std::string, std::string> m_prefs;
    std::string m_preferencesBundleExtras;
    /** Copying and assignment are disabled */
    CordovaPreferences(const CordovaPreferences&);
    CordovaPreferences& operator=(const CordovaPreferences&);

public:
    CordovaPreferences() {}

    ~CordovaPreferences() {}

    void setPreferencesBunlde(std::string extras);
    void set(const std::string& name, const std::string& value);
    std::map<std::string, std::string>& getAll();
    bool getBoolean(std::string name, bool defaultValue);
    bool contains(std::string name);
    int getInteger(std::string name, int defaultValue);
    double getDouble(std::string name, double defaultValue);
    std::string getString(std::string name, std::string defaultValue);
};
#endif