/*
 * The MIT License (MIT)
 * Copyright (C) 2026 Huawei Device Co., Ltd and iSoftStone Information Technology(Group)Co.,Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#include "PluginHandle.h"
#include "TsCapacitorPlugin.h"

PluginHandle::PluginHandle(const std::string &strClassName, Plugin *plugin) : m_pPlugin(plugin)
{
    m_strClassName = strClassName;
    m_pPlugin->load();
}

void PluginHandle::invoke(const std::string &strMethodName, PluginCall &call)
{
    if (m_strClassName == "TsCapacitorPlugin") {
        TsCapacitorPlugin *pTsCapacitorPlgin = dynamic_cast<TsCapacitorPlugin *>(m_pPlugin);
        pTsCapacitorPlgin->callMethod(call);
        return;
    }
    CCapacitorPluginMethod::invoke(m_pPlugin, m_strClassName, strMethodName, call);
}

Plugin *PluginHandle::getInstance() { return m_pPlugin; }

void PluginHandle::addListener(PluginCall &call) const { m_pPlugin->addListener(call); }

void PluginHandle::removeListener(PluginCall &call) const { m_pPlugin->removeListener(call); }
void PluginHandle::removeAllListeners(PluginCall &call) const { m_pPlugin->removeAllListeners(call); }

void PluginHandle::checkPermissions(PluginCall &call) const { m_pPlugin->checkPermissions(call); }

void PluginHandle::requestPermissions(PluginCall &call) const { m_pPlugin->requestPermissions(call); }