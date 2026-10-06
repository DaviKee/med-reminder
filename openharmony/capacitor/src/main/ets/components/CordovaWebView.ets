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

import { webview } from "@kit.ArkWeb";

export  class CordovaWebView {
  private webTag:string|undefined;
  private pageStack: NavPathStack|undefined;
  private webAttribute:WebAttribute|undefined;
  private webviewController: webview.WebviewController;
  private uiContext:UIContext;
  private isShow:boolean;

  constructor(webTag:string, webAttribute:WebAttribute|undefined, webviewController: webview.WebviewController,  uiContext:UIContext, pageStack?: NavPathStack){
    this.webTag = webTag;
    this.webAttribute = webAttribute;
    this.webviewController = webviewController;
    this.pageStack = pageStack;
    this.uiContext = uiContext;
    this.isShow = true;
  }

  setWebAttribute(webAttribute:WebAttribute|undefined):void {
    this.webAttribute = webAttribute;
  }

  setIsShow(isShow:boolean) {
    this.isShow = isShow;
  }
  getUIContext():UIContext {
    return this.uiContext;
  }
  getWebviewController():webview.WebviewController {
    return this.webviewController;
  }
  getWebAttribute():WebAttribute|undefined {
    return this.webAttribute;
  }
  getPageStack():NavPathStack|undefined {
    return this.pageStack;
  }
  getWebTag():string|undefined {
    return this.webTag;
  }
  getIsShow():boolean {
    return this.isShow;
  }
}