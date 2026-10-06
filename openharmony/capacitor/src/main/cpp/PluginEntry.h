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

#ifndef PLUGIN_ENTRY_H
#define PLUGIN_ENTRY_H

#include "CordovaPlugin.h"
#include <string>

/**
 * @brief Plugin configuration, dependent on the config.xml file.
 * 举例如下:
 *  <feature name="InAppBrowser">
 *       <param name="android-package" value="org.apache.cordova.inappbrowser.InAppBrowser" />
 *   </feature>
 * Plugin name: InAppBrowser (feature's name)
 * Plugin instantiation object: InAppBrowser (value without the path) (consistent with Android, excluding the path)
 */
struct PluginEntry {
    /**Plugin name*/
    std::string m_service;
    /**Implementation class name.*/
    std::string m_pluginClass;
    /**
     * Whether to instantiate during loading.
     * When instantiated, the plugin's constructor is executed to initialize plugin properties in constructor.
     */
    bool m_onload;

    PluginEntry()
    {
        m_onload = false;
    }

    PluginEntry(const PluginEntry& arg)
    {
        m_service = arg.m_service;
        m_pluginClass = arg.m_pluginClass;
        m_onload = arg.m_onload;
    }

    PluginEntry& operator=(const PluginEntry& arg)
    {
        if (this == &arg) {
            return *this;
        }
        m_service = arg.m_service;
        m_pluginClass = arg.m_pluginClass;
        m_onload = arg.m_onload;
        return *this;
    }

    PluginEntry(std::string service, std::string className, bool onload)
    {
        m_service = service;
        m_pluginClass = className;
        m_onload = onload;
    }
};
#endif