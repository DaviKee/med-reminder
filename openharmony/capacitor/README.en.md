# <center>capacitor</center>
[![zh-CN](https://img.shields.io/badge/lang-中文-blue.svg)](README.md)
[![en](https://img.shields.io/badge/lang-English-blue.svg)](README.en.md)

This project is developed based on [@capacitor/android@8.0.0](https://www.npmjs.com/package/@capacitor/android).

## Introduction

openHarmony-capacitor is an OpenHarmony port of Capacitor. All APIs are compatible with the Android and iOS versions of Capacitor. This document only covers the openHarmony-capacitor framework: usage guide, development notes, integration steps, and related topics.

## Supported Platforms

- **OpenHarmony**: 5.0+

## Dependencies

This project depends on OpenSSL. Official site: [https://openssl.org](https://openssl.org/). Integrate OpenSSL before building; the project builds only after OpenSSL is integrated successfully. Integration guide: [https://gitcode.com/li_in/openharmony-capacitor-openssl3.5](https://gitcode.com/li_in/openharmony-capacitor-openssl3.5).

## Development Notes

openHarmony-capacitor is an OpenHarmony port of Capacitor and supports custom plugin development on both the ArkTS side and the C/C++ side. The framework is implemented in C/C++, uses a self-developed Socket TCP/IP stack at the lower layer, and wraps HTTP/HTTPS to address cross-origin access without configuring a web server. Together with the WebView communication stack, it improves application-layer network efficiency.

## Additional Notes

openHarmony-capacitor uses a multi-page view model while remaining compatible with the single-page view model used on Android and iOS, so existing projects can be migrated easily. In complex projects, you can use multi-page views and multiple WebViews working together.

## Background

The Capacitor official site is [https://capacitorjs.com](https://capacitorjs.com). It is a cross-platform mobile framework adopted directly or indirectly by many vendors to build apps. It did not support OpenHarmony; developers could not adapt existing Android and iOS projects to OpenHarmony. openHarmony-capacitor was built to follow Capacitor’s official standards so existing projects can be ported to OpenHarmony with minimal extra work. New projects can target Android, iOS, and OpenHarmony from one codebase, saving time and effort.

This framework is an evolution of the cordova-openharmony framework. It reuses some cordova-openharmony capabilities and optimizes plugins and communication for Capacitor; some features still follow cordova-openharmony.

## Compatibility

Verified on the following versions:

1. SDK: 5.0.5(17); IDE: DevEco Studio: 6.0.0; ROM: 5.1.0.150;

## Usage Guide

### 1. Create a Project

Open DevEco Studio, create a project, choose Empty Ability, proceed to the next step, fill in the required fields, click Finish. The project is created.

### 2. Integrate Source Code

Download this repository and place it under the main project folder. The Capacitor module should appear in DevEco Studio.

### 3. Add Dependencies

Add the dependency in `oh-package.json` at the project root:

```json
{
  "dependencies": {
    "openHarmony-capacitor": "file:./capacitor"
  }
}
```

Then edit the project-level `build-profile.json5` and add under `modules`:

```json5
{
    "name": "capacitor",
    "srcPath": "./capacitor",
}
```

After these three steps, the Capacitor source is integrated into the main project.

### 4. Project Migration

**Front-end build**

To improve deployment flexibility, change front-end resource references and routing from absolute root paths to relative paths:

Adjust the page base URL:

1. In `index.html` at the project root, set the `<base>` tag’s `href` from the default absolute root `/` to `./` so all relative assets resolve correctly.
2. Configure the build output path: e.g. in Vue, set Webpack `publicPath` from `/` to `./` in `vue.config.js` so bundled JS, CSS, images, etc. use relative paths.

**Android project migration**

Copy everything under the original Android Studio project’s `assets` folder into `entry/src/main/resources/rawfile` in the OpenHarmony project. The Android `assets` folder typically contains `config.xml` (if any), `capacitor.config.json` (required), `capacitor.plugins.json` (required), and a `dist` folder. Rename `dist` to `www`. `www` should contain `index.html` (required), `cordova.js` (if any), `cordova_plugins.js` (if any), `css`, `js`, etc. For a custom entry page instead of the default, see the Advanced section. After copying, install the OpenHarmony equivalents of the Android plugins you use.

**iOS project migration**

Step 1: Copy the `public` folder from the original iOS app into `entry/src/main/resources/rawfile` in the OpenHarmony project and rename `public` to `www`. Contents include `index.html` (required), `cordova.js` (if any), `cordova_plugins.js` (if any), `css`, `js`, etc.

Step 2: Xcode configuration files live under the App folder and cannot be used as-is by openHarmony-capacitor; they must be converted. They store framework settings and plugin names and initializer classes. OpenHarmony initializes plugins from the Android-style config, so convert the Xcode config to Android’s format: add the Android platform to the Xcode project with Node so Android `config.xml` (if any), `capacitor.config.json` (required), and `capacitor.plugins.json` (required) are generated. Copy those files to `entry/src/main/resources/rawfile` in the OpenHarmony project. After copying, install the OpenHarmony equivalents of the iOS plugins you use.

**New projects**

If you have no Android or iOS project yet, create an Android project with Capacitor, then follow the Android migration steps above.

**Add configuration**

Add a `harmony` block in `capacitor.config.json` for custom settings.

```json
{
  "appId": "appId",
  "appName": "appName",
  "webDir": "www",
  "harmony": {
    // Custom configuration.
  }
}
```

**Special usage**

If you do not use Capacitor or need no native bridge/plugins and only render UI in a WebView, set `isInjectBridgeJs: false` on MainPage to skip injecting `native-bridge.js` and speed up loading.

### 5. Edit Index.ets

Open `entry/src/main/ets/pages/Index.ets` and replace its contents with the following (you may paste the whole block):

```typescript
import { MainPage, pageBackPress, pageHideEvent, pageShowEvent, PluginEntry, MainPageOnBackPress} from 'openHarmony-capacitor';
//import { TestPlugin } from "../plugins/TestPlugin" // Custom plugin TestPlugin; import your own plugins as needed
@Entry
@Component
struct Index {
  // ArkTS-side custom plugins: map plugin name to instance; see the custom development section
  cordovaPlugs: Array<PluginEntry> = [];
  mainPageOnBackPress: MainPageOnBackPress = new MainPageOnBackPress();
  /*
  cordovaPlugs: Array<PluginEntry> =
  [
      {
        pluginName: 'TestPlugin', // Plugin name
        pluginObject: new TestPlugin() // Plugin instance for the framework
      }
  ];
  */
  onPageShow() {
    pageShowEvent(); // Notify framework on page show
  }
  onBackPress() {
    pageBackPress(); // Intercept back key for framework handling
    return this.mainPageOnBackPress.backPress();
  }
  onPageHide() {
    pageHideEvent(); // Notify framework on page hide
  }
  build() {
    RelativeContainer() {
      // Default: load rawfile/www/index.html
      // For a custom entry page, see Advanced
      MainPage({ isWebDebug: false, cordovaPlugs: this.cordovaPlugs });
    }
    .height('100%')
    .width('100%')
  }
}
```

### 6. Edit EntryAbility.ets

Open `entry/src/main/ets/entryAbility/EntryAbility.ets` and change `onCreate` as follows:

```typescript
import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { webview } from '@kit.ArkWeb';  // Import WebView

// ... omitted code
onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
   hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onCreate');
   webview.WebviewController.initializeWebEngine();// Initialize WebView engine
}
```

### 7. Done

After these code changes, the OpenHarmony migration is complete. Build and test on an emulator or a device.

## Advanced Usage (Differences from Android and iOS)

### 1. MainPage `indexPage` for a custom entry path; supports rawfile, resfile, and sandbox paths

```typescript
/*
*    indexPage: default entry page, examples:
*    "/www/index.html": file under rawfile
*    "/data/storage/el2/base/files/www/index.html": load sandbox file via virtual host www.example.com
*    "https://cn.bing.com": load online page; must specify https or http
*    "file:///data/storage/el2/base/files/www/index.html": file protocol, el2 sandbox
*    "file:///data/storage/el1/bundle/entry/resources/resfile/www/index.html": file protocol, el1 resfile sandbox
*    "file://" + getContext().resourceDir + "/www/index.html": file protocol, el1 sandbox via resourceDir
*    Capacitor supports virtual host www.example.com for local files and file:// for local files
*    Changing this.indexPage reloads the WebView
*/
// ... other code omitted
MainPage({indexPage:"/www/index.html"});
// ... other code omitted
```

### 2. Register a custom URL scheme for Capacitor to intercept internally

```typescript
import { RegisterCustomSchemes } from 'openHarmony-capacitor';
onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
   hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onCreate');
   RegisterCustomSchemes("cmp"); // Register custom scheme
   webview.WebviewController.initializeWebEngine();// Initialize WebView engine
}
```

```typescript
// customSchemes: custom schemes, multiple schemes separated by ","
// ... other code omitted
MainPage({customSchemes:"cmp,xmp,xxx"});
// ... other code omitted
```

### 3. Intercept custom schemes on the WebView side (or http(s)); handle requests

```typescript
/*
* Intercept requests as needed; commonly used for custom schemes. If you use custom schemes, handle them here.
* Use this to handle your schemes, or handle in MainPage lifecycle callbacks — choose one approach, not both.
* After interception, two handling modes; the first is recommended:
*   1. Let the Capacitor WebView handle it: return null; Capacitor can replace all resources (online, local, js, img, css, etc.)
*   2. Handle yourself: return WebResourceResponse
* Notes:
*   1. Child callbacks cannot use `this`; use parentPage if you need the page instance.
*   2. Mode 1 is simpler and faster; prefer mode 1.
*/
onInterceptWebRequest(request: WebResourceRequest, webTag: string): ESObject {
  let url = request.getRequestUrl();
  // Capacitor WebView replacement
  if (url == "cmp://v1.1.1/temp/test2.png") {
    /*
    * Replacement rules:
    * For local assets, use the virtual host https://www.example.com as the marker for local resources.
    * See FAQ for built-in www.example.com virtual host rules.
    * Replaced and replacement targets can be images, css, js, etc.
    * Examples:
    * 1. Sandbox path
    *   https://www.example.com/data/storage/el2/base/files/test.png
    * 2. rawfile assets
    *   https://www.example.com/www/test.png
    * 3. Online resources
    *   https://www.chuzhitong.com/images/logo.png
    * 4. cdvfile sandbox absolute path
    *   cdvfile:///data/storage/el2/base/files/test.png
    * This notifies the Capacitor WebView to apply resource replacement on subsequent loads.
    */
    SetResourceReplace(webTag, url, "https://www.chuzhitong.com/images/logo.png");
  }

  // Handle and return to WebView yourself
  if(url == "https://www.ext.com/v1.1.1/temp/test3.png") {
    let response = new WebResourceResponse();
    response.setResponseData($rawfile("www/picture/bao.png"));
    response.setResponseEncoding('utf-8');
    response.setResponseMimeType("image/png");
    response.setResponseCode(200);
    response.setReasonMessage('OK');
    response.setResponseIsReady(true);
    return response;
  }
  return null;
}

// ... other code omitted
/*
* onInterceptWebRequest: return null to pass through; return WebResourceResponse to handle
*
*/
MainPage({onInterceptWebRequest: this.onInterceptWebRequest});
// ... other code omitted
```

### 4. Set WebView attributes dynamically from native after load

```typescript
/*
* After native page load, inject JS; or inject after MainPage finishes loading in lifecycle
*/
onSetCordovaWebAttribute(cordovaWebView: CordovaWebView) {
    if(cordovaWebView) {
    // Get WebView attributes for dynamic changes; see links below; fires after page load
    // OpenHarmony does not support dynamic WebAttribute for all props; some work; unsupported ones throw "Method not implemented.", "is not callable", etc.
    //https://docs.openharmony.cn/pages/v5.1.0/zh-cn/application-dev/reference/apis-arkweb/js-apis-webview.md
    //https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/reference/apis-arkui/arkui-ts/ts-universal-attributes-attribute-modifier.md
    cordovaWebView!.getWebAttribute()?.height('50%');
    // Get WebView controller for runJavaScript / injection; see:
    //https://docs.openharmony.cn/pages/v5.1.0/zh-cn/application-dev/reference/apis-arkweb/ts-basic-components-web.md
    cordovaWebView!.getWebviewController().runJavaScript("alert(1);");
  }
}
// ... other code omitted
MainPage({onSetCordovaWebAttribute: this.onSetCordovaWebAttribute});
// ... other code omitted
```

### 5. Multiple WebViews (multi-page views), custom `webId`, plugins for cross-WebView messaging (e.g. tablets)

```typescript
// ... other code omitted
// webId: custom id for multiple WebViews; must be unique; see custom plugin samples
MainPage({ webId:"123456"})
// ... other code omitted
```

### 6. Dynamic components: when combining WebView and NodeController, always set `webId` to avoid duplicate WebViews

```typescript
// Example: dynamic MainPage for hybrid native + WebView in one view
// For other parameters, see this document
//https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/reference/apis-arkui/arkui-js/js-components-create-elements.md
@Builder
function buildMainPage() {
  Column() {
    // Load online site directly
    MainPage({webId: "123456", indexPage: "https://cn.bing.com", cordovaPlugs: [
      {
        pluginName: 'TestPlugin', // Plugin name
        pluginObject: new TestPlugin() // Plugin instance
      }
    ]});
  }.width("100%").height("100%")
}

class TextNodeController extends NodeController {
  private textNode: BuilderNode<[]> | null = null;

  constructor() {
    super();
  }

  makeNode(context: UIContext): FrameNode | null {
    // Create BuilderNode
    this.textNode = new BuilderNode(context);
    this.textNode.build(wrapBuilder<[]>(buildMainPage));
    // Return node to display
    return this.textNode.getFrameNode();
  }
}
```

```typescript
// ... other code omitted
private textNodeController = new TextNodeController();

// ... other code omitted
RelativeContainer() {
    if (this.isShow) {
      NodeContainer(this.textNodeController)
        .width('100%')
        .height("100%")
        .backgroundColor('#FFF0F0F0')
    }
}
.height('30%')
.width('100%')

Button("Show/hide web").onClick(()=>{
  this.isShow = false;
})
```

### 7. W3C permissions for WebView (camera, microphone, etc.)

```typescript
// Web component W3C permission callback, e.g. camera and microphone
onPermissionRequest(event:OnPermissionRequestEvent, parentPage?: object){
  let page = parentPage as Index;// page is current page; pass this into MainPage via parentPage
  if (event) {
    // Camera and microphone: for re-prompt after deny, request permissions one by one
    // One-by-one shows multiple dialogs; alternatively grant multiple at once, but deny may block second prompt
    // Camera and microphone with dialog:
    //const yourPermissions: Array< Permissions> = ['ohos.permission.CAMERA', 'ohos.permission.MICROPHONE'];
    // Accelerometer and gyroscope without dialog
    const yourPermissions: Array< Permissions> = ['ohos.permission.ACCELEROMETER', 'ohos.permission.GYROSCOPE'];
    for (let i = 0; i < yourPermissions.length; i++) {
      let confirmPermissions: Array< Permissions> = [yourPermissions[i]];
      let atManager = abilityAccessCtrl.createAtManager();
      atManager.requestPermissionsFromUser(getContext(this), confirmPermissions).then((data) => {
        let grantStatus: Array< number> = data.authResults;
        if (grantStatus[0] != 0) {
          // User denied: guide to Settings
          atManager.requestPermissionOnSetting(getContext() as common.UIAbilityContext, confirmPermissions)
            .then((data: Array< abilityAccessCtrl.GrantStatus>) => {
              if (data.length > 0 && data[0] == 0 ) {
                event.request.grant(event.request.getAccessibleResource());
              }
              console.info('data:' + JSON.stringify(data));
            })
            .catch((err: BusinessError) => {
              console.error('data:' + JSON.stringify(err));
              return;
            });
        } else {
          event.request.grant(event.request.getAccessibleResource());
        }
      }).catch((error: BusinessError) => {
        console.error(`Failed to request permissions from user. Code is ${error.code}, message is ${error.message}`);
      })
    }
  }
}

/*
* onPermissionRequest: W3C permission callback for Web
*    Reference: https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/web/web-rtc.md
*/
MainPage({parentPage: this, onPermissionRequest: this.onPermissionRequest,});
```

### 8. Parent observes all MainPage lifecycle callbacks

```typescript
// MainPage lifecycle hooks; register one or more as needed
// Lifecycle order: https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/web/web-event-sequence.md
mainPageCycle?: MainPageCycle;
aboutToAppear() {
    this.mainPageCycle = new MainPageCycle()
        .setOnAboutToAppear((webviewController: webview.WebviewController, parentPage?: object)=>{
          // page is current page; pass this via parentPage into MainPage
          let page = parentPage as Index;
          console.log("exec onAboutToAppear");
        })
        .setOnControllerAttached((webviewController: webview.WebviewController, parentPage?: object)=>{
          console.log("exec onControllerAttached");
        })
        .setOnLoadIntercept((webResourceRequest: WebResourceRequest, parentPage?: object):boolean=>{
          console.log("exec onLoadIntercept");
          return false;
        })
        .setOnOverrideUrlLoading((webResourceRequest: WebResourceRequest, parentPage?: object):boolean=>{
          console.log("exec onOverrideUrlLoading");
          return false;
        })
        .setOnInterceptRequest((request: WebResourceRequest, webTag:string, parentPage?: object):WebResourceResponse|null=>{
          console.log("exec setOnInterceptRequest");
          return null;
        })
        .setOnPageBegin((url:string, parentPage?: object):void=>{
          console.log("exec onPageBegin");
        })
        .setOnProgressChange((newProgress: number, parentPage?: object):void=>{
          console.log("exec onProgressChange");
        })
        .setOnPageEnd((url:string, webviewController: webview.WebviewController, parentPage?: object):void=>{
          console.log("exec onPageEnd");
        })
        .setOnPageVisible((url:string, parentPage?: object):void=>{
          console.log("exec onPageVisible");
        })
        .setOnRenderExited((renderExitReason:RenderExitReason, parentPage?: object):void=>{
          console.log("exec onRenderExited");
        })
        .setOnDisAppear((parentPage?: object):void=>{
          console.log("exec onDisAppear");
        });
}
```

```typescript
// ... other code omitted
/*
* lifeCycle: pass lifecycle object so parent can observe MainPage
* parentPage: pass this for WebView parent; usable from plugins
*/
MainPage({lifeCycle: this.mainPageCycle, parentPage: this})
// ... other code omitted
```

### 9. Multiple WebViews in one Page (local + online hybrid)

```typescript
build() {
  Column() {
    RelativeContainer() {
      MainPage({indexPage:"/www/index.html"});
    }
    .height('30%')
    .width('100%')
    RelativeContainer() {
      MainPage({indexPage:"https://developer.huawei.com"});
    }
    .height('30%')
    .width('100%')
  }
}
```

### 10. Pages without cordova.js / capacitor.js: parent controls back or routing

```typescript
/*
* Control MainPage back navigation; pass this object into MainPage
* If the page has no cordova.js/capacitor.js, pageBackPress cannot notify Capacitor; use this object instead
* You can also control WebView routing with this object
*/
mainPageOnBackPress: MainPageOnBackPress = new MainPageOnBackPress();

onBackPress() {
	pageBackPress();
    /*
     * If the page has no cordova.js/capacitor.js (e.g. https://cn.bing.com),
     * return value:
     *  true: already at top of stack
     *  false: navigated back
     */
    //return this.mainPageOnBackPress.backPress();
	return true;
}
// backPress: pass routing controller when pages lack cordova.js/capacitor.js
MainPage({ backPress: this.mainPageOnBackPress })
```

### 11. MainPage navigation toggle for nested MainPage (avoid routing conflicts)

```typescript
/*
* isNavPath: true uses MainPage internal routing (default true); false disables it
*     When nesting MainPage, parent may need routing on while child turns it off to avoid conflicts
*/
MainPage({isNavPath:false});
```

### 12. Custom cookies (key/value pairs)

```typescript
// Add cookies manually for POST/GET; HTTPS session cookies are handled by Cordova automatically
// HTTP session cookies: see HTTPS cookie FAQ at the end
this.cookies.set("https://mem.tongecn.com", ["key1=value1; path=/; Domain=.tongecn.com", "key2=value2"]);

/*
* cookies: pass custom ArkTS cookies here
*     Usually Cordova handles cookies; ArkTS manual cookies are optional
*     For HTTP (not HTTPS), see cross-origin vs same-origin in FAQ
*/
MainPage({cookies: this.cookies});
```

### 13. WebView text zoom ratio (accessibility; ignore system font scaling)

```typescript
/*
*     textZoomRatio: WebView text zoom percent; default 100
*     After disabling system font follow and display scaling follow-up,
*     use this to set a uniform zoom and avoid layout breakage
*     Or use the Device plugin’s font APIs from JS; see Device plugin and FAQ
*/
MainPage({textZoomRatio:110});
```

### 14. Same-layer rendering and combining with plugins

```typescript
// Same-layer sample: add native TextInput on H5 page
@Observed
declare class Params{
  elementId: string
  textOne: string
  textTwo: string
  width: number
  height: number
  onTextChange?: (value: string) => void;
}

@Component
struct TextInputComponent {
  @Prop params: Params
  @State bkColor: Color = Color.Blue

  build() {
    Column() {
      TextInput({text: '', placeholder: 'please input your word...'})
        .placeholderColor(Color.Gray)
        .id(this.params?.elementId)
        .placeholderFont({size: 13, weight: 400})
        .caretColor(Color.Gray)
        .width(this.params?.width)
        .height(this.params?.height)
        .fontSize(14)
        .fontColor(Color.Black)
        .onChange((value:string)=>{
          if (this.params.onTextChange) {
            this.params.onTextChange(value); // Fire callback
          }
        })
    }
    // Outer container should match same-layer tag size
    .width(this.params.width)
    .height(this.params.height)
  }
}

@Builder
function TextInputBuilder(params:Params) {
  TextInputComponent({params: params})
    .width(params.width)
    .height(params.height)
    .backgroundColor(Color.White)
}

class MyNodeController extends NodeController {
  private rootNode: BuilderNode<[Params]> | undefined | null;
  private surfaceId_: string = "";
  private renderType_: NodeRenderType = NodeRenderType.RENDER_TYPE_DISPLAY;
  private width_: number = 0;
  private height_: number = 0;
  private embedId_: string = "";
  private isDestroy_: boolean = false;

  setRenderOption(params: ESObject) {
    this.surfaceId_ = params.surfaceId;
    this.renderType_ = params.renderType;
    this.embedId_ = params.embedId;
    this.width_ = params.width;
    this.height_ = params.height;
  }

  // Must override: build node tree and attach to NodeContainer
  makeNode(uiContext: UIContext): FrameNode | null {
    if (this.isDestroy_) { // rootNode is null
      return null;
    }
    if (!this.rootNode) {// rootNode undefined
      this.rootNode = new BuilderNode(uiContext, { surfaceId: this.surfaceId_, type: this.renderType_ });
      if(this.rootNode) {
        this.rootNode.build(wrapBuilder(TextInputBuilder),
            {textOne: "myTextInput", width: this.width_, height: this.height_, onTextChange:(value:string)=>{
         // After TextInput changes, notify JS; simple example — adjust JS as needed
         let jsFun:string = "setValue('"+value+"')";
         try {
           this.cordovaWebView?.getWebviewController().runJavaScript(jsFun);
         } catch (error) {
           console.log(error);
         }
        }})
        return this.rootNode.getFrameNode();
      } else {
        return null;
      }
    }
    return this.rootNode.getFrameNode();
  }

  updateNode(arg: Object): void {
    this.rootNode?.update(arg);
  }

  getEmbedId(): string {
    return this.embedId_;
  }

  setDestroy(isDestroy: boolean): void {
    this.isDestroy_ = isDestroy;
    if (this.isDestroy_) {
      this.rootNode = null;
    }
  }

  postEvent(event: TouchEvent | undefined): boolean {
    return this.rootNode?.postTouchEvent(event) as boolean
  }
}
```

```typescript
@Entry
@Component
export struct Index {
    // ... other code omitted
    public nodeControllerMap: Map<string, MyNodeController> = new Map();
    @State componentIdArr: Array<string> = [];
    @State widthMap: Map<string, number> = new Map();
    @State heightMap: Map<string, number> = new Map();
    @State positionMap: Map<string, Edges> = new Map();
    @State edges: Edges = {};
    @State textValue:string = "hello";
    /*
    * Same-layer lifecycle callback
    */
    onNativeEmbedLifecycleChange(embed: NativeEmbedDataInfo,cordovaWebView:CordovaWebView,parentPage?:object) {
      let page = parentPage as Index;// page is current page; pass this into MainPage
      console.log("NativeEmbed surfaceId" + embed.surfaceId);
      // If using embed.info.id as Map key, set id explicitly in H5
      const componentId = embed.info?.id?.toString() as string
      if (embed.status == NativeEmbedStatus.CREATE) {
        console.log("NativeEmbed create" + JSON.stringify(embed.info));
        // Create controller, set options, rebuild
        let nodeController = new MyNodeController()
        // embed width/height are px; convert to vp
        nodeController.setRenderOption({
          surfaceId : embed.surfaceId as string,
          type : embed.info?.type as string,
          renderType : NodeRenderType.RENDER_TYPE_TEXTURE,
          embedId : embed.embedId as string,
          width : cordovaWebView.getUIContext().px2vp(embed.info?.width),
          height : cordovaWebView.getUIContext().px2vp(embed.info?.height),
          cordovaWebView:cordovaWebView,
          textValue:page.textValue
        })
        page.edges = {left: `${embed.info?.position?.x as number}px`, top: `${embed.info?.position?.y as number}px`}
        nodeController.setDestroy(false);
        // Use web embed id as key in Map
        page.nodeControllerMap.set(componentId, nodeController);
        page.widthMap.set(componentId, cordovaWebView.getUIContext().px2vp(embed.info?.width));
        page.heightMap.set(componentId, cordovaWebView.getUIContext().px2vp(embed.info?.height));
        page.positionMap.set(componentId, page.edges);
        // Push id into @State after set for dynamic NodeContainer
        page.componentIdArr.push(componentId)
      } else if (embed.status == NativeEmbedStatus.UPDATE) {
        let nodeController = page.nodeControllerMap.get(componentId);
        console.log("NativeEmbed update" + JSON.stringify(embed));
        page.edges = {left: `${embed.info?.position?.x as number}px`, top: `${embed.info?.position?.y as number}px`}
        page.positionMap.set(componentId, page.edges);
        page.widthMap.set(componentId, cordovaWebView.getUIContext().px2vp(embed.info?.width));
        page.heightMap.set(componentId, cordovaWebView.getUIContext().px2vp(embed.info?.height));
        nodeController?.updateNode({page: page, textOne: 'update', width: cordovaWebView.getUIContext().px2vp(embed.info?.width), height: cordovaWebView.getUIContext().px2vp(embed.info?.height), text: page.textValue, onTextChange: page.onTextChangeCallBack} as ESObject);
      } else if (embed.status == NativeEmbedStatus.DESTROY) {
        console.log("NativeEmbed destroy" + JSON.stringify(embed));
        let nodeController = page.nodeControllerMap.get(componentId);
        nodeController?.setDestroy(true)
        page.nodeControllerMap.clear();
        page.positionMap.delete(componentId);
        page.widthMap.delete(componentId);
        page.heightMap.delete(componentId);
        page.componentIdArr.filter((value: string) => value != componentId)
      } else {
        console.log("NativeEmbed status" + embed.status);
      }
    }

    onNativeEmbedGestureEvent(touch: NativeEmbedTouchInfo,cordovaWebView:CordovaWebView,parentPage?:object) {
      let page = parentPage as Index;// page is current page; pass this into MainPage
      console.log("NativeEmbed onNativeEmbedGestureEvent" + JSON.stringify(touch.touchEvent));
      page.componentIdArr.forEach((componentId: string) => {
        let nodeController = page.nodeControllerMap.get(componentId);
        // Forward gesture to matching embedId
        if(nodeController?.getEmbedId() == touch.embedId) {
          let ret = nodeController?.postEvent(touch.touchEvent)
          if(ret) {
            console.log("onNativeEmbedGestureEvent success " + componentId);
          } else {
            console.log("onNativeEmbedGestureEvent fail " + componentId);
          }
          if(touch.result) {
            // Notify Web of gesture consumption result
            touch.result.setGestureEventResult(ret);
          }
        }
      })
    }

    /*
    * Callback when same-layer TextInput text changes
    * Custom plugins can read the new value
    */
    onTextChangeCallBack(page:Index, value:string) {
      page.textValue = value;
    }
    
    getTextValue():string {
      return this.textValue;
    }
    
    /*
    * Set displayed text for same-layer TextInput
    * Custom plugins can set TextInput display text
    */
    setNativeValue(id:string, value:string){
      this.textValue = value;
      let nodeController = this.nodeControllerMap.get(id);
      nodeController?.updateNode({page: this, textOne: 'update', width: this.widthMap.get(id), height: this.heightMap.get(id), text: this.textValue, onTextChange:this.onTextChangeCallBack} as ESObject)
    }

  RelativeContainer() {
      // Same-layer rendering
      ForEach(this.componentIdArr, (componentId: string) => {
        NodeContainer(this.nodeControllerMap.get(componentId))
          .position(this.positionMap.get(componentId))
          .width(this.widthMap.get(componentId))
          .height(this.heightMap.get(componentId))
      }, (embedId: string) => embedId)
      /*
       * nativeEmbedHtmlTag: register same-layer tag (default <embed>; use object for <object>)
       * nativeEmbedHtmlType: register tag type (default native; set as needed)
       * onNativeEmbedLifecycleChange: same-layer lifecycle
       * onNativeEmbedGestureEvent: same-layer gesture
       *    Reference: https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/web/web-same-layer.md
       */
      MainPage({
        parentPage: this,
        onNativeEmbedLifecycleChange: this.onNativeEmbedLifecycleChange,
        onNativeEmbedGestureEvent: this.onNativeEmbedGestureEvent
      });
    }
}  
```

### 15. Keyboard avoidance mode

```typescript
// webKeyboardAvoidMode: keyboard avoidance; default WebKeyboardAvoidMode.RESIZE_VISUAL
MainPage({webKeyboardAvoidMode:WebKeyboardAvoidMode.RESIZE_VISUAL})
```

### 16. Custom HTTP headers

```typescript
/*
*     customHttpHeaders: custom HTTP headers
*     When withCredentials is true on the front end, add custom headers here if needed
*     See CORS FAQ
* isAllowCredentials: default false
*     If withCredentials is true, pass isAllowCredentials: true
*     See CORS FAQ
*/
MainPage({customHttpHeaders:"X-AUTH", isAllowCredentials:true})
```

### 17. Pass parent page via `parentPage` to access page fields from MainPage lifecycle

```typescript
// parentPage: pass this as WebView parent; usable from plugins
MainPage({parentPage:this})
```

## Hot Update

Capacitor does not officially support hot update. The OpenHarmony port adapts Cordova’s hot-code-push plugin for Capacitor; it is built in—no extra install.

### Prerequisites

Host two files on your server (generated with cordova-hot-code-push-cli; see [https://www.npmjs.com/package/cordova-hot-code-push-cli](https://www.npmjs.com/package/cordova-hot-code-push-cli)):

#### Install CLI (global)

```
npm install -g cordova-hot-code-push-cli
```

#### Run at project root

```
chcp init
```

#### Enter update server URL when prompted (e.g. https://www.example.com/chcp)

#### Build local web assets; generates chcp.json and chcp.manifest

```
chcp build
```

Under the `chcp` folder:

- **chcp.json**: version, content URL, etc.
- **chcp.manifest**: file list and hashes for verification.
- **www**: mirror `rawfile/www` layout for updated files; structure must match.

### Basic configuration

1. Edit `capacitor.config.json` and add:

```json
{
	"plugins": {
		"chcp": {
			"auto-download": true,
			"auto-install": true,
			"config-file": "http://www.example.com/chcp/chcp.json"
		}
	}
}
```

2. chcp.json sample

Keep one copy under local `rawfile/www` and one on the server.

- **release:** version string; CHCP compares with local to decide update.
- **content_url:** URL of updated files.
- **Other fields:** unused by CHCP for now.

```json
{
  "name": "capacitor",
  "autogenerated": true,
  "update": "now",
  "min_native_interface": 1,
  "content_url": "http://www.example.com/chcp/www",
  "release": "2025.03.05-16.47.30"
}
```

3. chcp.manifest

Local copy under `rawfile/www`; server copy next to `chcp.json`.

```json
[
  {
    "file": "assets/icon/favicon.png",
    "hash": "988be98f12b400c41a22b59b82cfeab1"
  }
]
```

4. JavaScript

Call from your app to perform hot update:

```javascript
function chcpUpdate() {
	// Optional new config URL; if omitted, uses www/chcp.json
	window.Capacitor.Plugins.HotCodePushPlugin.fetchUpdate({
		"config-file":"http://www.example.com/chcp/chcp.json" // Server config
	}).then(result => {
		if (result.action == 'chcp_updateIsReadyToInstall') {
			console.log('Update available');
			// After install, app restarts; wait >1 minute between updates for testing
			// For testing, bump release in www/chcp.json and hashes in www/chcp.manifest
			window.Capacitor.Plugins.HotCodePushPlugin.installUpdate().then(result2 => {
				console.log('Update installed');
			});
		}
		else {
			console.log('No update');
		}
	});
}
```

5. Flow

App start → fetch server `chcp.json` → compare versions → download diff → install update.

## Custom ArkTS Plugins

Custom ArkTS plugins reuse cordova-openharmony and follow the Capacitor SDK API. Example: `TestPlugin`.

### 1. New ArkTS file

Create `TestCapPlugin` with:

```typescript
import { CapacitorPlugin, PluginCall, NormalizeError, PluginMethod } from 'openHarmony-capacitor';

export class TestCapPlugin extends CapacitorPlugin {
  constructor() {
    super();
    try {
      this.registerMethod("testMethod", (call: PluginCall) => {
        this.testMethod(call);
      });
      this.registerMethod("notifyEvent", (call: PluginCall) => {
        this.notifyEvent(call);
      });
      this.registerMethod("removeEvent", (call: PluginCall) => {
        this.removeEvent(call);
      });
      this.registerPermission(["ohos.permission.LOCATION"]);
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to onWatchIndexPageUpdate. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  async testMethod(call: PluginCall): Promise<void> {
    let ret:object = new Object();
    ret["time"] = new Date().getTime().toString();
    call.resolve(ret);
  }

  notifyEvent(call:PluginCall):void {
    if(this.hasListeners("onDataReceived")) {
      let obj:object = new Object;
      obj["name"] = "value";
      this.notifyListeners("onDataReceived", obj);
    }

    let ret:object = new Object;
    ret["return"] = "success";
    call.resolve(ret);
  }

  removeEvent(call:PluginCall):void {
    this.removeEventListener("onDataReceived", call);
  }

  handleOnStart():void{
    console.log("handleOnStart");
  }
  
  handleOnPageStart():void {
    console.log("handleOnPageStart");
  }
  
  handleOnEnd():void{
    console.log("handleOnEnd");
  }

  handleOnResume():void{
    console.log("handleOnResume");
  }

  handleOnPause():void{
    console.log("handleOnPause");
  }

  handleOnDestroy():void{
    console.log("handleOnDestroy");
  }
}
```

### 2. Plugin registration

After implementing the plugin, register in `entry/src/main/ets/pages/index.ets`:

```typescript
import { MainPage, pageBackPress, pageHideEvent, pageShowEvent, PluginEntry} from 'openHarmony-capacitor';
import { TestCapPlugin } from '../plugins/TestCapPlugin';// Import plugin
struct Index {
    /*
    * ArkTS custom plugins: name + instance; see custom development section
    * If one plugin is used by multiple MainPages, use separate instances per MainPage
    */
  capacitorPlugins:Array<PluginEntry> = [
    {
      pluginName:"TestCapPlugin",
      pluginObject:new TestCapPlugin()
    }
  ]

    // ... other code omitted
 
    build() {
        RelativeContainer() {
          // isWebDebug: debug toggle; capacitorPlugins: custom plugins; default index.html
          MainPage({isWebDebug:false,capacitorPlugins:this.capacitorPlugins}); 
        }
        .height('50%')
        .width('100%')

        RelativeContainer() {
          // isWebDebug; capacitorPlugins; load rawfile path
          MainPage({isWebDebug:false,indexPage:"/www2/index.html", capacitorPlugins:this.capacitorPlugins}); 
        }
        .height('50%')
        .width('100%')
    }
}
```

### 3. Calling from JS

Follow Capacitor’s standard plugin API:

```javascript
let data = await window.Capacitor.Plugins.TestCapPlugin.testMethod({
    message: 'Exec testMethod'
})
```

### 4. How it works

OpenHarmony exposes ArkTS and C/C++ APIs; the Capacitor SDK is C/C++. Custom plugins cross language boundaries: JS → C/C++ → ArkTS for calls; callbacks reverse; ArkTS may also call JS directly. Choose ArkTS or C/C++ depending on the feature.

## Custom C++ Plugins

For C++ plugins, follow patterns from ported official Capacitor plugins.

### 1. Steps

1. In the integrated Capacitor source tree, create a folder under `cpp` for your plugin.
2. Add a class extending `Plugin` and a `CMakeLists.txt` for it.
3. In your `.cpp`, use `REGISTER_CAP_PLUGIN()` with your plugin name (e.g. CapacitorPlugin) to register the class.
4. Use `REGISTER_PLUGIN_METHOD()` for each method (e.g. PluginHello).
5. To call ArkTS from C++, use `executeArkTs` (sync) or `executeArkTsAsync` (async); see `Plugin` docs. Register ArkTS sources under `build-profile.json5` → `buildOption` → `arkOptions` → `runtimeOnly` → `sources`.
6. To return results from ArkTS to C++, ArkTS calls `onArkTsResult`; implement and register it on the C++ side too.
7. Add your `.cpp` files to `CMakeLists.txt` and build.

### 2. Configuration

In `rawfile/capacitor.plugins.json`, add package name and C++ class name:

```json
[
  {
    "pkg": "@capacitor/CapacitorPlugin",
    "classpath": "CapacitorPlugin"
  }
]
```

### 3. JS usage

```javascript
const result = await window.Capacitor.Plugins.CapacitorPlugin.PluginHello({
    message: 'Exec PluginHello'
});
```

## Web Loading Performance

### 1. Pre-warm WebView and pre-render

After app start, in `EntryAbility`, start the Web engine in the background and pre-render pages. When entering the page, it opens instantly; closing keeps the WebView alive for next open. Note: pre-rendering with Capacitor initializes plugins, which may access system resources before the user accepts the privacy policy.

You may wrap `MainPage` for this; contact the maintainer for sample code.

Reference: [https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization](https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization)

**1. Create `WebBuilder.ets` under pages and paste:**

```typescript
import { MainPage, MainPageCycle, PluginEntry } from 'openHarmony-capacitor';
import { BuilderNode, FrameNode, NodeController } from '@kit.ArkUI';
import { webview } from '@kit.ArkWeb';
import { TestPlugin } from '../plugins/TestPlugin';

// Extend with more fields as needed; same as MainPage props
class DataParameters{
  url?: string;
  mainPageCycle?:MainPageCycle;
  mainPagePageNodeController?:MainPagePageNodeController;
  cordovaPlugs?:Array< PluginEntry>;
}

@Builder
function buildMainPage(data:DataParameters) {
  Column() {
    MainPage({indexPage:data.url, lifeCycle:data.mainPageCycle, parentPage:data.mainPagePageNodeController,cordovaPlugs:data.cordovaPlugs});
  }.width("100%").height("100%")
}

let wrap = wrapBuilder< DataParameters[]>(buildMainPage);

class MainPagePageNodeController extends NodeController {
  private rootNode: BuilderNode< DataParameters[]> | null = null;
  private root: FrameNode | null = null;
  private cordovaPlugs:Array< PluginEntry> = [
      {
        pluginName: 'TestPlugin', // Plugin name
        pluginObject:new TestPlugin() // Plugin instance
      }
  ];
  private mainPageCycle:MainPageCycle = new MainPageCycle().setOnAboutToAppear((webviewController: webview.WebviewController,parentPage?:object)=>{
    let page = parentPage as MainPagePageNodeController;// page; pass this into MainPage
    console.log("exec onAboutToAppear");
  });

  constructor() {
    super();
  }

  makeNode(uiContext: UIContext): FrameNode | null {
    if (this.rootNode != null) {
      const parent = this.rootNode.getFrameNode()?.getParent();
      if (parent) {
        console.info(JSON.stringify(parent.getInspectorInfo()));
        parent.removeChild(this.rootNode.getFrameNode());
        this.root = null;
      }
      this.root = new FrameNode(uiContext);
      this.root.appendChild(this.rootNode.getFrameNode());
      return this.root;
    }
    return null;
  }

  initWeb(url:string, uiContext:UIContext) {
    if(this.rootNode != null) {
      return;
    }
    this.rootNode = new BuilderNode(uiContext);
    // Pass different params per page; single-page apps rarely need this
    if(url === "/www3/index.html") {
      this.rootNode.build(wrap, {url:url, mainPageCycle:this.mainPageCycle,mainPagePageNodeController:this, cordovaPlugs:this.cordovaPlugs});
    } else {
      this.rootNode.build(wrap, {url:url});
    }
  }
}

let NodeMap:Map< string, MainPagePageNodeController | undefined> = new Map();

export const createNWeb = (url: string, uiContext: UIContext) : MainPagePageNodeController | undefined => {
  let baseNode = new MainPagePageNodeController();
  baseNode.initWeb(url, uiContext);
  NodeMap.set(url, baseNode);
  return baseNode;
}

export const getNWeb = (url : string, uiContext:UIContext) : MainPagePageNodeController | undefined => {
  if(NodeMap.has(url)) {
    return NodeMap.get(url);
  } else {
    return createNWeb(url, uiContext);
  }
}
```

**2. Edit EntryAbility.ets — add pre-warm and pre-render:**

```typescript
// ... other code omitted
onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('pages/Splash', (err) => {
      // Pre-warm WebView and pre-render; multi-page can pre-init multiple URLs
      createNWeb('/www3/index.html', windowStage.getMainWindowSync().getUIContext());
      createNWeb('/www3/index2.html', windowStage.getMainWindowSync().getUIContext());
      createNWeb('/www3/index3.html', windowStage.getMainWindowSync().getUIContext());
    });
}
```

**3. In Index.ets, use the wrapped MainPage for instant display:**

```typescript
build() {
    Column() {
      RelativeContainer() {
        NodeContainer(getNWeb('/www3/index.html', this.getUIContext()))
          .height('100%')
          .width('100%')
      }
      .height('100%')
      .width('100%')
	}
}
```

### 2. JavaScript bytecode cache (Code Cache) via intercept/replace

With Capacitor, local pages and JS use interception/replace internally. For online JS forced through Capacitor’s stack (`capacitor.config.json` or `SetCordovaProtocolUrl`), resources are cached. For online pages on the WebView stack, use `onInterceptWebRequest` to replace online JS with local sandbox copies via `SetResourceReplace` for faster loads.

Reference: [https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization#section172031338172719](https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization#section172031338172719)

```typescript
// ... omitted — JS precompile sample
configs: Array< Config> = [
{
  url: 'https://www.tongecn.com/example.js',
  localPath: 'example.js',// File under rawfile
  options: {
    responseHeaders: [
      { headerKey: 'E-Tag', headerValue: 'xxx' },
      { headerKey: 'Last-Modified', headerValue: 'Web, 21 Mar 2024 10:38:41 GMT' }
    ]
  }
}]

mainPageCycle = new MainPageCycle().setOnControllerAttached((webviewController: webview.WebviewController,parentPage?:object)=>{
  console.log("exec onControllerAttached");
  for (const config of this.configs) {
      let content = await this.getUIContext().getHostContext()?.resourceManager.getRawFileContentSync(config.localPath);
      try {
        this.controller.precompileJavaScript(config.url, content, config.options)
          .then((errCode: number) => {
            console.log('precompile successfully!' );
          }).catch((errCode: number) => {
          console.error('precompile failed.' + errCode);
        })
      } catch (err) {
        console.error('precompile failed!.' + err.code + err.message);
      }
  }
})

// ... omitted — intercept/replace
onInterceptWebRequest(request: WebResourceRequest, webTag:string):ESObject {
    // WebView kernel replacement
    if(url == "https://www.tongecn.com/v1.1.1/temp/test3.js") {
      // Replace with sandbox path
      SetResourceReplace(webTag, url, "https://localhost/data/storage/el2/base/files/test.js");
      // Or rawfile
      //SetResourceReplace(webTag, url, "https://www.example.com/test.js");
    }
    return null;
}

// ... omitted
MainPage({isWebDebug:true, indexPage:"https://www.tongecn.com", lifeCycle:data.mainPageCycle, parentPage:this,onInterceptWebRequest:this.onInterceptWebRequest});
```

## FAQ

### 1. Back key not working

On OpenHarmony, the back gesture (swipe from left edge) may not navigate back or exit at root. Frameworks differ; if you only handle back in Capacitor, listen for the back button:

```javascript
document.addEventListener("deviceready", onDeviceReady, false);
function onDeviceReady() {
    document.addEventListener("backbutton", onBackKeyDown, false);
}

function onBackKeyDown() {
    // Handle back yourself
}
```

With Ionic Angular:

```javascript
function showConfirm() {
   // Exit app logic
}
$ionicPlatform.registerBackButtonAction(function (e) {
// Is there a page to go back to?
  if ($location.path() == '/tab/message') { // At root; adjust route for your app
      showConfirm();
      return false
  } else if ($ionicHistory.backView()) {
      // Go back in history
      $ionicHistory.goBack();
  } else {
      // This is the last page: Show confirmation popup
      showConfirm();
      return false;
  }
  e.preventDefault();
  return false;
}, 101);
```

You can always handle `backbutton` in Capacitor. If the page lacks the bridge JS, pass `MainPageOnBackPress` to control WebView navigation.

### 2. Accessing sandbox files

Use `cdvfile://`, e.g. for `downloadImage.png`:

```
cdvfile:///data/storage/el2/base/files/chuzhitong/downloadedImage.png
```

If the MainPage entry uses `file://`, you can also use `file://` for local assets (images png/jpg/svg, js, html, etc.). See the file:// section at the end.

### 3. HTTP cookies

For HTTP (not HTTPS):

**(1) Same-origin**

If you load `http://www.tongecn.com` in MainPage, Capacitor handles cookies automatically.

**(2) Cross-origin**

If files are in sandbox or rawfile and HTML uses HTTP GET/POST, add the HTTP domain under `harmony` in `capacitor.config.json` so Capacitor can attach cookies:

```json
{
  "harmony": {
    "cordova-protocol-force": ["***.***.com"]
  }
}
```

Set cookies at runtime in ArkTS; GET/POST carry cookies. Capacitor does not handle static assets for those; WebView does:

```typescript
// Runtime HTTP cookies in ArkTS; GET/POST auto-carry; static assets use WebView
aboutToAppear() {
    SetCordovaProtocolUrl("***.****.com");
}
```

**(3) HTTPS**

HTTPS requests get automatic cookie handling from Capacitor.

**(4) Manual cookies**

See the advanced cookies section for manual ArkTS cookie setup.

### 4. Virtual host www.example.com, custom host, localhost, file://, cdvfile://

When loading rawfile pages, DevTools or logs may show `https://www.example.com`. That is expected:

- OpenHarmony cannot load rawfile via `file://` directly; `www.example.com` is a virtual host standing in for `file://`. Treat `https://www.example.com` like `file://`.
- In `capacitor.config.json` under `harmony`, set `"Hostname": "app.com"` for a custom host.
- Use `cdvfile://` for sandbox files.

If your H5 uses `file://`, MainPage’s entry must use `file://` too, or local files won’t load. Capacitor supports `file://` for resources and sandbox paths on OpenHarmony.

### 5. Ignore system font size

- Add `configuration` in `app.json5` to ignore system font scaling: [https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/quick-start/app-configuration-file.md](https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/quick-start/app-configuration-file.md)
- In `EntryAbility.onWindowStageCreate`, call `windowStage.setDefaultDensityEnabled(true);` to ignore display scaling: [https://docs.openharmony.cn/pages/v5.0.3/zh-cn/application-dev/reference/apis-arkui/js-apis-window.md](https://docs.openharmony.cn/pages/v5.0.3/zh-cn/application-dev/reference/apis-arkui/js-apis-window.md)

### 6. CORS errors

Capacitor handles cross-origin access, cookies, and custom headers by default. If `withCredentials` is true, configure accordingly.

For POST/GET with `withCredentials` and custom headers like `X-Auth-Token`, `Test-Type`, pass:

```typescript
MainPage({customHttpHeaders:"X-Auth-Token,Test-Type",isAllowCredentials:true})
```

**If misconfigured, you may see:**

```
Access to XMLHttpRequest at '*****' from origin '****' has been blocked by CORS policy: The value of the 'Access-Control-Allow-Origin' header in the response must not be the wildcard '*' when the request's credentials mode is 'include'. The credentials mode of requests initiated by the XMLHttpRequest is controlled by the withCredentials attribute.
```

**Fix:** add `isAllowCredentials:true` on MainPage.

```typescript
MainPage({isAllowCredentials:true})
```

```
Access to XMLHttpRequest at '*****' from origin '*****' has been blocked by CORS policy: Request header field ***** is not allowed by Access-Control-Allow-Headers in preflight response
```

**Fix:** add custom headers, e.g. `X-Auth-Token`, `Test-Type`.

```typescript
MainPage({customHttpHeaders:"X-Auth-Token,Test-Type", isAllowCredentials:true})
```

### 7. iframe cookies (cross-site)

If an iframe loads a third-party page that sets cookies via JS (not `Set-Cookie`), add `SameSite=None; Secure` or the iframe may not display because cookies won’t attach to requests:

```javascript
document.cookie = 'token=467d1510-xxxx-xxxx-xxxx-73852620effa1; path=/; SameSite=None; Secure';
```

If you cannot change the third party, open in the in-app browser or an `<a target="_blank">` link—on OpenHarmony Capacitor, `<a target="_blank">` opens the in-app browser (Android/iOS may differ).

```javascript
// In-app browser; configure options; requires in-app browser plugin
window.open("https://www.*****.com/index.html", "title=Test title");
```

```html
<!-- target=_blank opens in-app browser -->
<a href="https://www.*****.com/index.html" target="_blank">Open link</a>
```

### 8. Capacitor internal cache duration

Default static cache when using Capacitor’s stack is one day (`24 * 60 * 60` seconds). To customize, add in `capacitor.config.json`:

```json
{
  "harmony": {
    "cordova-cache-duration":60
  }
}
```

## Directory Structure

```
Project root/
└─src
    ├─main
    │  ├─cpp
    │  │  ├─CoreHarmony // OpenHarmony adapter layer
    │  │  ├─getcapacitor // Capacitor adapter layer
    │  │  ├─HotCodePushPlugin // Hot update
    │  │  ├─TsCordovaPlugin // ArkTS Cordova plugin
    │  │  └─types
    │  │      └─libcapacitor
    │  ├─ets
    │  │  └─components
    │  │      ├─AlertDialog // Dialog
    │  │      ├─CoreHarmony // OpenHarmony adapter layer
    │  │      ├─getcapacitor // Capacitor adapter layer
    │  │      ├─ImageCompress // Image processing
    │  │      ├─InAppBrowser // In-app browser
    │  │      ├─Permission // Permission handling
    │  │      ├─PluginAction // Plugin utilities
    │  │      └─SplashScreen // Splash screen
    │  └─resources // Resources
    ├─ohosTest // Tests
    │  └─ets
    │      └─test
    └─test // Tests
```

## Contributing

File [issues](https://gitcode.com/CPF-Ionic/openHarmony-capacitor/issues) or open [pull requests](https://gitcode.com/CPF-Ionic/openHarmony-capacitor/pulls).

## License

This project is released under the **MIT License**; see [LICENSE](LICENSE).
