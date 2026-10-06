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

import { ArkTsAttribute,
  cordovaWebTagToObjectGlobe, cordovaWebViewGlobe, NativeAttribute } from '../PluginGlobal';
import { BusinessError } from '@kit.BasicServicesKit';
import capacitor from 'libcapacitor.so';
import { common, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { MainPage } from '../MainPage';
import { CordovaWebView } from '../CordovaWebView';
import { KeyCode } from '@kit.InputKit';
import { inputConsumer } from '@kit.InputKit';
import deviceInfo from '@ohos.deviceInfo';
import { NormalizeError } from '../NormalizeError';


export function LoadUrl(pageIndex:NativeAttribute):void {
  try {
    if (!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    let arg: object = JSON.parse(pageIndex.pageArgs);
    let isOpenExternal: boolean = false;
    let isClearHistory: boolean = false;
    if (arg?.["args"] != undefined) {
      let urlArg: object = arg?.["args"];
      if (urlArg?.["openexternal"] != undefined) {
        isOpenExternal = urlArg?.["openexternal"];
      }
      if (urlArg?.["clearhistory"] != undefined) {
        isClearHistory = urlArg?.["clearhistory"];
      }
    }
    if (isClearHistory) {
      if (!cordovaWebViewGlobe.hasKey(pageIndex.pageWebTag)) {
        return;
      }
      let cordovaWebView: CordovaWebView = cordovaWebViewGlobe.get(pageIndex.pageWebTag);
      cordovaWebView.getWebviewController().clearHistory();
    }

    let strUrl: string = "";
    if (arg?.["url"] != undefined) {
      strUrl = arg?.["url"];
    }

    if (strUrl.startsWith("intent://")) {
      return; //不兼容安卓协议
    }

    //外部浏览器打开
    if (isOpenExternal && strUrl != "" && !strUrl.startsWith("file://")) {
      let context = mainPage.getUIContext().getHostContext() as common.UIAbilityContext
      let wantInfo: Want = {
        action: 'ohos.want.action.viewData',
        entities: ['entity.system.browsable'],
        uri: strUrl
      }
      context.startAbility(wantInfo).then(() => {
        console.log('[跳转至外部浏览器] - success')
      }).catch((err: BusinessError) => {
        console.error('[跳转至外部浏览器] - Failed to startAbility. Code: ' + err.code + 'message:' + err.message);
      })
      return;
    }

    //当前页面打开，兼容file协议
    if (strUrl != "") {
      mainPage.indexPage = arg?.["url"];
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`LoadUrl failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}

export function BackHistory(pageIndex:NativeAttribute):void {
  try {
    if (!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    mainPage.backPress.backPress();
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`BackHistory failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}

//鸿蒙只处理音量的UP和Down键代替web函数onInterceptKeyEvent
export function OverrideButton(pageIndex:NativeAttribute):void {
  try {
    if (!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;

    let keyCodeInfo: object = JSON.parse(pageIndex.pageArgs);
    let keyCodeStr: string = "";
    if (keyCodeInfo?.["button"] != undefined) {
      keyCodeStr = keyCodeInfo?.["button"];
    }

    let keyCode: number = 0;
    if (keyCodeStr == "volumeup") {
      keyCode = KeyCode.KEYCODE_VOLUME_UP;
    } else if (keyCodeStr == "volumedown") {
      keyCode = KeyCode.KEYCODE_VOLUME_DOWN;
    } else {
      return;
    }

    let isOverride: boolean = true;
    if (keyCodeInfo?.["override"] != undefined) {
      isOverride = keyCodeInfo?.["override"];
    }

    const apiVersion = deviceInfo.sdkApiVersion;
    if (apiVersion >= 16) {
      //TODO:按键信息拦截在API14和API16版本中，在API12版本中编译，也能正确编译
      let options: inputConsumer.KeyPressedConfig = {
        key: keyCode,
        action: 1, // 按下按键的行为
        isRepeat: false, // 优先消费掉按键事件，不上报
      }

      if (isOverride) {
         inputConsumer.on('keyPressed', options, mainPage.volumeKeyCallBackFunc);
      } else {
         inputConsumer.off('keyPressed', mainPage.volumeKeyCallBackFunc);
      }
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`OverrideButton failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}


export async function getWindowAvoidArea(context: common.UIAbilityContext): Promise<window.AvoidArea | null> {
  try {
    // The default area of the system includes the status bar and navigation bar
    const mainWindow = await window.getLastWindow(context);
    const avoidAreaType = window.AvoidAreaType.TYPE_SYSTEM;
    const avoidArea = mainWindow.getWindowAvoidArea(avoidAreaType);
    return avoidArea
  } catch (e) {
    return null
  }
}

export async function getWindowNavigationBarHeight(context: common.UIAbilityContext): Promise<number | null> {
  try {
    // The default area of the system includes the status bar and navigation bar
    const mainWindow = await window.getLastWindow(context);
    const avoidAreaTypeNavigationBar = window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR;
    const avoidAreaNavigationBar = mainWindow.getWindowAvoidArea(avoidAreaTypeNavigationBar);
    const heightNavigationBar = avoidAreaNavigationBar.bottomRect.height;
    return heightNavigationBar;
  } catch (e) {
    return null
  }
}

export function ClearCache(pageIndex:NativeAttribute) {
  try {
    if (!cordovaWebViewGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let cordovaWebView: CordovaWebView = cordovaWebViewGlobe.get(pageIndex.pageWebTag);
    if (pageIndex.pageValue == 1) {
      cordovaWebView.getWebviewController().removeCache(true);
    } else {
      cordovaWebView.getWebviewController().removeCache(false);
    }

    let result: ArkTsAttribute = { content: "", result: [] };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`ClearCache failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}

export function ClearHistory(pageIndex:NativeAttribute) {
  try {
    if (!cordovaWebViewGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let cordovaWebView: CordovaWebView = cordovaWebViewGlobe.get(pageIndex.pageWebTag);
    cordovaWebView.getWebviewController().clearHistory();
    let result: ArkTsAttribute = { content: "", result: [] };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`ClearHistory failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}
