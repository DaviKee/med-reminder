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
import { webview } from '@kit.ArkWeb';
import { CordovaWebView } from '../CordovaPlugin';
import { MainPage,MainPageOnBackPress } from '../MainPage';
import { NormalizeError } from '../NormalizeError';
import { ArkTsAttribute, cordovaWebTagToObjectGlobe, cordovaWebViewGlobe, NativeAttribute } from '../PluginGlobal';
import capacitor from 'libcapacitor.so';

let registerPageIndex:NativeAttribute;

let onWebViewBackPress: (webviewController:webview.WebviewController) => boolean = (webviewController:webview.WebviewController) => {
  if(!registerPageIndex) {
    if(webviewController.accessBackward()) {
      webviewController.backward();
      return true;
    }
  } else {
    let result: ArkTsAttribute = {content:"backbutton", result:[webviewController.accessBackward()+""]};
    capacitor.onArkTsResult(JSON.stringify(result), registerPageIndex.pageObject, registerPageIndex.pageWebTag);
  }
  return false;
};

export function RegisterBackbutton(pageIndex:NativeAttribute):void {
  try {
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    let cordovaWebView: CordovaWebView = cordovaWebViewGlobe.get(pageIndex.pageWebTag);
    mainPage.backPress.webviewController = cordovaWebView.getWebviewController();
    mainPage.backPress.onWebViewBackPress = onWebViewBackPress;
    registerPageIndex = pageIndex;
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`Failed to onWatchIndexPageUpdate. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
  }
}