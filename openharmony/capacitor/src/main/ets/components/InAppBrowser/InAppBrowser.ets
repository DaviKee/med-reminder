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

import { MainPage, MainPageCycle, MainPageOnBackPress } from '../MainPage';
import capacitor from 'libcapacitor.so';
import { webview } from '@kit.ArkWeb';
import { BusinessError } from '@kit.BasicServicesKit';
import { HashMap } from '@kit.ArkTS';
import { common } from '@kit.AbilityKit';
import { SymbolGlyphModifier, window } from '@kit.ArkUI';
import { ArkTsAttribute } from '../PluginGlobal';
import { deviceInfo } from '@kit.BasicServicesKit';

interface UrlAttribute {
  url: string;
  title: string;
  target: string;
  args:string;
}

export interface UrlArgs {
  key:string;
  value:string;
}
@Builder
export function PageBrowser() {
  InAppBrowser()
}

class JsObject {
  private webTag:string = "";
  private serviceObject:string = "InAppBrowser";
  constructor(webTag:string, serviceObject:string) {
    this.webTag = webTag;
    this.serviceObject = serviceObject;
  }
  postMessage(data:string) {
    let result: ArkTsAttribute = {content:"message", result:[this.webTag, data]};
    capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.webTag);
  }
  postCallBack(callbackId:string, data:string) {
    let result: ArkTsAttribute = {content:"callback", result:[callbackId, data]};
    capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.webTag);
  }
}

class NavDestinationModifier implements AttributeModifier<NavDestinationAttribute> {
  applyNormalAttribute(instance: NavDestinationAttribute): void {
    if (deviceInfo.sdkApiVersion >= 13) {
      instance.hideToolBar(true);
    }
  }
}

@Entry
@Component
struct InAppBrowser {
  private webId:string = "";
  private parentWebTag:string = "";
  private serviceObject:string = "InAppBrowser";

  public jsObject?:JsObject;
  @State title:string = "";
  @State url:string = "";
  @State isShowProgress:boolean = true;
  @State progressValue: number = 0;

  @State location:boolean = false;
  @State beforeLoad:boolean = true;
  @State cleardata:boolean = false;
  @State closebuttoncaption:string = "";
  @State closeButtonColor:string|Resource = $r('sys.color.ohos_id_color_titlebar_icon');
  @State titleFontAlign:HorizontalAlign = HorizontalAlign.Start;
  @State hidenavigationbuttons:boolean = false;
  @State isLayoutFullScreen:boolean = false;
  @State barBackgroundColor:string|Resource = $r('sys.color.ohos_id_color_navigationbar_bg');
  @State navBarHeight:number|Resource = $r('sys.float.ohos_id_navigation_bar_height');

  @State visibilityTitle:Visibility = Visibility.Visible;
  private closeEventName:string = "";//关闭按钮回调事件名称
  private backEventName:string = "";//返回按钮回调事件名称
  appendUserAgent:string = "";
  pageInfos: NavPathStack = new NavPathStack();
  mainPageOnBackPress:MainPageOnBackPress = new MainPageOnBackPress();
  downDelegate: webview.WebDownloadDelegate = new webview.WebDownloadDelegate();
  mainPageCycle?:MainPageCycle;

  @State modifierNavDest: NavDestinationModifier = new NavDestinationModifier();

  setFullScreen(isLayoutFullScreen:boolean) {
    let context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    let windowStage = context.windowStage;
    let windowClass: window.Window | undefined = undefined;
    windowStage.getMainWindow((err: BusinessError, data) => {
      const errCode: number = err.code;
      if (errCode) {
        console.error(`Failed to obtain the main window. Cause code: ${err.code}, message: ${err.message}`);
        return;
      }
      windowClass = data;
      try {
        if(canIUse("SystemCapability.WindowManager.WindowManager.Core")) {
          windowClass.setWindowLayoutFullScreen(isLayoutFullScreen);
        }
        if(canIUse("SystemCapability.Window.SessionManager")) {
          windowClass.setSpecificSystemBarEnabled('status', !isLayoutFullScreen);
          windowClass.setSpecificSystemBarEnabled('navigation', !isLayoutFullScreen);
          windowClass.setSpecificSystemBarEnabled('navigationIndicator', !isLayoutFullScreen);
        }
      } catch (exception) {
        console.error(`Failed to set the window layout to full-screen mode. Cause code: ${exception.code}, message: ${exception.message}`);
      }
    });
  }

  aboutToAppear() {
    this.mainPageCycle = new MainPageCycle().setOnControllerAttached((webviewController: webview.WebviewController,parentPage?:object)=>{
      let inAppBrowser  = parentPage as InAppBrowser;
      try {
        webviewController.registerJavaScriptProxy(inAppBrowser.jsObject, "cordova_iab", [], ["postMessage", "postCallBack"]);
      } catch (error) {
        console.error(`ErrorCode: ${(error as BusinessError).code},  Message: ${(error as BusinessError).message}`);
      }
      if(inAppBrowser.cleardata) {
        webviewController.removeCache(true);
        webviewController.clearHistory();
        webviewController.clearMatches();
        webviewController.clearSslCache();
      }

      if(inAppBrowser.url.startsWith("file://")) {
        let path:string = inAppBrowser.url.substring(7);
        path = path.substring(0, path.lastIndexOf("/"));
        try {
          webviewController.setPathAllowingUniversalAccess([path]);
        } catch (error) {
          console.error(`ErrorCode: ${(error as BusinessError).code},  Message: ${(error as BusinessError).message}`);
        }
      }

      let userAgent = webviewController.getUserAgent() + " " + this.appendUserAgent;
      webviewController.setCustomUserAgent(userAgent);
    })
    .setOnLoadIntercept((webResourceRequest: WebResourceRequest,parentPage?:object):boolean=>{
      let url:string = webResourceRequest.getRequestUrl();
      let inAppBrowser  = parentPage as InAppBrowser;
      if(url.indexOf("http") != 0 && url.indexOf("file") != 0  && url.indexOf("cdvfile") != 0 && url.match("^[A-Za-z0-9+.-]*://.*?$")) {
        let res: ArkTsAttribute = {content:"customscheme", result:[inAppBrowser.parentWebTag, url]};
        capacitor.onArkTsResult(JSON.stringify(res), this.serviceObject, inAppBrowser.parentWebTag);
      }
      return false;
    })
    .setOnOverrideUrlLoading((webResourceRequest: WebResourceRequest,parentPage?:object):boolean=>{
      let url:string = webResourceRequest.getRequestUrl();
      let inAppBrowser  = parentPage as InAppBrowser;
      let res: ArkTsAttribute = {content:"beforeload", result:[inAppBrowser.parentWebTag, url]};
      capacitor.onArkTsResult(JSON.stringify(res), this.serviceObject, inAppBrowser.parentWebTag);
      return false;
    })
    .setOnPageBegin((url:string,parentPage?:object):void=>{
      let result: ArkTsAttribute = {content:"loadstart", result:[this.parentWebTag]};
      capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.parentWebTag);
    })
    .setOnPageEnd((url:string, webviewController: webview.WebviewController,parentPage?:object):void=>{
      if(parentPage) {
        let page = parentPage as InAppBrowser;
        if(page.title == "") { //如果有内容，使用自定义的
          page.title = webviewController!.getTitle();
        }
      }
      let result: ArkTsAttribute = {content:"loadstop", result:[this.parentWebTag]};
      capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.parentWebTag);
    })
    .setOnProgressChange((newProgress: number,parentPage?:object):void=>{
      if(parentPage) {
        let page = parentPage as InAppBrowser;
        page.progressValue = newProgress;
        if(page.progressValue >= 100) {
          page.isShowProgress = false;
        } else {
          page.isShowProgress = true;
        }
      }
    })
    .setOnRenderExited((renderExitReason:RenderExitReason,parentPage?:object):void=>{
      let result: ArkTsAttribute = {content:"loaderror", result:[this.parentWebTag]};
      capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject,this.parentWebTag);
    })
    .setOnDisAppear((webviewController: webview.WebviewController, parentPage?:object):void=>{
      try {
        webviewController.deleteJavaScriptRegister("cordova_iab");
      } catch (error) {
        console.error(`ErrorCode: ${(error as BusinessError).code},  Message: ${(error as BusinessError).message}`);
      }
      let result: ArkTsAttribute = {content:"exit", result:[this.parentWebTag]};
      capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.parentWebTag);
    });

    this.downDelegate.onBeforeDownload((webDownloadItem: webview.WebDownloadItem) => {
      //传入到js侧的监听函数，有js侧下载
      let url:string = webDownloadItem.getUrl();
      let userAgent:string = "";
      let contentDisposition:string = "";
      let mimetype:string = webDownloadItem.getMimeType();
      let contentLength:string = ""+webDownloadItem.getTotalBytes();
      let res: ArkTsAttribute = {content:"download", result:[this.parentWebTag, url, userAgent, contentDisposition, mimetype, contentLength]};
      capacitor.onArkTsResult(JSON.stringify(res), this.serviceObject, this.parentWebTag);
    });
  }

  aboutToDisappear() {
    if(this.isLayoutFullScreen) {
      this.setFullScreen(false);
    }
  }
  @Builder
  CustomerTitle() {
      Text(this.title).fontColor(this.closeButtonColor).fontSize(20).lineHeight($r('sys.float.ohos_id_navigation_bar_height')).position({ x: '50%'})
        .translate({x:'-50%'}).textAlign(TextAlign.Center).height($r('sys.float.ohos_id_navigation_bar_height'))
  }

  @Builder
  NavigationMenus() {
      if(this.closebuttoncaption == "") {
        SymbolGlyph($r("sys.symbol.xmark")).scale({ x: 1.8, y: 1.8 }).fontColor([this.closeButtonColor]).onClick(()=>{
          this.pageInfos.pop();
        }).margin({top:20,right:20})
      } else {
        Text(this.closebuttoncaption).fontSize(20).margin({top:15,right:10}).fontColor(this.closeButtonColor).onClick(()=>{
          this.pageInfos.pop();
        });
      }
  }

  build() {
    NavDestination () {
      Column() {
        if(!this.location) {
          Row() {//自定义导航条，代替系统的导航条
            Column() {
              SymbolGlyph($r("sys.symbol.chevron_left")).fontColor([this.closeButtonColor]).scale({ x: 2, y: 2 }).onClick(() => {
                if(!this.mainPageOnBackPress.backPress()){
                  this.pageInfos.pop();
                }
                if(this.backEventName != "") {
                  let result: ArkTsAttribute = {content:this.backEventName, result:[this.parentWebTag]};
                  capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.parentWebTag);
                }
              });
            }.width($r('sys.float.titlebar_icon_width')).margin({ left: $r('sys.float.titlebar_title_margin_left') })

            Column() {
              Text(this.title).fontColor(this.closeButtonColor).fontSize($r('sys.float.titlebar_title_tertiary_size')).visibility(this.visibilityTitle)
            }.layoutWeight(1).alignItems(this.titleFontAlign)

            Column() {
              if (this.closebuttoncaption == "") {
                SymbolGlyph($r("sys.symbol.xmark"))
                  .scale({ x: 1.8, y: 1.8 })
                  .fontColor([this.closeButtonColor])
                  .onClick(() => {
                    this.pageInfos.pop();
                    if(this.closeEventName != "") {
                      let result: ArkTsAttribute = {content:this.closeEventName, result:[this.parentWebTag]};
                      capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.parentWebTag);
                    }
                  })
              } else {
                Text(this.closebuttoncaption).fontColor(this.closeButtonColor).onClick(() => {
                  this.pageInfos.pop();
                  if(this.closeEventName != "") {
                    let result: ArkTsAttribute = {content:this.closeEventName, result:[this.parentWebTag]};
                    capacitor.onArkTsResult(JSON.stringify(result), this.serviceObject, this.parentWebTag);
                  }
                });
              }
            }.margin({ right: $r('sys.float.titlebar_title_margin_right') })
          }
          .width('100%')
          .height(this.navBarHeight)
          .backgroundColor(this.barBackgroundColor)
          .alignItems(VerticalAlign.Center)
        }
        Stack({ alignContent: Alignment.Top }) {
          MainPage({
            webId: this.webId,
            indexPage: this.url,
            isNavPath: false,
            backPress: this.mainPageOnBackPress,
            lifeCycle: this.mainPageCycle,
            parentPage: this,
            downDelegate: this.downDelegate
          });
          if (this.isShowProgress) {
            Progress({ value: 0, total: 100, type: ProgressType.Linear })
              .width("100%")
              .height(3)
              .value(this.progressValue)
          }
        }
        .width('100%')
        .layoutWeight(1)
      }.height('100%')
    }
    .menus(this.NavigationMenus())
    .onBackPressed(():boolean =>{
      return this.mainPageOnBackPress.backPress();
		})
    .hideTitleBar(true)
    .attributeModifier(this.modifierNavDest)
    .backButtonIcon(
      new SymbolGlyphModifier($r("sys.symbol.chevron_left")).fontColor([this.closeButtonColor]).scale({ x: 1.5, y: 1.5 }).margin({left:8})
    )
    .title(this.CustomerTitle())
    .onReady((context: NavDestinationContext) => {
      this.pageInfos = context.pathStack;
      let strArrayPara = JSON.stringify(this.pageInfos.getParamByName("InAppBrowser")[0]);
      let arrayPara:Array<string> = JSON.parse(strArrayPara);
      let urlInfo:UrlAttribute = JSON.parse(arrayPara[0]);
      this.webId = arrayPara[1];
      this.parentWebTag = arrayPara[2];
      this.serviceObject = arrayPara[3];
      if(arrayPara.length > 3) {//获取到config.xml的配置，用于设置内置浏览器的配置
        try {
          let cordovaPreferences:object = JSON.parse(arrayPara[3]);
          if(cordovaPreferences?.["NavigationBarBackgroundColor"] !== undefined) {
            this.barBackgroundColor = cordovaPreferences?.["NavigationBarBackgroundColor"];
          }
          if(cordovaPreferences?.["NavigationBarFontColor"] !== undefined) {
            this.closeButtonColor = cordovaPreferences?.["NavigationBarFontColor"];
          }
          if(cordovaPreferences?.["NavigationBarFontAlign"] !== undefined) {
            if(cordovaPreferences?.["NavigationBarFontAlign"] == "left") {
              this.titleFontAlign = HorizontalAlign.Start;
            } else if(cordovaPreferences?.["NavigationBarFontAlign"] == "right") {
              this.titleFontAlign = HorizontalAlign.End;
            } else if(cordovaPreferences?.["NavigationBarFontAlign"] == "center") {
              this.titleFontAlign = HorizontalAlign.Center;
            } else {
              this.titleFontAlign = HorizontalAlign.Start;
            }
          }
          if(cordovaPreferences?.["NavigationBarHeight"] !== undefined) {
            this.navBarHeight = Number(cordovaPreferences?.["NavigationBarHeight"]);
            if(this.navBarHeight == 0) {
              this.location = true;
            }
          }
        } catch (error) {
            console.error("InAppbrowser Failed to parse json");
        }
      }
      this.title = urlInfo.title;
      this.url = urlInfo.url;
      let mapArgs:HashMap<string, string> = new HashMap();
      //参数在args是以key=value数组形式存储
      if(urlInfo.args.startsWith("[")) {
        let arrayArgs: Array<UrlArgs> = JSON.parse(urlInfo.args);
        for (let i = 0; i < arrayArgs.length; i++) {
          let obj: UrlArgs = arrayArgs[i];
          mapArgs.set(obj.key, obj.value);
        }
      }
      //参数在args中是以对象存储
      let objArgs:object = new Object();
      if(urlInfo.args.startsWith("{")) {
        objArgs = JSON.parse(urlInfo.args) as object;
      }

      //处理mapArgs数据
      if(mapArgs.hasKey("location")) {
        if (mapArgs.get("location") == "no") {
          this.location = true;
        } else {
          this.location = false;
        }
      }
      if(mapArgs.hasKey("beforeload")) {
        if (mapArgs.get("beforeload") == "no") {
          this.beforeLoad = false;
        } else {
          this.beforeLoad = true;
        }
      }
      if(mapArgs.hasKey("clearcache")) {
        if (mapArgs.get("clearcache") == "yes") {
          webview.WebCookieManager.clearAllCookiesSync();
        }
      }

      if(mapArgs.hasKey("clearsessioncache")) {
        if (mapArgs.get("clearsessioncache") == "yes") {
          webview.WebCookieManager.clearSessionCookieSync();
        }
      }

      if(mapArgs.hasKey("cleardata")) {
        if (mapArgs.get("cleardata") == "yes") {
          webview.WebCookieManager.clearAllCookiesSync();
          this.cleardata = true;
        }
      }

      if(mapArgs.hasKey("closebuttoncaption")) {
        this.closebuttoncaption = mapArgs.get("closebuttoncaption");
      }

      if(mapArgs.hasKey("closebuttoncolor")) {
        this.closeButtonColor = mapArgs.get("closebuttoncolor");
      }

      if(mapArgs.hasKey("fullscreen")) {
        if (mapArgs.get("fullscreen") == "yes") {
          this.isLayoutFullScreen = true;
          this.setFullScreen(true);
        } else {
          this.isLayoutFullScreen = false;
        }
      }

      if(mapArgs.hasKey("barGroundColor")) {
        this.barBackgroundColor = mapArgs.get("barGroundColor");
      }

      if(mapArgs.hasKey("navbarheight")) {
        this.navBarHeight = Number(mapArgs.get("navbarheight"));
      }

      if(mapArgs.hasKey("appendUserAgent")) {
        this.appendUserAgent = mapArgs.get("appendUserAgent");
      }

      if(objArgs?.["statusbar"] !== undefined) {
        let statusbar:object = objArgs?.["statusbar"];
        if(statusbar?.["color"] !== undefined) {
          let color:string = statusbar?.["color"];
          let context = this.getUIContext().getHostContext() as common.UIAbilityContext;
          let windowStage = context.windowStage;
          let windowClass: window.Window | undefined = undefined;
          windowStage.getMainWindow((err: BusinessError, data) => {
            const errCode: number = err.code;
            if (errCode) {
              console.error(`Failed to obtain the main window. Cause code: ${err.code}, message: ${err.message}`);
              return;
            }
            windowClass = data;
            let SystemBarProperties: window.SystemBarProperties = {
              statusBarColor: color,
            };

            try {
                windowClass.setWindowSystemBarProperties(SystemBarProperties);
            } catch (exception) {
              console.error(`Failed to set the window layout to full-screen mode. Cause code: ${exception.code}, message: ${exception.message}`);
            }
          });
        }
      }

      if(objArgs?.["toolbar"] !== undefined) {
        let toolbar:object = objArgs?.["toolbar"];
        if(toolbar?.["height"] !== undefined) {
          this.navBarHeight = toolbar?.["height"];
        }
        if(toolbar?.["color"] !== undefined) {
          this.barBackgroundColor = toolbar?.["color"];
        }
      }

      if(objArgs?.["title"] !== undefined) {
        let title:object = objArgs?.["title"];
        if(title?.["color"] != undefined) {
          this.closeButtonColor = title?.["color"];
        }
        if(title?.["showPageTitle"] !== undefined) {
          let showPageTitle:boolean = title?.["showPageTitle"];
          if(showPageTitle) {
            this.visibilityTitle = Visibility.Visible;
          } else {
            this.visibilityTitle = Visibility.Hidden;
          }
        }
      }

      if(objArgs?.["closeButton"] !== undefined) {
        let closeButton:object = objArgs?.["closeButton"];
        if(closeButton?.["event"] !== undefined) {
          this.closeEventName = closeButton?.["event"];
        }
      }
      if(objArgs?.["backButton"] !== undefined) {
        let backButton:object = objArgs?.["backButton"];
        if(backButton?.["event"] !== undefined) {
          this.backEventName = backButton?.["event"];
        }
      }
      this.jsObject = new JsObject(this.parentWebTag, this.serviceObject);
    })
  }
}