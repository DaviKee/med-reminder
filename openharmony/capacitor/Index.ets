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
import capacitor from "libcapacitor.so"
import { ArkTsAttribute } from './src/main/ets/components/PluginGlobal';
import { UIAbility, Want } from "@kit.AbilityKit";

export { MainPageCycle } from './src/main/ets/components/MainPage'
export { MainPageOnBackPress } from './src/main/ets/components/MainPage'
export { MainPage } from './src/main/ets/components/MainPage'

export { CordovaPlugin, CordovaInterface,CallbackContext,CordovaWebView,PluginEntry,MessageStatus, PluginResult } from './src/main/ets/components/CordovaPlugin'
export { PluginMethod, FunctionPair, CapacitorPlugin } from "./src/main/ets/components/getcapacitor/CapacitorPlugin"
export { PluginCall } from "./src/main/ets/components/getcapacitor/PluginCall"
export { NormalizeError } from "./src/main/ets/components/NormalizeError"

export function RegisterCustomSchemes(customSchemes:string) {
  capacitor.RegisterCustomSchemes(customSchemes);
}

/**
 * @brief 页面显示事件，通知cordova的所有webview，鸿蒙系统限制只能在mainPage的父组件中调用
 */
export function pageShowEvent() {
  let result: ArkTsAttribute = {content:"resume", result:[]};
  capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");
}

/**
 * @brief 页面进入后台，通知cordova的所有webview，鸿蒙系统限制只能在mainPage的父组件中调用
 */
export function pageHideEvent() {
  let result: ArkTsAttribute = {content:"pendingPause", result:[]};
  capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");
}

/**
 * @brief 点击返回，通知cordova的所有webview，鸿蒙系统限制只能在mainPage的父组件中调用
 */
export function pageBackPress() {
  let result: ArkTsAttribute = {content:"overrideBackbutton", result:[]};
  capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");
}

export function postMessage(id:string, value:string) {
  let result: ArkTsAttribute = {content:"postMessage", result:[id, value]};
  capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");
}


export function SetResourceReplace(webTag:string, src:string, obj:string) {
  capacitor.SetResourceReplace(webTag, src, obj);
}

export function SetCordovaProtocolUrl(url:string) {
  capacitor.SetCordovaProtocolUrl(url);//多个url以","分隔，例如：baidu.com,bing.com
}

export function pagePushNotify(data:Record<string, Object>|undefined) {
  if(data) {
    if (data["ohos.aafwk.param.callerBundleName"] == "com.huawei.hms.pushservice") {
      let result: ArkTsAttribute = { content: "receiveMessage", result: ["notification", JSON.stringify(data)] };
      capacitor.onArkTsResult(JSON.stringify(result), "HuaweiPushPlugin", "");
    }
  }
}

export function PluginRegisterHandle(ability:UIAbility, want:Want, module:string, functionName:string, args:string) {
  let m:string = "./src/main/ets/components/"+module;
  import(m).then((ns:ESObject) => {
    ns[functionName](ability, want, args);
  });
}