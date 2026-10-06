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

#include "CordovaPreferences.h"

void CordovaPreferences::setPreferencesBunlde(std::string extras)
{
    m_preferencesBundleExtras = extras;
}

void CordovaPreferences::set(const std::string& name, const std::string& value)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_prefs[name] = value;
}

std::map<std::string, std::string>& CordovaPreferences::getAll()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_prefs;
}

bool CordovaPreferences::getBoolean(std::string name, bool defaultValue)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_prefs.find(name) != m_prefs.end()) {
        std::string strValue = m_prefs[name];
        if (strValue == "true") {
            defaultValue = true;
        } else {
            defaultValue = false;
        }
    }
    return defaultValue;
}

bool CordovaPreferences::contains(std::string name)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_prefs.find(name) != m_prefs.end();
}

int CordovaPreferences::getInteger(std::string name, int defaultValue)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_prefs.find(name) != m_prefs.end()) {
        std::string strValue = m_prefs[name];
        defaultValue = atoi(strValue.c_str());
    }
    return defaultValue;
}

double CordovaPreferences::getDouble(std::string name, double defaultValue)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_prefs.find(name) != m_prefs.end()) {
        std::string strValue = m_prefs[name];
        defaultValue = atof(strValue.c_str());
    }
    return defaultValue;
}

std::string CordovaPreferences::getString(std::string name, std::string defaultValue)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_prefs.find(name) != m_prefs.end()) {
        defaultValue = m_prefs[name];
    }
    return defaultValue;
}
