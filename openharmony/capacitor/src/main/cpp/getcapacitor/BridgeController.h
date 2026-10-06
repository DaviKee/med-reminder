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

#ifndef MyApplication_BRIDGECONTROLLER_H
#define MyApplication_BRIDGECONTROLLER_H

#include "Bridge.h"

class BridgeController {
    BridgeController(const BridgeController &);
    BridgeController &operator=(const BridgeController &);

protected:
    Bridge::Builder *m_bridgeBuilder{nullptr};
    Bridge *m_pBridge{nullptr};
    CapConfig *m_pCapConfig;

public:
    BridgeController(const std::string &strWebTag, Capacitor::PluginManager *pPluginManager,
                     MessageQueue *pMessageQueue);
    ~BridgeController() = default;

    Bridge *getBridge();
};

#endif // MyApplication_BRIDGECONTROLLER_H
