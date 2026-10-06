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

import { webview } from "@kit.ArkWeb";
import { BusinessError } from "@kit.BasicServicesKit";
import { common } from "@kit.AbilityKit";
import { SymbolGlyphModifier, window } from "@kit.ArkUI";
import { deviceInfo } from "@kit.BasicServicesKit";
import { ArkTsAttribute } from "../PluginGlobal";
import capacitor from 'libcapacitor.so';

export enum ToolbarPosition {
  TOP = "TOP", // 工具栏在顶部
  BOTTOM = "BOTTOM", // 工具栏在底部
}

// Helper to send result to C++
export function sendResult(
  pluginName: string,
  webTag: string,
  content: string,
  result: Array<ESObject>,
) {
  let attr: ArkTsAttribute = {
    content: content,
    result: result,
  };
  capacitor.onArkTsResult(JSON.stringify(attr), pluginName, webTag);
}

export interface WebViewOptions {
  showURL?: boolean; // 是否显示 URL
  showToolbar?: boolean; // 是否显示工具栏
  clearCache?: boolean; // 是否清除缓存
  clearSessionCache?: boolean; // 是否清除会话缓存(session cookie 和 sessionStorage)
  mediaPlaybackRequiresUserAction?: boolean; // 媒体播放是否需要用户操作
  closeButtonText?: string; // 关闭按钮文本
  toolbarPosition?: ToolbarPosition; // 工具栏位置
  showNavigationButtons?: boolean; // 是否显示导航按钮
  leftToRight?: boolean; // 是否从左到右布局
  customWebViewUserAgent?: string | null; // 自定义 UserAgent
  android?: object; // Android 特定选项 (保持兼容性)
  iOS?: object; // iOS 特定选项 (保持兼容性)

  // 扁平化参数
  allowZoom?: boolean; // 是否允许缩放
  hardwareBack?: boolean; // 是否启用硬件返回键
  allowOverScroll?: boolean; // 是否允许过度滚动
}

interface UrlAttribute {
  url: string;
  title: string;
  target: string;
  args: string;
}

export interface UrlArgs {
  key: string;
  value: string;
}

@Builder
export function PageBrowser() {
  InAppBrowser();
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
  private parentWebTag: string = "";
  private serviceObject: string = "InAppBrowser";
  // ... (previous state variables)
  @State title: string = "";
  @State url: string = "";
  @State isShowProgress: boolean = true;
  @State progressValue: number = 0;
  @State showToolbar: boolean = true;
  @State beforeLoad: boolean = true;
  @State cleardata: boolean = false;
  @State closeButtonText: string = "";
  @State closeButtonColor: string | Resource = $r("sys.color.ohos_id_color_titlebar_icon");
  @State titleFontAlign: HorizontalAlign = HorizontalAlign.Start;
  @State showNavigationButtons: boolean = true;
  @State leftToRight: boolean = true;
  @State mediaPlaybackRequiresUserAction: boolean = false;
  @State allowZoom: boolean = false;
  @State fullscreen: boolean = false;
  @State toolbarColor: string | Resource = $r("sys.color.ohos_id_color_navigationbar_bg");
  @State navigationBarHeight: number | Resource = $r("sys.float.ohos_id_navigation_bar_height");
  @State visibilityTitle: Visibility = Visibility.Visible;
  @State toolbarPosition: string = ToolbarPosition.TOP;
  @State canGoBack: boolean = false;
  @State canGoForward: boolean = false;
  @State showURL: boolean = true;
  private webviewController: webview.WebviewController | null = null;
  private closeEventName: string = ""; //关闭按钮回调事件名称
  private backEventName: string = ""; //返回按钮回调事件名称
  @State customWebViewUserAgent: string | null = "";
  private needClearSessionStorage: boolean = false; // 是否需要清除 sessionStorage（首次加载时）
  hardwareBack: boolean = true; // 是否启用硬件返回键，默认启用
  @State allowOverScroll: boolean = true; // 是否允许过度滚动
  @State overScrollMode: OverScrollMode = OverScrollMode.ALWAYS; // 默认允许滚动回弹
  pageInfos: NavPathStack = new NavPathStack();
  @State isWebReady: boolean = false; // 添加标志位，确保参数解析完成后再加载 Web 组件
  @State modifierNavDest: NavDestinationModifier = new NavDestinationModifier();

  setFullScreen(isLayoutFullScreen: boolean) {
    let context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    let windowStage = context.windowStage;
    let windowClass: window.Window | undefined = undefined;
    windowStage.getMainWindow((err: BusinessError, data) => {
      const errCode: number = err.code;
      if (errCode) {
        console.error(
          `CONSOLE Failed to obtain the main window. Cause code: ${err.code}, message: ${err.message}`,
        );
        return;
      }
      windowClass = data;
      try {
        if (canIUse("SystemCapability.WindowManager.WindowManager.Core")) {
          windowClass.setWindowLayoutFullScreen(isLayoutFullScreen);
        }
        if (canIUse("SystemCapability.Window.SessionManager")) {
          windowClass.setSpecificSystemBarEnabled("status", !isLayoutFullScreen);
          windowClass.setSpecificSystemBarEnabled("navigation", !isLayoutFullScreen);
          windowClass.setSpecificSystemBarEnabled("navigationIndicator", !isLayoutFullScreen);
        }
      } catch (exception) {
        console.error(
          `CONSOLE Failed to set the window layout to full-screen mode. Cause code: ${exception.code}, message: ${exception.message}`,
        );
      }
    });
  }

  aboutToAppear() {
    // 初始化 webviewController
    this.webviewController = new webview.WebviewController();
  }

  aboutToDisappear() {
    if (this.fullscreen) {
      this.setFullScreen(false);
    }

    // 发送浏览器关闭事件
    sendResult(this.serviceObject, this.parentWebTag, "browserClosed", []);
  }

  @Builder
  CustomerTitle() {
    Text(this.title)
      .fontColor(this.closeButtonColor)
      .fontSize(20)
      .lineHeight($r("sys.float.ohos_id_navigation_bar_height"))
      .position({ x: "50%" })
      .translate({ x: "-50%" })
      .textAlign(TextAlign.Center)
      .height($r("sys.float.ohos_id_navigation_bar_height"));
  }

  @Builder
  NavigationMenus() {
    if (this.closeButtonText == "") {
      SymbolGlyph($r("sys.symbol.xmark"))
        .fontSize(24)
        .fontColor([this.closeButtonColor])
        .onClick(() => {
          this.pageInfos.pop();
        })
        .margin({ top: 20, right: 20 });
    } else {
      Text(this.closeButtonText)
        .fontSize(20)
        .margin({ top: 15, right: 10 })
        .fontColor(this.closeButtonColor)
        .onClick(() => {
          this.pageInfos.pop();
        });
    }
  }

  @Builder
  NavButtons() {
    Row({ space: 20 }) {
      SymbolGlyph($r("sys.symbol.chevron_left"))
        .fontColor([this.canGoBack ? this.closeButtonColor : "#FFC0C0C0"])
        .fontSize(30)
        .enabled(this.canGoBack)
        .onClick(() => {
          if (this.webviewController && this.webviewController.accessBackward()) {
            this.webviewController.backward();
          }
          if (this.backEventName != "") {
            sendResult(this.serviceObject, this.parentWebTag, this.backEventName, [
              this.parentWebTag,
            ]);
          }
        })
      SymbolGlyph($r("sys.symbol.chevron_right"))
        .fontColor([this.canGoForward ? this.closeButtonColor : "#FFC0C0C0"])
        .fontSize(30)
        .enabled(this.canGoForward)
        .onClick(() => {
          if (this.webviewController && this.webviewController.accessForward()) {
            this.webviewController.forward();
          }
          if (this.backEventName != "") {
            sendResult(this.serviceObject, this.parentWebTag, this.backEventName, [
              this.parentWebTag,
            ]);
          }
        })
    }.visibility(this.showNavigationButtons ? Visibility.Visible : Visibility.None);
  }

  @Builder
  CloseButton() {
    Column() {
      if (this.closeButtonText == "") {
        SymbolGlyph($r("sys.symbol.xmark"))
          .fontSize(24)
          .fontColor([this.closeButtonColor])
          .onClick(() => {
            this.pageInfos.pop();
            if (this.closeEventName != "") {
              sendResult(this.serviceObject, this.parentWebTag, this.closeEventName, [
                this.parentWebTag,
              ]);
            }
          });
      } else {
        Text(this.closeButtonText)
          .fontColor(this.closeButtonColor)
          .onClick(() => {
            this.pageInfos.pop();
            if (this.closeEventName != "") {
              sendResult(this.serviceObject, this.parentWebTag, this.closeEventName, [
                this.parentWebTag,
              ]);
            }
          });
      }
    };
  }

  @Builder
  ToolbarBuilder() {
    Row({ space: 10 }) {
      this.NavButtons()
      Column() {
        Text(this.showURL ? this.url : this.title)
          .fontColor(this.closeButtonColor)
          .fontSize(14)
          .visibility(this.visibilityTitle)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .width("100%")
      }
      .layoutWeight(1)
      .alignItems(this.titleFontAlign)

      this.CloseButton()
    }
    .width("100%")
    .height(this.navigationBarHeight)
    .backgroundColor(this.toolbarColor)
    .alignItems(VerticalAlign.Center)
    .direction(this.leftToRight ? Direction.Ltr : Direction.Rtl)
    .padding({ left: 10, right: 10 });
  }

  build() {
    NavDestination() {
      Column() {
        if (this.showToolbar && this.toolbarPosition === ToolbarPosition.TOP) {
          this.ToolbarBuilder();
        }
        Stack({ alignContent: Alignment.Top }) {
          if (this.isWebReady) {
            Web({ src: this.url, controller: this.webviewController! })
              .onControllerAttached(() => {
                if (this.webviewController) {
                  // 清除缓存数据
                  if (this.cleardata) {
                    this.webviewController.removeCache(true);
                    this.webviewController.clearHistory();
                    this.webviewController.clearMatches();
                    this.webviewController.clearSslCache();
                  }

                  // 设置本地文件访问权限
                  if (this.url.startsWith("file://")) {
                    let path: string = this.url.substring(7);
                    path = path.substring(0, path.lastIndexOf("/"));
                    try {
                      this.webviewController.setPathAllowingUniversalAccess([path]);
                    } catch (error) {
                      let err = error as BusinessError;
                      console.error(
                        `CONSOLE setPathAllowingUniversalAccess ErrorCode: ${err.code}, Message: ${err.message}`,
                      );
                      sendResult(this.serviceObject, this.parentWebTag, "loaderror", [err.message]);
                    }
                  }

                  // 设置 UserAgent
                  if (this.customWebViewUserAgent) {
                    let userAgent =
                      this.webviewController.getUserAgent() + " " + this.customWebViewUserAgent;
                    this.webviewController.setCustomUserAgent(userAgent);
                  }
                }
              })
              .onPageBegin((event) => {
                if (event) {
                  this.url = event.url;
                  sendResult(
                    this.serviceObject,
                    this.parentWebTag,
                    "browserPageNavigationCompleted",
                    [event.url],
                  );
                }
              })
              .onPageEnd((event) => {
                if (event && this.webviewController) {
                  this.canGoBack = this.webviewController.accessBackward();
                  this.canGoForward = this.webviewController.accessForward();
                  if (this.title == "") {
                    this.title = this.webviewController.getTitle();
                  }

                  // 清除 sessionStorage
                  if (this.needClearSessionStorage) {
                    try {
                      this.webviewController.runJavaScript("sessionStorage.clear();");
                      console.log("CONSOLE InAppBrowserPage 已清除 sessionStorage");
                      this.needClearSessionStorage = false;
                    } catch (error) {
                      console.error(
                        `CONSOLE 清除 sessionStorage 失败: ${(error as BusinessError).code}, ${(error as BusinessError).message}`,
                      );
                    }
                  }

                  sendResult(this.serviceObject, this.parentWebTag, "browserPageLoaded", []);
                }
              })
              .onProgressChange((event) => {
                if (event) {
                  this.progressValue = event.newProgress;
                  if (this.progressValue >= 100) {
                    this.isShowProgress = false;
                  } else {
                    this.isShowProgress = true;
                  }
                }
              })
              .onRenderExited((event) => {
                sendResult(this.serviceObject, this.parentWebTag, "loaderror", [this.parentWebTag]);
              })
              // 启用 JS 脚本执行
              .javaScriptAccess(true)
              // 设置是否允许缩放
              .zoomAccess(this.allowZoom)
              // 设置媒体播放是否需要用户手势
              .mediaPlayGestureAccess(this.mediaPlaybackRequiresUserAction)
              // 启用 DOM 存储
              .domStorageAccess(true)
              // 允许访问本地文件
              .fileAccess(true)
              // 允许混合内容加载
              .mixedMode(MixedMode.All)
              // 启用数据库访问
              .databaseAccess(true)
              // 允许地理位置访问
              .geolocationAccess(true)
              // 设置滚动回弹模式
              .overScrollMode(this.overScrollMode)
              .height("100%")
              .width("100%");
          }

          if (this.isShowProgress) {
            Progress({ value: 0, total: 100, type: ProgressType.Linear })
              .width("100%")
              .height(3)
              .value(this.progressValue);
          }
        }
        .width("100%")
        .layoutWeight(1)

        if (this.showToolbar && this.toolbarPosition === ToolbarPosition.BOTTOM) {
          this.ToolbarBuilder();
        }
      }.height("100%")
    }
    .menus(this.NavigationMenus())
    .onBackPressed((): boolean => {
      // 如果禁用硬件返回键,则拦截返回事件,阻止默认行为
      if (!this.hardwareBack) {
        return true; // 返回 true 表示已处理该事件,阻止系统默认的返回行为
      }

      // 如果启用硬件返回键,则处理 WebView 的返回逻辑
      if (this.webviewController && this.webviewController.accessBackward()) {
        this.webviewController.backward();
        return true; // 返回 true 表示已处理该事件
      }
      return false; // 返回 false 表示未处理,允许系统执行默认返回行为(关闭页面)
    })
    .hideTitleBar(true)
    .attributeModifier(this.modifierNavDest)
    .backButtonIcon(
      new SymbolGlyphModifier($r("sys.symbol.chevron_left"))
        .fontColor([this.closeButtonColor])
        .fontSize(24)
        .margin({ left: 8 }),
    )
    .title(this.CustomerTitle())
    .onReady((context: NavDestinationContext) => {
      this.pageInfos = context.pathStack;
      let strArrayPara = JSON.stringify(this.pageInfos.getParamByName("AppBrowserPage")[0]);
      let arrayPara: Array<string> = JSON.parse(strArrayPara);
      let urlInfo: UrlAttribute = JSON.parse(arrayPara[0]);

      this.parentWebTag = arrayPara[2];
      this.serviceObject = arrayPara[3];
      if (arrayPara.length > 4) {
        //获取到config.xml的配置，用于设置内置浏览器的配置
        try {
          // 解析来自 config.xml 的全局偏好设置 (arrayPara[4])
          // 这些设置用于统一控制 InAppBrowser 导航栏的默认样式
          let cordovaPreferences: object = JSON.parse(arrayPara[4]);
          // 1. 设置导航栏背景颜色
          if (cordovaPreferences?.["NavigationBarBackgroundColor"] !== undefined) {
            this.toolbarColor = cordovaPreferences?.["NavigationBarBackgroundColor"];
          }
          // 2. 设置导航栏字体颜色 (按钮和标题)
          if (cordovaPreferences?.["NavigationBarFontColor"] !== undefined) {
            this.closeButtonColor = cordovaPreferences?.["NavigationBarFontColor"];
          }
          // 3. 设置标题文字对齐方式 (left, right, center)
          if (cordovaPreferences?.["NavigationBarFontAlign"] !== undefined) {
            if (cordovaPreferences?.["NavigationBarFontAlign"] == "left") {
              this.titleFontAlign = HorizontalAlign.Start;
            } else if (cordovaPreferences?.["NavigationBarFontAlign"] == "right") {
              this.titleFontAlign = HorizontalAlign.End;
            } else if (cordovaPreferences?.["NavigationBarFontAlign"] == "center") {
              this.titleFontAlign = HorizontalAlign.Center;
            } else {
              this.titleFontAlign = HorizontalAlign.Start;
            }
          }
          // 4. 设置导航栏高度 (若同为0则隐藏高度或隐藏工具栏)
          if (cordovaPreferences?.["NavigationBarHeight"] !== undefined) {
            this.navigationBarHeight = Number(cordovaPreferences?.["NavigationBarHeight"]);
            if (this.navigationBarHeight == 0) {
              this.showToolbar = false;
            }
          }
        } catch (error) {
          console.error("CONSOLE InAppbrowser Failed to parse config json");
        }
      }

      this.title = urlInfo.title;
      let mapArgs: WebViewOptions = {};

      // 解析 args 参数
      try {
        mapArgs = JSON.parse(urlInfo.args);
      } catch (e) {
        console.error("CONSOLE InAppBrowserPage Failed to parse args");
      }

      // 必须在设置 this.url 之前应用所有设置，以确保 Web 组件初始化时配置生效

      // 处理 clearCache 参数（默认为 true，与 Android 一致）
      // 只有显式设置为 false 时才不清除缓存
      if (mapArgs.clearCache !== false) {
        this.cleardata = true;
        try {
          // 清除全局缓存和 cookie
          webview.WebCookieManager.clearAllCookiesSync();
          // 清除 WebStorage (localStorage, sessionStorage, IndexedDB 等)
          webview.WebStorage.deleteAllData();
        } catch (e) {
          console.error("CONSOLE InAppBrowserPage clearAllCookiesSync failed");
        }
      }

      // 处理 clearSessionCache 参数（默认为 true，与 Android 一致）
      // 只有显式设置为 false 时才不清除会话缓存
      if (mapArgs.clearSessionCache !== false) {
        this.needClearSessionStorage = true;
        try {
          webview.WebCookieManager.clearSessionCookieSync();
        } catch (e) {
          console.error("CONSOLE InAppBrowserPage clearSessionCookieSync failed");
        }
      }

      // 处理 showNavigationButtons 参数
      if (mapArgs.showNavigationButtons !== undefined) {
        this.showNavigationButtons = mapArgs.showNavigationButtons;
      }

      // 处理 allowOverScroll 参数
      if (mapArgs.allowOverScroll !== undefined) {
        this.allowOverScroll = mapArgs.allowOverScroll;
        this.overScrollMode = mapArgs.allowOverScroll
          ? OverScrollMode.ALWAYS
          : OverScrollMode.NEVER;
      }

      // 简单字段直接赋值
      if (mapArgs.showURL !== undefined) {
        this.showURL = mapArgs.showURL;
      }
      if (mapArgs.closeButtonText !== undefined) {
        this.closeButtonText = mapArgs.closeButtonText;
      }
      if (mapArgs.customWebViewUserAgent !== undefined && mapArgs.customWebViewUserAgent !== null) {
        this.customWebViewUserAgent = mapArgs.customWebViewUserAgent;
      }
      if (mapArgs.toolbarPosition !== undefined) {
        this.toolbarPosition = mapArgs.toolbarPosition as string;
      }
      if (mapArgs.leftToRight !== undefined) {
        this.leftToRight = mapArgs.leftToRight;
      }
      if (mapArgs.mediaPlaybackRequiresUserAction !== undefined) {
        this.mediaPlaybackRequiresUserAction = mapArgs.mediaPlaybackRequiresUserAction;
      }
      console.log(
        "CONSOLE InAppBrowserPage 媒体需要用户操作",
        this.mediaPlaybackRequiresUserAction,
      );
      if (mapArgs.allowZoom !== undefined) {
        this.allowZoom = mapArgs.allowZoom;
      }
      if (mapArgs.hardwareBack !== undefined) {
        this.hardwareBack = mapArgs.hardwareBack;
      }

      // 处理 showToolbar 参数
      if (mapArgs.showToolbar !== undefined) {
        this.showToolbar = mapArgs.showToolbar;
      }

      // 最后设置 URL，触发 Web 组件加载
      this.url = urlInfo.url;
      this.isWebReady = true;
    });
  }
}
