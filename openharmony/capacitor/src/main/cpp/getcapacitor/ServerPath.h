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

#ifndef MYAPPLICATION_SERVERPATH_H
#define MYAPPLICATION_SERVERPATH_H

/**
 * @brief ServerPath在 Capacitor中没有任何作用，但保留以做热更新功能的更新说明
 * 1，Capacitor/Cordova封装的Web组件mainPage已具备原其他移动端的ServerPath的功能
 * 2，mainPage可以自己设定web的加载路径，该路径可以是rawfile、resfile和任何沙箱路径
 * 3，热更新目录Application::g_strHotCodeUpdateDirectory（可以自定义修改），下载h5资源后存放在此目录下，capacitor或cordova自动加载热更新目录资源
 * 4，在生产环境下，热更新index.html(首页）Capacitor需要在下载热更新资源后，调用Bridge::AutoInjectCordovaJs在index.html注入js，Cordova无需注入
 * 特别说明：热更新应用市场并不推荐，但可以用于解决bug，建议更新App采用应用市场上架流程
 */
class ServerPath {
public:
    ServerPath() = default;
    ~ServerPath() = default;
};

#endif // MYAPPLICATION_SERVERPATH_H
