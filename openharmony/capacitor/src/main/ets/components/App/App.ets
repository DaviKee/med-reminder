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

import capacitor from 'libcapacitor.so';
import { cordovaWebTagToObjectGlobe, cordovaWebViewGlobe, NativeAttribute } from '../PluginGlobal';
import { ApplicationStateChangeCallback, common, UIAbility, Want } from '@kit.AbilityKit';
import bundleManager from '@ohos.bundle.bundleManager';
import { BusinessError } from '@kit.BasicServicesKit'
import { MainPage, MainPageOnBackPress } from '../MainPage';
import { CordovaWebView } from '../CordovaPlugin';
import { NormalizeError } from '../NormalizeError';
import { webview } from '@kit.ArkWeb';

let m_pageIndex: NativeAttribute | null
let registerPageIndex: NativeAttribute
let backPress: MainPageOnBackPress = new MainPageOnBackPress();
let toggleBackButton: boolean = true
let registerBackButton: boolean = false
let onWebViewBackPress: (webviewController: webview.WebviewController) => boolean

interface BackButtonListenerEvent {
  canGoBack: boolean;
}

interface AppInfo {
  name: string;
  id: string;
  version: string;
  build: string;
}

interface AppState {
  isActive: boolean;
}

interface AppLaunchUrl {
  url: string | undefined;
}

export function load(pageIndex: NativeAttribute) {
  try {
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage
    let configObj = mainPage.getJsonObjectFromRawFileSync('capacitor.config.json')
    const plugins = (configObj as Record<string, object>).plugins
    const App = plugins ? (plugins as Record<string, object>).App : undefined;
    if (App === undefined || App === null || (Object.keys(App).length == 0)) {
      return;
    }
    toggleBackButton = (App as Record<string, boolean>).disableBackButtonHandler
    toggleBackButtonHandler(pageIndex, toggleBackButton)
  } catch (error) {
    const err = error as BusinessError
  }
}

export function getOnWebViewBackPress() {
  if (!onWebViewBackPress) {
    onWebViewBackPress = (webviewController: webview.WebviewController) => {
      if (registerBackButton) {
        capacitor.onArkTsResult(JSON.stringify({
          notify: "backButton", content: ({
            canGoBack: webviewController.accessBackward()
          } as BackButtonListenerEvent)
        }), registerPageIndex.pageObject, registerPageIndex.pageWebTag);
      }
      if (toggleBackButton) {
        if (webviewController.accessBackward()) {
          webviewController.backward();
          return true;
        }
      } else {
        return false
      }
      return false;
    };
  }
  return onWebViewBackPress
}

/**
 * Force exit the app.
 * @param pageIndex
 */
export function exitApp(pageIndex: NativeAttribute) {
  let context = getContext() as common.UIAbilityContext;
  context.terminateSelf().catch((error: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  })
  capacitor.onArkTsResult(JSON.stringify({ result: "" }), pageIndex.pageObject, pageIndex.pageWebTag);
}

/**
 * Return information about the app.
 * @param pageIndex
 */
export async function getInfo(pageIndex: NativeAttribute) {
  try {
    let bundleInfo =
      await bundleManager.getBundleInfoForSelf(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION);
    let appResource = bundleInfo.appInfo.labelResource
    let context = getContext() as common.UIAbilityContext;
    let appName = await context.resourceManager.getStringValue(appResource)
    let bundleName = bundleInfo.name
    let versionName = bundleInfo.versionName;
    let versionCode = bundleInfo.versionCode;
    capacitor.onArkTsResult(JSON.stringify({
      result: "success", content: ({
        name: appName,
        id: bundleName,
        version: versionName,
        build: versionCode + ""
      } as AppInfo)
    }), pageIndex.pageObject, pageIndex.pageWebTag);
  } catch (e) {
    const error = e as BusinessError
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }
}

/**
 * Gets the current app state.
 * @param pageIndex
 */
export async function getState(pageIndex: NativeAttribute) {
  const bundleName = pageIndex.pageArgs
  let context = getContext() as common.UIAbilityContext;
  context.getApplicationContext().getRunningProcessInformation().catch((error: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }).then((array) => {
    if (array) {
      for (let index = 0; index < array.length; index++) {
        const element = array[index];
        if (bundleName === element.processName) {
          if (element.state === 1 || element.state === 2) {
            capacitor.onArkTsResult(JSON.stringify({
              result: "success", content: ({
                isActive: true
              } as AppState)
            }), pageIndex.pageObject, pageIndex.pageWebTag);
          } else {
            capacitor.onArkTsResult(JSON.stringify({ result: "success", content: { isActive: false } as AppState }),
              pageIndex.pageObject, pageIndex.pageWebTag);
          }
        }
      }
    }
  })
}

/**
 * Get the URL the app was launched with, if any.
 * @param ability
 * @param want
 * @param appId
 */
export async function getLaunchUrl(ability: UIAbility, want: Want, appId: string) {
  if (m_pageIndex !== null) {
    capacitor.onArkTsResult(JSON.stringify({
      result: "success", content: ({
        url: want.uri
      } as AppLaunchUrl)
    }), m_pageIndex.pageObject, m_pageIndex.pageWebTag);
  }
}

/**
 * Minimizes the application.
 * @param pageIndex
 */
export function AppMinimize(pageIndex: NativeAttribute) {
  let context = getContext() as common.UIAbilityContext;
  let windowStage = context.windowStage;
  windowStage.getMainWindow().then((win) => {
    win.minimize().catch((err: BusinessError) => {
      capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
        pageIndex.pageWebTag);
    })
    capacitor.onArkTsResult(JSON.stringify({ result: "" }), pageIndex.pageObject, pageIndex.pageWebTag);
  }).catch((err: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  })
}

/**
 * Enables or disables the plugin's back button handling during runtime.
 * @param pageIndex
 */
export async function toggleBackButtonHandler(pageIndex: NativeAttribute, initToggleBack?: boolean) {
  addBackButtonListener(pageIndex)
  let returnResult: boolean = true
  if (initToggleBack !== undefined) {
    toggleBackButton = initToggleBack
    returnResult = false
  } else {
    if (pageIndex.pageValue == 0) {
      toggleBackButton = false
    } else {
      toggleBackButton = true
    }
  }
  if (returnResult) {
    capacitor.onArkTsResult(JSON.stringify({ result: "" }), registerPageIndex.pageObject, registerPageIndex.pageWebTag);
  }
}

/**
 * Listen for changes in the app or the activity states.
 * @param pageIndex
 */
export async function appStateChange(pageIndex: NativeAttribute) {
  let applicationStateChangeCallback: ApplicationStateChangeCallback = {
    onApplicationForeground() {
      console.info('applicationStateChangeCallback onApplicationForeground');
      capacitor.onArkTsResult(JSON.stringify({
        notify: "appStateChange", content: ({
          isActive: true
        } as AppState)
      }), pageIndex.pageObject, pageIndex.pageWebTag);
    },
    onApplicationBackground() {
      console.info('applicationStateChangeCallback onApplicationBackground');
      capacitor.onArkTsResult(JSON.stringify({ notify: "appStateChange", content: { isActive: false } as AppState }),
        pageIndex.pageObject, pageIndex.pageWebTag);
    }
  };

  let applicationContext = getContext().getApplicationContext();
  try {
    // 2.通过applicationContext注册当前进程状态监听
    if (applicationContext != undefined) {
      applicationContext.on('applicationStateChange', applicationStateChangeCallback);
    }
  } catch (paramError) {
    console.error(`error: ${(paramError as BusinessError).code}, ${(paramError as BusinessError).message}`);
  }
}

/**
 * 获取启动app 的url 后进行缓存
 * @param pageIndex
 */
export async function setLaunchUrl(pageIndex: NativeAttribute) {
  m_pageIndex = pageIndex
}

/**
 * 添加back按钮的监听
 * @param pageIndex
 */
export function RegisterBackButton(pageIndex: NativeAttribute): void {
  addBackButtonListener(pageIndex)
  registerBackButton = true
}

function addBackButtonListener(pageIndex: NativeAttribute): void {
  try {
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    let cordovaWebView: CordovaWebView = cordovaWebViewGlobe.get(pageIndex.pageWebTag);
    mainPage.backPress.webviewController = cordovaWebView.getWebviewController();
    mainPage.backPress.onWebViewBackPress = getOnWebViewBackPress();
    mainPage.backPress = backPress;
    registerPageIndex = pageIndex;
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`Failed to onWatchIndexPageUpdate. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
  }
}

export function removeAllListeners(pageIndex: NativeAttribute): void {
  registerBackButton = false
  let applicationContext = getContext().getApplicationContext();
  applicationContext.off('applicationStateChange')
  m_pageIndex = null
}