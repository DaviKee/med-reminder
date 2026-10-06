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

import capacitor from 'libcapacitor.so';
import { webview } from '@kit.ArkWeb';
import { BusinessError} from '@kit.BasicServicesKit';
import { AttributeUpdater, display, window } from '@kit.ArkUI';
import { util, HashMap, JSON } from '@kit.ArkTS';
import { CordovaInterface} from "./CordovaInterface"
import { CordovaWebView } from './CordovaWebView';
import { PluginEntry } from './CordovaPlugin';
import {
  cordovaWebIdToWebviewGlobe,
  cordovaWebTagToObjectGlobe, NativeAttribute} from './PluginGlobal'
import {ArkTsAttribute} from './PluginGlobal'

import {InitArkTsPlugins, NotifyArkTsPluginDestroy, removeWebAndPlugins } from './PluginAction/PluginAction';

import { PluginHandler } from './PluginHandler';
import { CordovaPreferences } from './CordovaPreferences';
import { call, observer } from '@kit.TelephonyKit';
import { KeyEvent, KeyCode } from '@kit.InputKit';
import {
  ClearSplashFullScreen,
  ClearSplashScreenImmersive,
  SetSplashScreenFullScreen,
  SetSplashScreenImmersive,
  SplashScreenNodeController } from './SplashScreen/SplashScreen';
import { ShowPromptDialog } from './PluginAction/PromptActionClass';
import { common } from '@kit.AbilityKit';
import { NormalizeError } from './NormalizeError';

export class WebAttributeModifier extends AttributeUpdater<WebAttribute>{
  // 首次绑定时触发initializeModifier方法，进行属性初始化
  initializeModifier(instance: WebAttribute): void {
  }
}

export class MainPageCycle {
  private aboutToAppear?: (webviewController: webview.WebviewController,parentPage?:object) => void;
  private onControllerAttached?: (webviewController: webview.WebviewController,parentPage?:object) => void;
  private onLoadIntercept?:(webResourceRequest: WebResourceRequest,parentPage?:object)=>boolean;
  private onOverrideUrlLoading?:(webResourceRequest: WebResourceRequest,parentPage?:object)=>boolean;
  private onInterceptRequest?:(request: WebResourceRequest, webTag:string,parentPage?:object) => WebResourceResponse|null;

  private onPageBegin?:(url:string,parentPage?:object)=>void;
  private onProgressChange?:(newProgress:number,parentPage?:object)=>void;
  private onPageEnd?:(url:string,webviewController: webview.WebviewController,parentPage?:object)=>void;
  private onPageVisible?:(url:string,parentPage?:object)=>void;
  private onRenderExited?:(renderExitReason:RenderExitReason,parentPage?:object)=>void;

  private onDisAppear?:(webviewController: webview.WebviewController,parentPage?:object)=>void;

  private onSslErrorEventReceive?:(event:OnSslErrorEventReceiveEvent, webviewController: webview.WebviewController,parentPage?:object)=>void;
  private onSslErrorEvent?:(event: SslErrorEvent,  webviewController: webview.WebviewController,parentPage?:object)=>void;
  private onClientAuthenticationRequest?:(event:OnClientAuthenticationEvent,  url:string,  webviewController: webview.WebviewController,parentPage?:object)=>void;
  private onErrorReceive?:(event:OnErrorReceiveEvent,webviewController: webview.WebviewController,parentPage?:object)=>void;

  public webviewController?: webview.WebviewController;

  setOnAboutToAppear(aboutToAppear?: (webviewController: webview.WebviewController,parentPage?:object) => void):MainPageCycle {
    this.aboutToAppear = aboutToAppear;
    return this;
  }

  setOnControllerAttached(onControllerAttached?: (webviewController: webview.WebviewController,parentPage?:object) => void):MainPageCycle {
    this.onControllerAttached = onControllerAttached;
    return this;
  }

  setOnLoadIntercept(onLoadIntercept?: (webResourceRequest: WebResourceRequest,parentPage?:object) => boolean):MainPageCycle {
    this.onLoadIntercept = onLoadIntercept;
    return this;
  }
  setOnOverrideUrlLoading(onOverrideUrlLoading?: (webResourceRequest: WebResourceRequest,parentPage?:object) => boolean):MainPageCycle {
    this.onOverrideUrlLoading = onOverrideUrlLoading;
    return this;
  }
  setOnInterceptRequest(onInterceptRequest?: (request: WebResourceRequest, webTag:string,parentPage?:object) => WebResourceResponse|null):MainPageCycle {
    this.onInterceptRequest = onInterceptRequest;
    return this;
  }

  setOnPageBegin(onPageBegin?: (url: string,parentPage?:object) => void):MainPageCycle {
    this.onPageBegin = onPageBegin;
    return this;
  }

  setOnProgressChange(onProgressChange?: (newProgress: number,parentPage?:object) => void):MainPageCycle {
    this.onProgressChange = onProgressChange;
    return this;
  }

  setOnPageEnd(onPageEnd?: (url: string,webviewController: webview.WebviewController,parentPage?:object) => void):MainPageCycle {
    this.onPageEnd = onPageEnd;
    return this;
  }


  setOnPageVisible(onPageVisible?: (url: string,parentPage?:object) => void):MainPageCycle {
    this.onPageVisible = onPageVisible;
    return this;
  }

  setOnRenderExited(onRenderExited?: (renderExitReason:RenderExitReason,parentPage?:object) => void):MainPageCycle {
    this.onRenderExited = onRenderExited;
    return this;
  }

  setOnDisAppear(onDisAppear?:(webviewController: webview.WebviewController, parentPage?:object) => void):MainPageCycle {
    this.onDisAppear = onDisAppear;
    return this;
  }

  setOnSslErrorEventReceive(onSslErrorEventReceive?:(event:OnSslErrorEventReceiveEvent, webviewController: webview.WebviewController,parentPage?:object) => void):MainPageCycle {
    this.onSslErrorEventReceive = onSslErrorEventReceive;
    return this;
  }

  setOnSslErrorEvent(onSslErrorEvent?:(event: SslErrorEvent,  webviewController: webview.WebviewController,parentPage?:object) =>void ):MainPageCycle {
    this.onSslErrorEvent = onSslErrorEvent;
    return this;
  }

  setOnClientAuthenticationRequest(onClientAuthenticationRequest?:(event:OnClientAuthenticationEvent,  url:string,  webviewController: webview.WebviewController,parentPage?:object) => void):MainPageCycle {
    this.onClientAuthenticationRequest = onClientAuthenticationRequest;
    return this;
  }

  setOnErrorReceive(onErrorReceive?:(event:OnErrorReceiveEvent,webviewController: webview.WebviewController,parentPage?:object)=>void):MainPageCycle {
    this.onErrorReceive = onErrorReceive;
    return this;
  }

  execOnAboutToAppear(webviewController: webview.WebviewController,parentPage?:object) {
    if(this.aboutToAppear) {
      this.aboutToAppear(webviewController,parentPage);
    }
  }

  execOnControllerAttached(webviewController: webview.WebviewController,parentPage?:object) {
    if(this.onControllerAttached) {
      this.onControllerAttached(webviewController,parentPage);
    }
  }

  execOnLoadIntercept(webResourceRequest: WebResourceRequest,parentPage?:object):boolean {
    if(this.onLoadIntercept) {
      return this.onLoadIntercept(webResourceRequest,parentPage);
    }
    return false;
  }

  execOnOverrideUrlLoading(webResourceRequest: WebResourceRequest,parentPage?:object):boolean {
    if(this.onOverrideUrlLoading) {
      return this.onOverrideUrlLoading(webResourceRequest,parentPage);
    }
    return false;
  }

  execOnInterceptRequest(request: WebResourceRequest, webTag:string,parentPage?:object):WebResourceResponse|null {
    if(this.onInterceptRequest) {
      return this.onInterceptRequest(request,webTag,parentPage);
    }
    return null;
  }

  execOnPageBegin(url: string,parentPage?:object):void {
    if(this.onPageBegin) {
      this.onPageBegin(url,parentPage);
    }
  }

  execOnProgressChange(newProgress: number,parentPage?:object):void {
    if(this.onProgressChange) {
      this.onProgressChange(newProgress,parentPage);
    }
  }

  execOnPageEnd(url: string, webviewController: webview.WebviewController,parentPage?:object):void {
    if(this.onPageEnd) {
      this.onPageEnd(url,webviewController,parentPage);
    }
  }

  execOnPageVisible(url: string,parentPage?:object):void {
    if(this.onPageVisible) {
      this.onPageVisible(url,parentPage);
    }
  }

  execOnRenderExited(renderExitReason:RenderExitReason,parentPage?:object):void {
    if(this.onRenderExited) {
      this.onRenderExited(renderExitReason,parentPage);
    }
  }

  execOnDisAppear(webviewController: webview.WebviewController,parentPage?:object):void {
    if(this.onDisAppear) {
      this.onDisAppear(webviewController, parentPage);
    }
  }

  execOnSslErrorEventReceive(event:OnSslErrorEventReceiveEvent, webviewController: webview.WebviewController, parentPage?:object):void {
    if(this.onSslErrorEventReceive) {
      this.onSslErrorEventReceive(event, webviewController, parentPage);
    } else {
      event.handler.handleConfirm();
    }
  }

  execOnSslErrorEvent(event: SslErrorEvent,  webviewController: webview.WebviewController,parentPage?:object):void {
    if(this.onSslErrorEvent) {
      this.onSslErrorEvent(event, webviewController, parentPage);
    } else {
      event.handler.handleConfirm();
    }
  }

  execOnClientAuthenticationRequest(event:OnClientAuthenticationEvent,  url:string,  webviewController: webview.WebviewController,parentPage?:object):void {
    if(this.onClientAuthenticationRequest) {
      this.onClientAuthenticationRequest(event, url, webviewController, parentPage);
    } else {
      event.handler.confirm(url);
    }
  }

  execOnErrorReceive(event:OnErrorReceiveEvent,webviewController: webview.WebviewController,parentPage?:object):void {
    if(this.onErrorReceive) {
      this.onErrorReceive(event, webviewController, parentPage);
    }
  }
}

export class MainPageOnBackPress {
  public webviewController?: webview.WebviewController;
  public onWebViewBackPress?: (webviewController:webview.WebviewController) => boolean;
  backPress:() => boolean = ():boolean => {
    if(this.onWebViewBackPress && this.webviewController) {
      return this.onWebViewBackPress(this.webviewController);
    }
    return false;
  }
}

@Component
export struct MainPage {
  private webTag: string = 'scheme-handler'+"-"+util.generateRandomUUID(false);
  private strTmpUrl:string = 'www.example.com';
  private src:string = "https://" + this.strTmpUrl + "/index.html";
  public webId:string = "";//父组件设定的webId，如果要用到多个webview之间通讯，父组件需要设定子组件ID
  public isWebDebug: boolean = false;
  public cordovaPlugs: Array<PluginEntry> = [];
  public capacitorPlugins:Array<PluginEntry> = [];
  @Prop @Watch('onWatchIndexPageUpdate') public indexPage:string = "";
  public basePath:string = "";
  public customSchemes:string = "";
  private webAttributeModifier: WebAttributeModifier = new WebAttributeModifier();
  public pageInfos: NavPathStack = new NavPathStack();
  public isNavPath:boolean = true;
  public parentPage?:object;
  private cordovaPreferences:CordovaPreferences = new CordovaPreferences();
  private cordovaInterface:CordovaInterface = new CordovaInterface(this.pageInfos);
  private webviewController: webview.WebviewController = new webview.WebviewController(this.webTag);
  public downDelegate: webview.WebDownloadDelegate = new webview.WebDownloadDelegate();
  private cordovaWebView:CordovaWebView = new CordovaWebView(this.webTag, this.webAttributeModifier.attribute, this.webviewController, this.getUIContext(), this.pageInfos);
  public lifeCycle:MainPageCycle = new MainPageCycle();
  public backPress:MainPageOnBackPress = new MainPageOnBackPress();

  public inAppBrowserWebId:string = ""; //打开内置浏览器的webId，一个mainPage只能打开一个内置浏览器全屏显示在mainPage之上
  public cookies:HashMap<string, Array<string>> = new HashMap<string, Array<string>>();
  public isInjectBridgeJs = true;
  private scriptItemArray = new Array<ScriptItem>();

  @Prop public textZoomRatio:number = 100;
  public customHttpHeaders:string = "";
  public isAllowCredentials:boolean = false;
  public webKeyboardAvoidMode:WebKeyboardAvoidMode = WebKeyboardAvoidMode.RESIZE_VISUAL;
  public nativeEmbedHtmlTag:string = "embed";
  public nativeEmbedHtmlType:string = "native";

  @State title:string = "";//标题
  @State hideTitleBar:boolean = true;
  @State barBackgroundColor:string|Resource|undefined = $r('sys.color.ohos_id_color_navigationbar_bg');
  @State titleFontColor:string|Resource|undefined = $r('sys.color.ohos_id_color_titlebar_text');
  @State titleFontAlign:FlexAlign = FlexAlign.Start;

  //闪屏作为web组件的一部分
  public isFullScreenAfterCloseSplash:boolean = false;//闪屏结束后，是否还要继续全屏
  public SystemBarPropertiesAfterCloseSplash?:window.SystemBarProperties;//闪屏关闭后状态栏状态设置
  @State isSplashScreen:boolean = false;//是否显示闪屏
  public SplashResourcePortrait:string = "splash_portrait";//刷屏闪屏资源图片
  public SplashResourceLandscape:string = "splash_landscape";//横屏闪屏资源图片
  @State AndroidSplashResourceName:string = "splash";
  @State AutoHideSplashScreen:boolean = true;//是否自动隐藏
  @State SplashScreenDelay:number = 500; //0默认禁用闪屏,显示持续时间（在web初始化完后，显示的时间）
  @State FadeOutDuration:number = 200; //配置淡出时间 会覆盖传入参数
  @State FadeSplashScreenDuration:number = 1000;//动画持续时间
  @State SplashScreenBackgroundColor:string = "#FFFFFF";
  @State AndroidScaleType:string = "FIT_XY";
  @State AndroidSpinnerStyle:string = "large";
  @State IosSpinnerStyle:string = "large";
  @State SplashFullScreen:boolean = false;
  @State SplashImmersive:boolean = false;
  @State LayoutName:string = "";
  @State UseDialog:boolean = false;
  @State ShowSplashScreenSpinner:boolean = true;//是否显示Spinner
  @State SplashScreenSpinnerColor:string = "#ffffff";//Spinner颜色
  @State SplashMaintainAspectRatio:boolean = true;//是否全屏
  @State SplashShowOnlyFirstTime:boolean = true;//闪屏第一次显示
  @State splashScreenOpacityValue:number = 1; //触发闪屏消失动画
  @State rotationSplash: number = 0;//横屏/竖屏闪屏切换标记，使用同一个结点会出现屏幕抖动，因此初始化两个结点
  private splashScreenPortrait?:SplashScreenNodeController; //竖屏闪屏结点，在cordova初始化完后，再初始化
  private splashScreenLandscape?:SplashScreenNodeController; //横屏闪屏结点，在cordova初始化完后，再初始化
  private isCordova: boolean = false;
  private isStartShow: boolean = false;

  private onWatchIndexPageUpdate() {
    try {
      if(this.indexPage.indexOf("http") == 0) {
        if(this.src != this.indexPage) {
          this.src = this.indexPage;
          this.webviewController.loadUrl(this.src);
        }
      } else {
        let src = "https://" + this.strTmpUrl + this.indexPage;
        if(src != this.src) {
          this.src = "https://" + this.strTmpUrl + this.indexPage;
          this.webviewController.loadUrl(this.src);
        }
      }
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to onWatchIndexPageUpdate. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  onSetCordovaWebAttribute: (cordovaWebView:CordovaWebView) => void = (cordovaWebView:CordovaWebView) => {
  };

  onInterceptWebRequest: (request: WebResourceRequest, webTag:string, parentPage?:object) => WebResourceResponse|null = (request: WebResourceRequest, webTag:string) => {
    return this.lifeCycle.execOnInterceptRequest(request,webTag, this.parentPage);
  };

  onNativeEmbedLifecycleChange:(event: NativeEmbedDataInfo,cordovaWebView:CordovaWebView,parentPage?:object)=>void=(event: NativeEmbedDataInfo,cordovaWebView:CordovaWebView,parentPage?:object)=>{
  };

  onNativeEmbedGestureEvent:(event: NativeEmbedTouchInfo,cordovaWebView:CordovaWebView,parentPage?:object)=>void=(event: NativeEmbedTouchInfo,cordovaWebView:CordovaWebView,parentPage?:object)=>{
  };

  onPermissionRequest:(event:OnPermissionRequestEvent,parentPage?:object)=>void=(event:OnPermissionRequestEvent,parentPage?:object)=>{
  }
  private onWebViewBackPress: (webviewController:webview.WebviewController) => boolean = (webviewController:webview.WebviewController) => {
    if(webviewController.accessBackward()) {
      webviewController.backward();
      return true;
    }
    return false;
  };

  //拦截音量键有插件自己处理
  volumeKeyCallBackFunc: (event:KeyEvent) => void = (data:KeyEvent) => {
    if(data.key.code == KeyCode.KEYCODE_VOLUME_UP) {
      let result: ArkTsAttribute = {content:"volumeupbutton", result:[]};
      capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", this.webTag);
    }
    if(data.key.code == KeyCode.KEYCODE_VOLUME_DOWN) {
      let result: ArkTsAttribute = { content: "volumedownbutton", result: [] };
      capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", this.webTag);
    }
  }

  aboutToAppear() {
    //横屏竖屏检测
    let callback = async () => {
      let d = await display.getDefaultDisplaySync();
      if(d.width > d.height) {
        if(this.isSplashScreen) {
          this.rotationSplash = 1;
        }
        let result: ArkTsAttribute = { content: "postMessage", result: ["displaychange"] };
        capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", this.webTag);
      }else {
        if(this.isSplashScreen) {
          this.rotationSplash = 0;
        }
        let result: ArkTsAttribute = { content: "postMessage", result: ["displaychange"] };
        capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", this.webTag);
      }
    }
    callback();
    display.on("change", callback);

    if(this.cookies.length > 0) {
      webview.WebCookieManager.putAcceptThirdPartyCookieEnabled(true);
      webview.WebCookieManager.putAcceptCookieEnabled(true);
      webview.WebCookieManager.clearSessionCookieSync();
      //Strict、Lax 和 None
      try {
        this.cookies.forEach((values, key) =>{
            let domain:string = key!;
            let cookies = values;
            cookies?.forEach((value)=>{
              if(value == "")
                return;
              let cookie:string = value;
              if(domain.startsWith("https://")) {
                cookie += "; SameSite=None; Secure";
              }
              try {
                webview.WebCookieManager.configCookieSync(domain, cookie);
              } catch (error) {
                const cordovaError = NormalizeError(error);
                console.error(`ErrorCode: ${cordovaError.code},  Message: ${cordovaError.message}`);
              }
            });
        });
      } catch (error) {
        const cordovaError = NormalizeError(error);
        console.error(`Failed to aboutToAppear. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
      }
      webview.WebCookieManager.saveCookieAsync();
    }

    if(this.webId == "") {
      this.webId = this.webTag;
    }
    try {
      if(!cordovaWebIdToWebviewGlobe.hasKey(this.webTag)) {
        if (this.parentPage) {
          this.cordovaInterface.setPage(this.parentPage);
        }
        webview.WebviewController.setWebDebuggingAccess(this.isWebDebug);
        capacitor.SetSchemeHandler(this.customSchemes, this.webTag);
        capacitor.RegisterCustomSchemes(this.customSchemes);

        let preferences:string = capacitor.InitCordova(this.webTag, this.strTmpUrl, PluginHandler, this.getUIContext().getHostContext()!.databaseDir, this.getUIContext().getHostContext()!.resourceManager, this.customHttpHeaders, this.isAllowCredentials, this.basePath);
        let jsonConfig:object = JSON.parse(preferences) as object;
        let arrayObject:Array<object> = jsonConfig?.["preferences"];
        arrayObject.forEach((obj:object)=>{
          if(obj?.["name"] && obj?.["value"]) {
            if(obj?.["name"] === "Hostname") {
              this.strTmpUrl = obj?.["value"];//设置hostname
            }

            if(obj?.["name"] === "AutoHideSplashScreen") {
              this.AutoHideSplashScreen = obj?.["value"].toLowerCase() === "true"?true:false;
            }
            if(obj?.["name"] === "SplashScreenDelay") {
              this.SplashScreenDelay = Number(obj?.["value"]);
            }
            if(obj?.["name"] === "FadeSplashScreenDuration") {
              this.FadeSplashScreenDuration = obj?.["value"];
            }
            if(obj?.["name"] === "ShowSplashScreenSpinner") {
              this.ShowSplashScreenSpinner = obj?.["value"].toLowerCase() === "true"?true:false;
            }
            if(obj?.["name"] === "SplashMaintainAspectRatio") {
              this.SplashMaintainAspectRatio = obj?.["value"].toLowerCase() === "true"?true:false;
            }
            if(obj?.["name"] === "SplashShowOnlyFirstTime") {
              this.SplashShowOnlyFirstTime = obj?.["value"].toLowerCase() === "true"?true:false;
            }
            if(obj?.["name"] === "SplashScreenSpinnerColor") {
              this.SplashScreenSpinnerColor = obj?.["value"];
            }
            if(obj?.["name"] === "FullScreenAfterCloseSplash") {
              this.isFullScreenAfterCloseSplash = obj?.["value"].toLowerCase() === "true"?true:false;
            }

            this.cordovaPreferences.set(obj?.["name"], obj?.["value"]);
          }
        })

        let configFile = this.getJsonObjectFromRawFileSync('capacitor.config.json');
        if (configFile) {
          this.splashScreenConfig(configFile as object);
        }

        try {
          //多webview下，只有第一个webview会启动闪屏
          if (cordovaWebTagToObjectGlobe.length == 0) {
            this.getUIContext().getHostContext()!.resourceManager.getDrawableDescriptorByName(this.SplashResourcePortrait);
            this.getUIContext().getHostContext()!.resourceManager.getDrawableDescriptorByName(this.SplashResourceLandscape);
            this.isStartShow = true;
            this.createNode(this.SplashScreenDelay);
          }
        } catch (e) {
          this.isSplashScreen = false;
          this.isStartShow = false;
          console.error("Splash image resource does not exist, flashing screen startup failed");
        }

        if(this.indexPage == "") {//默认加载页面
            this.src = "https://" + this.strTmpUrl + "/www/index.html";
        } else {
          this.indexPage = this.indexPage.trim();
          if(this.indexPage.indexOf("http") == 0 || this.indexPage.indexOf("file") == 0 || this.indexPage.match("^[A-Za-z0-9+.-]*://.*?$")) {
            this.src = this.indexPage;
          } else if(this.indexPage.charAt(0) != "/") {
            this.src = "https://" + this.strTmpUrl + "/"+this.indexPage;
          } else {
            this.src = "https://" + this.strTmpUrl + this.indexPage;
          }
        }
        InitArkTsPlugins(this.webId, this.webTag, this.cordovaPreferences, this.cordovaInterface, this.cordovaWebView, this.cordovaPlugs, this.capacitorPlugins);
        this.lifeCycle.execOnAboutToAppear(this.webviewController, this.parentPage);
        this.backPress.webviewController = this.webviewController;
        this.lifeCycle.webviewController = this.webviewController;
        this.backPress.onWebViewBackPress = this.onWebViewBackPress;
        cordovaWebTagToObjectGlobe.set(this.webTag, this);
      }
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to aboutToAppear. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
    this.cordovaWebView.setIsShow(true);

    //通知所有插件
    let result: ArkTsAttribute = {content:"start", result:[this.webTag]};
    capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");

    //监听来电状态通知所有插件
    if(canIUse("SystemCapability.Telephony.StateRegistry")) {
      try {
        observer.on('callStateChange', (data) => {
          let state = call.getCallStateSync();
          switch (state) {
            case call.CallState.CALL_STATE_RINGING: {
              let result: ArkTsAttribute = { content: "ringing", result: [] };
              capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");
            }
              break;
            case call.CallState.CALL_STATE_OFFHOOK: {
              let result: ArkTsAttribute = { content: "offhook", result: [] };
              capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");
            }
              break;
            case call.CallState.CALL_STATE_IDLE: {
              let result: ArkTsAttribute = { content: "idle", result: [] };
              capacitor.onArkTsResult(JSON.stringify(result), "CoreHarmony", "");
            }
              console.log('挂断或空闲');
              break;
          }
        });
      } catch (error) {
        const cordovaError = NormalizeError(error);
        console.error(`Failed to aboutToAppear. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
      }
    }

    if(this.isSplashScreen && this.SplashFullScreen) {
      SetSplashScreenFullScreen(this.getUIContext().getHostContext() as common.UIAbilityContext);
    }

    if(this.isSplashScreen && this.SplashImmersive){
      SetSplashScreenImmersive(this.getUIContext().getHostContext() as common.UIAbilityContext);
    }

    if(this.isInjectBridgeJs) {
      if (this.basePath == "" && this.indexPage.lastIndexOf("/") > 0) {
        this.basePath = this.indexPage.substring(0, this.indexPage.lastIndexOf("/"));
      }
      if (this.basePath == "" && this.indexPage == "") {
        this.basePath = "www"
      }
      let capacitorJs:string = capacitor.getCapacitorInjectJs(this.webTag, this.basePath);
      if (capacitorJs != "") {
        this.scriptItemArray.push({ script: capacitorJs, scriptRules: ["*"] });
      }
    }
  }


  getJsonObjectFromRawFileSync(fileName: string): object | null {
    try {
      const uiContext = this.getUIContext();
      const hostContext = uiContext?.getHostContext();
      if (!hostContext || !hostContext.resourceManager) {
        return null;
      }
      const rawData: Uint8Array = hostContext.resourceManager.getRawFileContentSync(fileName);
      const decoder = util.TextDecoder.create('utf-8', { ignoreBOM: true });
      const jsonString: string = decoder.decodeToString(rawData) ?? '';
      if (!jsonString) return null;
      const jsonObj = JSON.parse(jsonString) as object;
      return jsonObj;
    } catch (error) {

      return null;
    }
  }

  private splashScreenConfig(configObj: object) {

    const plugins = (configObj as Record<string, object>).plugins;
    const splashScreen = plugins ? (plugins as Record<string, object>).SplashScreen : undefined;

    // 无SplashScreen节点就直接返回，不报错
    if (splashScreen === undefined || splashScreen === null || (Object.keys(splashScreen).length == 0) ) {
      this.isCordova = true;
      return;
    } else {
      this.isCordova = false;
    }

    const splash = splashScreen as Record<string, number | boolean | string>;
    if (splash.launchShowDuration !== undefined) {
      this.SplashScreenDelay = splash.launchShowDuration as number;
    }

    if (splash.launchAutoHide !== undefined) {
      this.AutoHideSplashScreen = splash.launchAutoHide as boolean;
    }

    if (splash.launchFadeOutDuration !== undefined) {
      this.FadeOutDuration = splash.launchFadeOutDuration as number;
    }

    if (splash.backgroundColor !== undefined) {
      this.SplashScreenBackgroundColor = splash.backgroundColor as string;
    }

    if (splash.androidSplashResourceName !== undefined) {
      this.AndroidSplashResourceName = splash.androidSplashResourceName as string;
    }

    if (splash.AndroidLandscapeSplashResourceName !== undefined) { //未使用
      this.SplashResourceLandscape = splash.AndroidLandscapeSplashResourceName as string;
    }

    if (splash.AndroidPortraitSplashResourceName !== undefined) { //未使用
      this.SplashResourcePortrait = splash.AndroidPortraitSplashResourceName as string;
    }

    if (splash.androidScaleType !== undefined) {
      this.AndroidScaleType = splash.androidScaleType as string;
    }

    if (splash.androidSpinnerStyle !== undefined) {
      this.AndroidSpinnerStyle = splash.androidSpinnerStyle as string;
    }

    if (splash.showSpinner !== undefined) {
      this.ShowSplashScreenSpinner = splash.showSpinner as boolean;
    }

    if (splash.iosSpinnerStyle !== undefined) {
      if (this.AndroidSpinnerStyle.length <= 0) {
        this.IosSpinnerStyle = splash.iosSpinnerStyle as string;
      }
    }

    if (splash.spinnerColor !== undefined) {
      this.SplashScreenSpinnerColor = splash.spinnerColor as string;
    }

    if (splash.splashFullScreen !== undefined) {
      this.SplashFullScreen = splash.splashFullScreen as boolean;
    }

    if (splash.splashImmersive !== undefined) {
      this.SplashImmersive = splash.splashImmersive as boolean;
    }

    if (splash.layoutName !== undefined) {
      this.LayoutName = splash.layoutName as string;
    }

    if (splash.useDialog !== undefined) {
      this.UseDialog = splash.useDialog as boolean;
    }
  }

  createNode(showDuration:number){
    if (showDuration > 0) {
      this.isSplashScreen = true;
      this.splashScreenPortrait =
        new SplashScreenNodeController(
          this.getUIContext().getHostContext()!,
          (this.isCordova || this.isStartShow) ? this.SplashResourcePortrait : this.AndroidSplashResourceName,
          this.SplashMaintainAspectRatio,
          this.ShowSplashScreenSpinner,
          this.SplashScreenSpinnerColor,
          this.SplashScreenBackgroundColor,
          this.AndroidScaleType,
          this.AndroidSpinnerStyle); //闪屏结点，在cordova初始化完后，再初始化
      this.splashScreenPortrait.initNode(this.getUIContext());
      this.splashScreenLandscape =
        new SplashScreenNodeController(
          this.getUIContext().getHostContext()!,
          (this.isCordova || this.isStartShow) ? this.SplashResourcePortrait : this.AndroidSplashResourceName,
          this.SplashMaintainAspectRatio,
          this.ShowSplashScreenSpinner,
          this.SplashScreenSpinnerColor,
          this.SplashScreenBackgroundColor,
          this.AndroidScaleType,
          this.AndroidSpinnerStyle); //闪屏结点，在cordova初始化完后，再初始化
      this.splashScreenLandscape.initNode(this.getUIContext());
    }
  }

  public showSplashScreenWithFade(autoHide:boolean,fadeInDuration:number,fadeOutDuration:number,showDuration:number) {

    if(showDuration <= 0){
      return;
    }

    if (!this.splashScreenPortrait || !this.splashScreenLandscape ) {
      this.createNode(showDuration);
    }

    this.splashScreenOpacityValue = 0;
    this.isSplashScreen = true;
    this.AutoHideSplashScreen = autoHide;

    if(this.SplashFullScreen) {
      SetSplashScreenFullScreen(this.getUIContext().getHostContext() as common.UIAbilityContext);
    }

    if(this.SplashImmersive){
      SetSplashScreenImmersive(this.getUIContext().getHostContext() as common.UIAbilityContext);
    }

    if(fadeInDuration) {
      this.getUIContext().animateTo({
        duration: this.isCordova ? 0 : Number(fadeInDuration),
        onFinish: () => {
          this.splashScreenHide(false, fadeOutDuration, showDuration);
        }
      }, () => {
        this.splashScreenOpacityValue = 1; // 淡入到 1
      });
    } else {
      this.splashScreenOpacityValue = 1;
      this.splashScreenHide(false, fadeOutDuration, showDuration);
    }
  }

  public clearFullScreenModel() {

    if(this.SplashFullScreen) {
      ClearSplashFullScreen(this.getUIContext().getHostContext() as common.UIAbilityContext,
        this.isFullScreenAfterCloseSplash, this.SystemBarPropertiesAfterCloseSplash);
    }

    if(this.SplashImmersive){
      ClearSplashScreenImmersive(this.getUIContext().getHostContext() as common.UIAbilityContext,this.isFullScreenAfterCloseSplash)
    }
  }

  //关闭闪屏
  public splashScreenHide(bHide:boolean,fadeOutDuration?:number,showDuration?:number) {

    const delay = typeof showDuration === "number" ? showDuration : Number(this.SplashScreenDelay);
    const duration = typeof fadeOutDuration === "number" ? fadeOutDuration : Number(this.FadeOutDuration);

    if(this.isSplashScreen && this.AutoHideSplashScreen) {
      if (duration <= 0) {
        this.isSplashScreen = false;
        this.clearFullScreenModel();
      } else {
        this.getUIContext().animateTo({
          delay:  delay,
          duration: this.isCordova ? 0 : duration,
          onFinish: () => {
            this.isSplashScreen = false // 动画完成后隐藏组件
            this.splashScreenOpacityValue = 1 // 触发透明度动画
            //状态栏
            this.clearFullScreenModel();
          }
        }, () => {
          this.splashScreenOpacityValue = 0 // 触发透明度动画
        })
      }
      //需要增加逻辑 如果 没有过渡时间等参数 直接关闭 如果存在这些
    } else {
      if(bHide) {
        this.isSplashScreen = false;
        this.clearFullScreenModel();
      }
    }
  }

  aboutToDisappear() {
    if(canIUse("SystemCapability.Telephony.StateRegistry")) {
      try {
        observer.off('callStateChange');
      } catch (error) {
        const cordovaError = NormalizeError(error);
        console.error(`Failed to aboutToDisappear. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
      }
    }
    NotifyArkTsPluginDestroy(this.webTag);
    removeWebAndPlugins(this.webId);
    this.lifeCycle.execOnDisAppear(this.webviewController, this.parentPage);
  }

  @Builder
  CustomerTitle() {
    Row(){
      Text(this.title).fontColor(this.titleFontColor).fontSize($r('sys.float.titlebar_title_tertiary_size'))
    }.width('100%').backgroundColor(this.barBackgroundColor).height($r('sys.float.ohos_id_navigation_bar_height')).alignItems(VerticalAlign.Center).justifyContent(this.titleFontAlign)
  }

  @Builder
  builder() {
    Column() {
      Stack({ alignContent: Alignment.Top }) {
        Web({ src: this.src, controller: this.webviewController })
          .onControllerAttached(() => {
            if (this.src.startsWith("file://")) {
              let path: string = this.src.substring(7);
              path = path.substring(0, path.lastIndexOf("/"));
              try {
                this.webviewController.setPathAllowingUniversalAccess([path]);
              } catch (error) {
                const cordovaError = NormalizeError(error);
                console.error(`ErrorCode: ${cordovaError.code},  Message: ${cordovaError.message}`);
              }
            }
            this.lifeCycle.execOnControllerAttached(this.webviewController, this.parentPage);
          })
          .onLoadIntercept((event) => {
            if (event) {
              let url: string = event.data.getRequestUrl();
              return this.lifeCycle.execOnLoadIntercept(event.data, this.parentPage);
            }
            return false;
          })
          .onOverrideUrlLoading((webResourceRequest: WebResourceRequest) => {
            return this.lifeCycle.execOnOverrideUrlLoading(webResourceRequest, this.parentPage);
          })
          .onInterceptRequest((event) => {
            if (this.onInterceptWebRequest && event) {
              return this.onInterceptWebRequest(event.request, this.webTag, this.parentPage);
            }
            return null;
          })
          .onPageBegin((event) => {
            if (event) {
              this.lifeCycle.execOnPageBegin(event.url, this.parentPage);
            }
          })
          .onProgressChange((event) => {
            if (event) {
              this.lifeCycle.execOnProgressChange(event.newProgress, this.parentPage);
            }
          })
          .runJavaScriptOnDocumentStart(this.scriptItemArray)
          .javaScriptAccess(true)
          .attributeModifier(this.webAttributeModifier)
          .zoomAccess(false)
          .blockNetwork(false)
          .domStorageAccess(true)
          .mixedMode(MixedMode.All)
          .databaseAccess(true)
          .mediaPlayGestureAccess(false)
          .multiWindowAccess(true)
          .allowWindowOpenMethod(true)
          .onSslErrorEventReceive((event) => {
            this.lifeCycle.execOnSslErrorEventReceive(event, this.webviewController, this.parentPage);
          })
          .onSslErrorEvent((event: SslErrorEvent) => {
            this.lifeCycle.execOnSslErrorEvent(event, this.webviewController, this.parentPage);
          })
          .onClientAuthenticationRequest((event)=>{
            this.lifeCycle.execOnClientAuthenticationRequest(event, this.src, this.webviewController, this.parentPage);
            return true;
          })
          .onErrorReceive((event)=>{
            this.lifeCycle.execOnErrorReceive(event, this.webviewController, this.parentPage);
          })
          .onWindowNew((event) => {
            //主要解决a标签打开新窗口_blank的问题
            if (this.isNavPath) { //在路由中打开
              let jsonObject:Record<string, string> = {};
              this.cordovaPreferences.getAll().forEach((value, key) => {
                if (key !== undefined && value !== undefined) {
                  jsonObject[key] = value;
                }
              });
              let paras: Array<string> = new Array<string>();
              paras.push(JSON.stringify({
                url: event.targetUrl,
                title: "",
                target: "_blank",
                args: ""
              }), this.inAppBrowserWebId, this.webTag, JSON.stringify(jsonObject));
              this.pageInfos.pushPathByName('InAppBrowser', paras);
            } else { //在当前窗口重新加载新页面
              this.src = event.targetUrl;
              this.webviewController.loadUrl(this.src);
            }
            // 将新窗口对应WebviewController返回给Web内核。
            // 若不调用event.handler.setWebController接口，会造成render进程阻塞。
            // 如果没有创建新窗口，调用event.handler.setWebController接口时设置成null，通知Web没有创建新窗口。
            event.handler.setWebController(null);
          })
          .fileAccess(true)
          .onPageVisible((event) => {
            if (event) {
              this.lifeCycle.execOnPageVisible(event.url, this.parentPage);
            }
          })
          .onPageEnd((event) => {
            this.title = this.webviewController.getTitle();
            this.cordovaWebView.setWebAttribute(this.webAttributeModifier.attribute);
            this.webviewController.setDownloadDelegate(this.downDelegate);
            if (this.onSetCordovaWebAttribute) {
              this.onSetCordovaWebAttribute(this.cordovaWebView);
            }
            if (event) {
              this.lifeCycle.execOnPageEnd(event.url, this.webviewController, this.parentPage);
            }
            this.splashScreenHide(false);
          })
          .onRenderExited((event) => {
            if (event) {
              this.lifeCycle.execOnRenderExited(event.renderExitReason, this.parentPage);
            }
          })
          .onAlert((event) => {
            if (event) {
              this.getUIContext().showAlertDialog({
                title: event.url,
                message: event.message,
                primaryButton: {
                  value: 'ok',
                  action: () => {
                    event.result.handleConfirm();
                  }
                },
                cancel: () => {
                  event.result.handleCancel();
                }
              })
            }
            return true;
          })
          .onConfirm((event) => {
            if (event) {
              this.getUIContext().showAlertDialog({
                title: event.url,
                message: event.message,
                primaryButton: {
                  value: 'cancel',
                  action: () => {
                    event.result.handleCancel();
                  }
                },
                secondaryButton: {
                  value: 'ok',
                  action: () => {
                    event.result.handleConfirm();
                  }
                },
                cancel: () => {
                  event.result.handleCancel();
                }
              })
            }
            return true;
          })
          .onPrompt((event) => {
            if (event) {
              //cordova依赖于此函数设置桥接密码和桥接方式，必须调用
              let ret:string = capacitor.onJsPrompt(this.webTag, event.url, event.message, event.value);
              if (ret !== undefined && ret != "") {
                event.result.handlePromptConfirm(ret);
                return false;
              } else if (ret === undefined) {
                let params: object = new Object;
                params["title"] = event.url;
                params["message"] = event.message;
                params["defaultValue"] = event.value;
                params["result"] = event.result;
                ShowPromptDialog(this.getUIContext(), params);
                return true;
              }
            }
            return false;
          })
          .geolocationAccess(true)
          .onGeolocationShow((event) => {
            event.geolocation.invoke(event.origin, true, false); // 允许此站点地理位置权限请求
          })
          .enableNativeEmbedMode(true)
          .registerNativeEmbedRule(this.nativeEmbedHtmlTag, this.nativeEmbedHtmlType)
          .onNativeEmbedLifecycleChange((embed) => {
            this.onNativeEmbedLifecycleChange(embed, this.cordovaWebView, this.parentPage);
          })
          .onNativeEmbedGestureEvent((touch) => {
            this.onNativeEmbedGestureEvent(touch, this.cordovaWebView, this.parentPage);
          })
          .onPermissionRequest((event) => {
            this.onPermissionRequest(event, this.parentPage);
          })
          .keyboardAvoidMode(this.webKeyboardAvoidMode)
          .height('100%')
          .width('100%')
          .textZoomRatio(this.textZoomRatio)
        if(this.isSplashScreen && this.rotationSplash == 0) {
          NodeContainer(this.splashScreenPortrait).opacity(this.splashScreenOpacityValue);
        }
        if(this.isSplashScreen && this.rotationSplash == 1) {
          NodeContainer(this.splashScreenLandscape).opacity(this.splashScreenOpacityValue);
        }
      }
      .height('100%')
      .width('100%')
    }
    .height('100%')
    .width('100%')
    .backgroundColor('#000000')
  }

  build() {
    if(this.isNavPath) {
      Navigation(this.pageInfos) {
        this.builder();
      }
      .title(this.CustomerTitle())
      .titleMode(NavigationTitleMode.Mini)
      .mode(NavigationMode.Stack)
      .hideTitleBar(this.hideTitleBar)
      .hideToolBar(false)
      .hideBackButton(true)
      .hideNavBar(false)
      .height('100%')
      .width('100%')
    } else {
      this.builder();
    }
  }
}
