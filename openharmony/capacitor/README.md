# <center>capacitor</center>
[![zh-CN](https://img.shields.io/badge/lang-中文-blue.svg)](README.md)
[![en](https://img.shields.io/badge/lang-English-blue.svg)](README.en.md)

本项目基于 [@capacitor/android@8.0.0](https://www.npmjs.com/package/@capacitor/android) 开发。

## 简介

openHarmony-capacitor 是 capacitor 的 OpenHarmony 化版本，所有接口兼容 capacitor 的 Android 和 iOS 版本，本文档仅说明 openHarmony-capacitor 框架部分的使用手册、开发说明、集成步骤等。

## 支持平台

- **OpenHarmony**：5.0+

## 依赖说明

本工程依赖 openssl，官方网站 [https://openssl.org](https://openssl.org/)，编译之前先集成 openssl，只有成功集成后，才可编译，集成方法：[https://gitcode.com/li_in/openharmony-capacitor-openssl3.5](https://gitcode.com/li_in/openharmony-capacitor-openssl3.5)。

## 开发说明

openHarmony-capacitor 是 capacitor 的 OpenHarmony 化版本，并支持 ArkTS 侧和 C/C++ 侧自定义插件研发，框架采用 C/C++ 研发，底层使用自研 Socket TCP/IP 通讯，封装了 HTTP/HTTPS 协议通讯解决各种跨域访问问题，无需配置 web 服务端，同时结合 webview 的通讯协议栈，大大提高应用层网络请求效率。

## 附加说明

openHarmony-capacitor 使用多页面视图研发，同时兼容 Android 和 iOS 原有的单页面视图，原有项目可以轻松移植；另外在复杂项目中，可以使用 openHarmony-capacitor 的多页面视图功能，创建多个 webview 协同工作。

## 开发背景

capacitor 官方网站：[https://capacitorjs.com](https://capacitorjs.com)，是移动端跨平台的新框架，大量厂商直接或间接采用此框架开发 APP；但是目前不支持 OpenHarmony 版本，开发者将原 Android 和 iOS 项目移植到 OpenHarmony 版，无法适配，为此研发了 openHarmony-capacitor，遵守 capacitor 官方标准，原有项目无需投入任何研发轻松移植到 OpenHarmony 系统；新开发的项目，一次研发就适用于 Android、iOS 和 OpenHarmony 三大平台，也节省了大量的时间和人力成本。

本框架为 cordova-openharmony 框架的升级版本，沿用了部分 cordova-openharmony 框架的能力，在插件和通信部分针对 capacitor 做了优化，部分功能仍沿用 cordova-openharmony 框架。

## 兼容性

在以下版本中已测试通过：

1. SDK: 5.0.5(17); IDE: DevEco Studio: 6.0.0; ROM: 5.1.0.150;

## 使用说明

### 1. 创建项目

打开 DevEco 创建项目，选择 Empty Ability 进入下一步 (next)，填写必要信息，点击完成 (finish)，工程创建完成。

### 2. 集成源码

下载本工程，放入主工程文件夹中，此时在 DevEco 中已经可以看到 capacitor 的模块工程了。

### 3. 引入依赖

在根目录的 oh-package.json 中引入依赖：
```json
{
  "dependencies": {
    "openHarmony-capacitor": "file:./capacitor"
  }
}
```

然后再修改 build-profile.json5（项目级）配置文件，在 modules 模块中增加：

```json5
{
    "name": "capacitor",
    "srcPath": "./capacitor",
}
```

以上三步操作后，已经在主工程中集成了 capacitor 的源码了。

### 4. 项目移植

**前端工程打包**

为提升项目部署灵活性，需将前端工程的资源引用及路由跳转逻辑从「绝对根路径依赖」调整为「相对路径引用」：

调整页面基准路径：

1. 在项目根目录的 index.html 文件中，将 `<base>` 标签的 href 属性由默认的绝对根路径 `/` 修改为相对路径 `./`，作为页面所有相对路径资源的基准锚点；
2. 配置打包输出路径：如在 Vue 工程中，将 vue.config.js 配置文件中，控制 Webpack 静态资源打包路径的核心属性 publicPath 由默认的 `/` 调整为 `./`，确保打包后 JS、CSS、图片等静态资源均采用相对路径引用。

**Android 项目移植：**

复制原有 Android studio 的工程 assets 目录下面的所有文件到 OpenHarmony 工程 entry/src/main/resources/rawfile 目录下，原 Android 工程的 assets 目录包含 config.xml（如果有）、capacitor.config.json（必须）、capacitor.plugins.json（必须）和 dist 目录，将 dist 文件夹名修改为 www，www 目录包含 index.html（必须）、cordova.js（如果有）、cordova_plugins.js（如果有）、css 目录、js 目录等，如果要指定加载页面，不使用默认页面，请查看高级功能部分说明。复制成功后，仍需要安装 Android 包含的 OpenHarmony 版插件。

**iOS 项目移植：**

第一步：复制原有 iOS App 目录下的 public 文件夹到 OpenHarmony 工程 entry/src/main/resources/rawfile 目录下，将 public 文件夹名修改为 www，文件包含：index.html（必须）、cordova.js（如果有）、cordova_plugins.js（如果有）、css 目录、js 目录等。

第二步：Xcode 工程的配置文件在 App 目录下，Xcode 工程的该文件不能直接被 openHarmony-capacitor 使用，需要进行转换，该文件主要记录的是框架配置信息、插件的名称和初始化的类，因为 OpenHarmony 版是根据 Android 的配置文件进行插件初始化的，因此需要将 Xcode 工程配置文件转为 Android 的配置文件，请将 Xcode 工程使用 node 加入 Android 平台，系统会自动生成 Android 版的 config.xml（如果有）、capacitor.config.json（必须）、capacitor.plugins.json（必须）。然后将文件复制到 OpenHarmony 版工程的 entry/src/main/resources/rawfile 下。复制成功后，仍需要安装 iOS 包含的 OpenHarmony 版插件。

**新建项目：**

如果您没有 Android 和 iOS 项目，需要使用 capacitor 的框架，创建 Android 项目，创建成功后，再按照 Android 项目移植方法操作即可。

**添加配置：**

在 capacitor.config.json 中添加 harmony 属性，在此可配置自定义配置。

```json
{
  "appId": "appId",
  "appName": "appName",
  "webDir": "www",
  "harmony": {
    // 自定义配置。
  }
}
```

**特殊使用：**

如果是非 capacitor，或者不涉及插件等需要进行原生通信功能，仅仅用于 UI 渲染展示，可以在 MainPages 设置 `isInjectBridgeJs:false`，不进行 native-bridge.js 的注入，从而提高加载速度。

### 5. 修改 Index.ets 文件

打开 OpenHarmony 工程文件 entry/src/main/ets/pages/Index.ets 文件，修改代码如下（可以直接全部拷贝到 Index.ets 文件中）：

```typescript
import { MainPage, pageBackPress, pageHideEvent, pageShowEvent, PluginEntry, MainPageOnBackPress} from 'openHarmony-capacitor';
//import { TestPlugin } from "../plugins/TestPlugin" //自定义插件TestPlugin，根据实际情况导入自己的自定义插件
@Entry
@Component
struct Index {
  //ArkTs侧的自定义插件：配置插件名称和对象，请查看自定义开发部分
  cordovaPlugs: Array<PluginEntry> = [];
  mainPageOnBackPress: MainPageOnBackPress = new MainPageOnBackPress();
  /*
  cordovaPlugs: Array<PluginEntry> =
  [
      {
        pluginName: 'TestPlugin', //插件名称
        pluginObject: new TestPlugin() //实例化插件对象供框架调用
      }
  ];
  */
  onPageShow() {
    pageShowEvent(); //页面显示通知框架
  }
  onBackPress() {
    pageBackPress(); //拦截返回键由框架处理
    return this.mainPageOnBackPress.backPress();
  }
  onPageHide() {
    pageHideEvent(); //页面隐藏通知框架
  }
  build() {
    RelativeContainer() {
      //默认加载rawfile/www/index.html
      //如果要指定加载页面参考高级功能部分
      MainPage({ isWebDebug: false, cordovaPlugs: this.cordovaPlugs });
    }
    .height('100%')
    .width('100%')
  }
}
```

### 6. 修改 EntryAbility.ets 文件

打开 OpenHarmony 工程文件 entry/src/main/ets/entryAbility/EntryAbility.ets 文件，修改 onCreate 函数如下：

```typescript
import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { webview } from '@kit.ArkWeb';  //引入webview

//省略部分代码
onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
   hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onCreate');
   webview.WebviewController.initializeWebEngine();//webview引擎初始化
}
```

### 7. 完成

做以上代码修改后，OpenHarmony 的移植已经完毕，可以使用模拟器或者真机进行编译和测试了。

## 高级用法（区别于 Android 和 iOS）

### 1. MainPage 传入 indexPage 参数设置自定义启动路径，支持 rawfile、resfile 和沙箱路径

```typescript
/*
*    indexPage:默认启动首页，举例如下:
*    "/www/index.html"：rawfile目录下的文件
*    "/data/storage/el2/base/files/www/index.html"：使用虚拟域名www.example.com加载沙箱路径下的文件，
*    "https://cn.bing.com":加载在线网页，必须指定https或者http
*    "file:///data/storage/el2/base/files/www/index.html":file协议加载el2级别沙箱路径文件
*    "file:///data/storage/el1/bundle/entry/resources/resfile/www/index.html":file协议加载el1级别沙箱路径文件
*    "file://" + getContext().resourceDir + "/www/index.html":file协议加载el1级别沙箱路径文件
*    capacitor支持使用虚拟域名www.example.com加载本地文件，也支持使用file协议加载本地文件
*    改变this.indexPage的值，webview会重新加载页面
*/
//省略其它代码
MainPage({indexPage:"/www/index.html"});
//省略其它代码
```

### 2. 注册自定义用户 scheme，用于 capacitor 内部拦截 scheme 的请求

```typescript
import { RegisterCustomSchemes } from 'openHarmony-capacitor';
onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
   hilog.info(0x0000, 'testTag', '%{public}s', 'Ability onCreate');
   RegisterCustomSchemes("cmp"); //注册自定义scheme
   webview.WebviewController.initializeWebEngine();//webview引擎初始化
}
```

```typescript
//customSchemes:自定义scheme，多个scheme用","分隔
//省略其它代码
MainPage({customSchemes:"cmp,xmp,xxx"});
//省略其它代码
```

### 3. 拦截自定义的 scheme，在 webview 端拦截并处理，也可以拦截 http(s) 请求处理

```typescript
/*
*拦截请求函数，根据需要拦截相应请求，一般用于自定义scheme，如果存在自定义scheme的必须使用此函数拦截处理
*使用此函数拦截自己的scheme进行处理，也可以在MainPage生命周期回调函数中拦截处理，二选一，不能同时拦截处理。
*拦截后处理有两种方式，推荐使用第一种方式
*   1. capacitor webview内核处理，返回null，capacitor可以处理替换所有资源，例如在线资源，本地资源，js、img、css等
*   2. 自己处理，返回WebResourceResponse
*说明如下：
*   1. 子组件的回调函数不能使用this指针，如果要使用this，请参考parentPage参数
*   2. 采用第一种方式，写法简单，且效率高，推荐第一种方式
*/
onInterceptWebRequest(request: WebResourceRequest, webTag: string): ESObject {
  let url = request.getRequestUrl();
  //capacitor webview内核处理替换
  if (url == "cmp://v1.1.1/temp/test2.png") {
    /*
    *替换资源说明如下：
    *本地资源请使用https://www.example.com的虚拟域名作为访问本地资源的标记
    *详细了解www.example.com内置虚拟域名规则，查看最后面的常见问题说明
    *被替换和替换内容可以是图片、css、js等
    *替换资源举例如下：
    *1. 沙箱路径
    *   https://www.example.com/data/storage/el2/base/files/test.png
    *2. rawfile目录的下的资源文件
    *   https://www.example.com/www/test.png
    *3. 网络在线资源
    *   https://www.chuzhitong.com/images/logo.png
    *4. cdvfile协议的沙箱路径的文件，绝对路径
    *   cdvfile:///data/storage/el2/base/files/test.png
    *此函数是通知capacitor webview内核，后续加载页面实施资源替换
    */
    SetResourceReplace(webTag, url, "https://www.chuzhitong.com/images/logo.png");
  }

  //自己处理资源返回webview
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

//省略其它代码
/*
*onInterceptWebRequest 返回null放行，返回具体的WebResourceResponse
*
*/
MainPage({onInterceptWebRequest: this.onInterceptWebRequest});
//省略其它代码
```

### 4. 在原生层，动态设置 webview 属性

```typescript
/*
*在原生层页面加载后，往页面中注入新的js，也可以在mainPage的生命周期页面加载完毕后注入js
*/
onSetCordovaWebAttribute(cordovaWebView: CordovaWebView) {
    if(cordovaWebView) {
    //获取webview属性变量，用于动态修改webview属性，具体参考如下连接，页面加载完成后触发
    //OpenHarmony并不支持WebAttribute组件属性的动态设置，但是可以设置部分属性，不支持的属性会抛出"Method not implemented."、"is not callable"等异常信息
    //https://docs.openharmony.cn/pages/v5.1.0/zh-cn/application-dev/reference/apis-arkweb/js-apis-webview.md
    //https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/reference/apis-arkui/arkui-ts/ts-universal-attributes-attribute-modifier.md
    cordovaWebView!.getWebAttribute()?.height('50%');
    //获取webview的控制变量，用于实现具体的功能，示例代码实现在webview执行js或者注入新的js，具体参考如下连接
    //https://docs.openharmony.cn/pages/v5.1.0/zh-cn/application-dev/reference/apis-arkweb/ts-basic-components-web.md
    cordovaWebView!.getWebviewController().runJavaScript("alert(1);");
  }
}
//省略其它代码
MainPage({onSetCordovaWebAttribute: this.onSetCordovaWebAttribute});
//省略其它代码
```

### 5. 多 webview 界面，即多页面视图，自定义 webId，使用自定义插件各 webview 之间通讯，可用于平板等大屏幕研发需求

```typescript
//省略其它代码
//webId:自定义webId，用于多webview，各webview之间通讯，webId确保唯一，参考自定义插件研发示例代码
MainPage({ webId:"123456"})
//省略其它代码
```

### 6. 动态创建组件，在 webview 和 NodeController 相结合实现动态创建和显示组件时，切记一定要设定 webId 参数，避免重复创建 webview

```typescript
//动态创建MainPage的示例代码，主要用于原生界面和webview界面显示在同一个视图里面的混合式研发
//如果要传入其它参数，参考此文档详细了解
//https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/reference/apis-arkui/arkui-js/js-components-create-elements.md
@Builder
function buildMainPage() {
  Column() {
    //直接加载在线网站
    MainPage({webId: "123456", indexPage: "https://cn.bing.com", cordovaPlugs: [
      {
        pluginName: 'TestPlugin', //插件名称
        pluginObject: new TestPlugin() //实例化插件对象
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
    // 创建BuilderNode实例
    this.textNode = new BuilderNode(context);
    this.textNode.build(wrapBuilder<[]>(buildMainPage));
    // 返回需要显示的节点
    return this.textNode.getFrameNode();
  }
}
```

```typescript
//省略其它代码
private textNodeController = new TextNodeController();

//省略其它代码
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

Button("显示和隐藏web").onClick(()=>{
  this.isShow = false;
})
```

### 7. W3C WEB 授权 webview 权限，例如 webview 调起摄像头和麦克风

```typescript
//Web组件可以通过W3C标准协议授权回调函数，例如拉起摄像头和麦克风，示例如下
onPermissionRequest(event:OnPermissionRequestEvent, parentPage?: object){
  let page = parentPage as Index;//page为当前页面对象，相当于当前页面的this指针，使用该对象，必须将this指针传入到mainPage中
  if (event) {
    //拉起摄像头和麦克风，为确保用户拒绝后能二次拉起授权，需要多个授权时，单独分开授权
    //单独分开授权会多次弹出窗口，仅供参考，也可以一次授权多个权限，但是用户拒绝后，无法拉起二次授权窗口
    //授权摄像头和麦克风，弹窗授权
    //const yourPermissions: Array< Permissions> = ['ohos.permission.CAMERA', 'ohos.permission.MICROPHONE'];
    //授权加速度和陀螺仪，无弹窗用户无感知
    const yourPermissions: Array< Permissions> = ['ohos.permission.ACCELEROMETER', 'ohos.permission.GYROSCOPE'];
    for (let i = 0; i < yourPermissions.length; i++) {
      let confirmPermissions: Array< Permissions> = [yourPermissions[i]];
      let atManager = abilityAccessCtrl.createAtManager();
      atManager.requestPermissionsFromUser(getContext(this), confirmPermissions).then((data) => {
        let grantStatus: Array< number> = data.authResults;
        if (grantStatus[0] != 0) {
          // 用户拒绝授权，提示用户必须授权才能访问当前页面的功能，并引导用户到系统设置中打开相应的权限
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
*onPermissionRequest:web组件W3C标准拉起授权的回调函数
*    参考连接:https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/web/web-rtc.md
*/
MainPage({parentPage: this, onPermissionRequest: this.onPermissionRequest,});
```

### 8. 父组件感知 MainPage 子组件的所有生命周期，在不同的周期执行相应的操作

```typescript
//MainPage的生命周期的各回调函数，根据业务需要设置单个或多个生命周期回调函数添加业务功能
//生命周期的说明参考：https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/web/web-event-sequence.md
mainPageCycle?: MainPageCycle;
aboutToAppear() {
    this.mainPageCycle = new MainPageCycle()
        .setOnAboutToAppear((webviewController: webview.WebviewController, parentPage?: object)=>{
          //page为当前页面对象，相当于当前页面的this指针，使用该对象，必须将this指针通过parentPage参数传入mainPage中
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
//省略其它代码
/*
*lifeCycle:传入生命周期对象，让父组件感知MainPage的生命周期，进行相应业务处理
*parentPage:传入this，就是webview父组件对象，也就是当前组件的对象，可以在插件里面调用
*/
MainPage({lifeCycle: this.mainPageCycle, parentPage: this})
//省略其它代码
```

### 9. 在同一个 Page 中加载多个 webview，实现本地、在线页面混合研发

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

### 10. 加载不包含 cordova.js 和 capacitor.js 页面，父组件控制 webview 的返回键，或者自己控制路由

```typescript
/*
*控制mainPage的页面返回，需将此对象传入MainPage
*如果加载的页面不包含cordova.js和capacitor.js，使用pageBackPress无法通知capacitor返回，必须使用此对象控制页面返回
*也可以通过此对象控制webview的路由
*/
mainPageOnBackPress: MainPageOnBackPress = new MainPageOnBackPress();

onBackPress() {
	pageBackPress();
    /*
     *如果加载的页面没有包含cordova.js和capacitor.js，例如加载https://cn.bing.com，
     * 返回值
     *  true:已经到了页面顶层
     *  false:返回了上一页
     */
    //return this.mainPageOnBackPress.backPress();
	return true;
}
//backPress:传入控制webview路由的对象，加载的页面不包含cordova.js和capacitor.js时控制webview路由，需要传
MainPage({ backPress: this.mainPageOnBackPress })
```

### 11. MainPage 的路由开关控制，便于 MainPage 嵌套使用，路由子原生页面内再嵌套使用 MainPage

```typescript
/*
*isNavPath：true使用MainPage组件内的路由，默认是true，false:不使用MainPage内的路由，
*     特别是MainPage嵌套使用时，父组件要打开路由，子组件关闭路由，否则会路由冲突
*/
MainPage({isNavPath:false});
```

### 12. 自定义 cookie，传入 cookie 键值对

```typescript
//手动添加cookie，在发送POST或者Get请求时携带cookie，https的session cookie无需手动设置，cordova会自动处理
//http的session cookie参考最后的https的cookie说明
this.cookies.set("https://mem.tongecn.com", ["key1=value1; path=/; Domain=.tongecn.com", "key2=value2"]);

/*
*cookies:如果ArkTs侧有自定义的cookie，可以通过此参数传入
*     一般情况下cookie都是cordova自动处理的，无需ArkTS侧手动设置，不过ArkTS侧通过此参数可以手动设置cookie
*     如果您的请求是采用的http协议非https，分为跨域请求和非跨域请求，请查看最后的常见问题说明
*/
MainPage({cookies: this.cookies});
```

### 13. 自定义 webview 字体大小缩放百分比，支持适老化，屏蔽跟随系统字体大小变化

```typescript
/*
*     textZoomRatio:webview字体放大缩小百分比，默认是100保持默认
*     设置webview不跟随系统字体大小、并且屏蔽跟随显示大小缩放后
*     可以通过此参数统一设置webview字体大小变化百分比，避免页面错乱
*     也可以通过Device插件增加的字体大小百分比接口函数，在js侧设置，参考Device插件
*     参考常见问题屏蔽跟随系统字体大小和屏蔽跟随显示大小缩放
*/
MainPage({textZoomRatio:110});
```

### 14. 同层渲染，以及同层渲染组件和插件结合的使用的方法

```typescript
//同层渲染示例代码，H5页面增加一个原生的TextInput组件
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
            this.params.onTextChange(value); // 触发回调
          }
        })
    }
    //自定义组件中的最外层容器组件宽高应该为同层标签的宽高
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

  // 必须要重写的方法，用于构建节点数、返回节点数挂载在对应NodeContainer中。
  makeNode(uiContext: UIContext): FrameNode | null {
    if (this.isDestroy_) { // rootNode为null
      return null;
    }
    if (!this.rootNode) {// rootNode 为undefined时
      this.rootNode = new BuilderNode(uiContext, { surfaceId: this.surfaceId_, type: this.renderType_ });
      if(this.rootNode) {
        this.rootNode.build(wrapBuilder(TextInputBuilder),
            {textOne: "myTextInput", width: this.width_, height: this.height_, onTextChange:(value:string)=>{
         //TextInput值改变后，通知js侧，这里只是列举了一个简单的例子，以实际情况执行js代码
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
    //省略其它代码
    public nodeControllerMap: Map<string, MyNodeController> = new Map();
    @State componentIdArr: Array<string> = [];
    @State widthMap: Map<string, number> = new Map();
    @State heightMap: Map<string, number> = new Map();
    @State positionMap: Map<string, Edges> = new Map();
    @State edges: Edges = {};
    @State textValue:string = "hello";
    /*
    *同层渲染生命周期回调函数
    */
    onNativeEmbedLifecycleChange(embed: NativeEmbedDataInfo,cordovaWebView:CordovaWebView,parentPage?:object) {
      let page = parentPage as Index;//page为当前页面对象，相当于当前页面的this指针，使用该对象，必须将this指针传入到mainPage中
      console.log("NativeEmbed surfaceId" + embed.surfaceId);
      // 如果使用embed.info.id作为映射nodeController的key，请在h5页面显式指定id
      const componentId = embed.info?.id?.toString() as string
      if (embed.status == NativeEmbedStatus.CREATE) {
        console.log("NativeEmbed create" + JSON.stringify(embed.info));
        // 创建节点控制器、设置参数并rebuild
        let nodeController = new MyNodeController()
        // embed.info.width和embed.info.height单位是px格式，需要转换成ets侧的默认单位vp
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
        //根据web传入的embed的id属性作为key，将nodeController存入Map
        page.nodeControllerMap.set(componentId, nodeController);
        page.widthMap.set(componentId, cordovaWebView.getUIContext().px2vp(embed.info?.width));
        page.heightMap.set(componentId, cordovaWebView.getUIContext().px2vp(embed.info?.height));
        page.positionMap.set(componentId, page.edges);
        // 将web传入的embed的id属性存入@State状态数组变量中，用于动态创建nodeContainer节点容器，需要将push动作放在set之后
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
      let page = parentPage as Index;//page为当前页面对象，相当于当前页面的this指针，使用该对象，必须将this指针传入到mainPage中
      console.log("NativeEmbed onNativeEmbedGestureEvent" + JSON.stringify(touch.touchEvent));
      page.componentIdArr.forEach((componentId: string) => {
        let nodeController = page.nodeControllerMap.get(componentId);
        // 将获取到的同层区域的事件发送到该区域embedId对应的nodeController上
        if(nodeController?.getEmbedId() == touch.embedId) {
          let ret = nodeController?.postEvent(touch.touchEvent)
          if(ret) {
            console.log("onNativeEmbedGestureEvent success " + componentId);
          } else {
            console.log("onNativeEmbedGestureEvent fail " + componentId);
          }
          if(touch.result) {
            // 通知Web组件手势事件消费结果
            touch.result.setGestureEventResult(ret);
          }
        }
      })
    }

    /*
    *同层渲染的TextInput文本改变后回调该函数
    *可以通过自定义插件获取改变后的值
    */
    onTextChangeCallBack(page:Index, value:string) {
      page.textValue = value;
    }
    
    getTextValue():string {
      return this.textValue;
    }
    
    /*
    *设置同层渲染TextInput的显示文本
    *可以通过自定义插件设置TextInput的显示文本
    */
    setNativeValue(id:string, value:string){
      this.textValue = value;
      let nodeController = this.nodeControllerMap.get(id);
      nodeController?.updateNode({page: this, textOne: 'update', width: this.widthMap.get(id), height: this.heightMap.get(id), text: this.textValue, onTextChange:this.onTextChangeCallBack} as ESObject)
    }

  RelativeContainer() {
      //同层渲染
      ForEach(this.componentIdArr, (componentId: string) => {
        NodeContainer(this.nodeControllerMap.get(componentId))
          .position(this.positionMap.get(componentId))
          .width(this.widthMap.get(componentId))
          .height(this.heightMap.get(componentId))
      }, (embedId: string) => embedId)
      /*
       *nativeEmbedHtmlTag:注册同层渲染标签
       *     默认是：<embed>的标签，如果要注册object，请传入object，同层渲染只支持这两个标签，可以直接保持默认
       *nativeEmbedHtmlType:注册同层选择标签类型
       *     默认是：native类型，如要要传入其它类型，请随意取名字
       *onNativeEmbedLifecycleChange:同层渲染元素生命周期函数
       *onNativeEmbedGestureEvent:同层渲染手势回调函数
       *    同层渲染参考连接：https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/web/web-same-layer.md
       */
      MainPage({
        parentPage: this,
        onNativeEmbedLifecycleChange: this.onNativeEmbedLifecycleChange,
        onNativeEmbedGestureEvent: this.onNativeEmbedGestureEvent
      });
    }
}  
```

### 15. 键盘避让模式

```typescript
//webKeyboardAvoidMode:避让键盘模式，默认：WebKeyboardAvoidMode.RESIZE_VISUAL
MainPage({webKeyboardAvoidMode:WebKeyboardAvoidMode.RESIZE_VISUAL})
```

### 16. 自定义 http 头

```typescript
/*
*     customHttpHeaders:自定义http头
*     前端withCredentials为true时添加自定义http头，没有自定义http头不用添加
*     参考常见问题的跨域说明
*isAllowCredentials:默认是false
*     前端请求设置withCredentials为true，要传入参数isAllowCredentials:true
*     参考常见问题的跨域说明
*/
MainPage({customHttpHeaders:"X-AUTH", isAllowCredentials:true})
```

### 17. 传入当前页面对象 parentPage，在 mainPage 的生命周期函数中可以引用当前页面的变量

```typescript
//parentPage:传入this，就是webview父组件对象，也就是当前组件的对象，可以在插件里面调用
MainPage({parentPage:this})
```

## 热更新

capacitor 原框架官方不支持热更新，OpenHarmony 化框架基于 Cordova 官方热更新插件功能，改造成适配 capacitor 框架，框架自带，无需安装。

### 前置条件

在服务器上放置两个文件（通过 cordova-hot-code-push-cli 命令生成，参考：[https://www.npmjs.com/package/cordova-hot-code-push-cli](https://www.npmjs.com/package/cordova-hot-code-push-cli)）：

#### 安装 CLI 工具（全局）

```
npm install -g cordova-hot-code-push-cli
```

#### 在项目根目录执行

```
chcp init
```

#### 根据提示输入更新服务器地址（如 https://www.example.com/chcp）

#### 构建本地 Web 资源，生成 chcp.json 与 chcp.manifest

```
chcp build
```

在 chcp 目录下：

- **chcp.json**：定义版本、更新内容 URL 等。
- **chcp.manifest**：列出所有文件及其哈希值（用于校验）。
- **www** 目录：放置结构与工程中 rawfile/www 中的更新文件，需要与原结构保持一致。

### 基本配置步骤

1. 修改 capacitor.config.json，增加如下配置。

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

2. chcp.json 配置文件示例

该文件在本地 rawfile/www 目录下存放一份，然后在服务器存储一份。

- **release:** 版本号，chcp 会判断该版本号和本地版本号比较，判断是否要更新；
- **content_url:** 更新文件的存储位置；
- **其它参数:** 其它参数 chcp 暂不使用；

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

3. chcp.manifest 配置文件

该文件在本地 rawfile/www 目录下存放一份，然后在服务器存储一份，服务端存储在 chcp.json 项目目录内。

```json
[
  {
    "file": "assets/icon/favicon.png",
    "hash": "988be98f12b400c41a22b59b82cfeab1"
  }
]
```

4. js 代码部分

在本地工程中调用以下代码，实现热更新功能。

```javascript
function chcpUpdate() {
	//配置新的更新地址，如果不传option，更新地址使用www/chcp.json配置的地址
	window.Capacitor.Plugins.HotCodePushPlugin.fetchUpdate({
		"config-file":"http://www.example.com/chcp/chcp.json" // 服务端配置信息
	}).then(result => {
		if (result.action == 'chcp_updateIsReadyToInstall') {
			console.log('插件有更新');
			//检测到更新，更新成功后会自动重启app，每次更新间隔周期需大于1分钟
			//如果要进行测试，修改www/chcp.json的release版本号，修改www/chcp.manifest文件内，其中文件对应的md5值
			window.Capacitor.Plugins.HotCodePushPlugin.installUpdate().then(result2 => {
				console.log('更新完成');
			});
		}
		else {
			console.log('插件无更新');
		}
	});
}
```

5. 更新流程

应用启动 → 检查服务器 chcp.json → 对比版本 → 下载差异文件 → 安装更新。

## 自定义 ArkTS 插件研发

自定义 ArkTS 插件研发复用了 cordova-openharmony 能力进行实现，自定义插件接口遵守 capacitor sdk 官方规范，以自定义插件 TestPlugin 为例：

### 1. 新建 ArkTS 文件

新建 ArkTS 文件，命名为 TestCapPlugin，示例代码如下。

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

### 2. 插件的配置

ArkTS 侧插件写好以后，在 entry/src/main/ets/pages/index.ets 文件中配置：

```typescript
import { MainPage, pageBackPress, pageHideEvent, pageShowEvent, PluginEntry} from 'openHarmony-capacitor';
import { TestCapPlugin } from '../plugins/TestCapPlugin';//引入插件
struct Index {
    /*
    *ArkTs侧的自定义插件键值对：插件名称和实现对象，自定义插件开发，请查看自定义开发部分
    *如果一个插件传入多个MainPage，务必单独定义对象传入，不可多MainPage使用一个对象，否则会使窗口操作串联
    */
  capacitorPlugins:Array<PluginEntry> = [
    {
      pluginName:"TestCapPlugin",
      pluginObject:new TestCapPlugin()
    }
  ]

    //省略其它代码
 
    build() {
        RelativeContainer() {
          //isWebDebug:工具调试开关，capacitorPlugins：自定义插件列表，启动首页index.html
          MainPage({isWebDebug:false,capacitorPlugins:this.capacitorPlugins}); 
        }
        .height('50%')
        .width('100%')

        RelativeContainer() {
          //isWebDebug:工具调试开关，capacitorPlugins：自定义插件列表，指定加载rawfile资源目录下文件
          MainPage({isWebDebug:false,indexPage:"/www2/index.html", capacitorPlugins:this.capacitorPlugins}); 
        }
        .height('50%')
        .width('100%')
    }
}
```

### 3. JS 侧插件调用

JS 侧插件调用完全遵守 capacitor 官方调用规范：

直接调用，无需做任何配置，代码如下：

```javascript
let data = await window.Capacitor.Plugins.TestCapPlugin.testMethod({
    message: 'Exec testMethod'
})
```

### 4. 自定义插件实现原理简述

由于 OpenHarmony 提供 ArkTS 和 C/C++ API，capacitor sdk 是使用 C/C++ 研发，自定义插件是跨语言调用，调用顺序为：JS 侧 → C/C++ 侧 → ArkTS 侧，回调是相反顺序，不过 ArkTS 侧的插件也可以直接调用 JS 侧。自定义插件的研发根据具体实现的功能，可以选择使用 ArkTS 开发，也可选择 C/C++ 开发。

## 自定义 C++ 插件研发

研发自定义 C++ 侧插件，您可以参考已移植的 capacitor 官方插件，编写 C++ 侧插件。

### 1. 开发步骤

1. 在源码集成的 capacitor 工程中，在源码的 CPP 目录内新建一个插件目录，保存您的自定义插件；
2. 在新目录中新建一个 class，该 class 要继承 Plugin 类，同时新建对应插件的 CMakeLists.txt；
3. 在您的 CPP 文件中，添加 `REGISTER_CAP_PLUGIN()` 注册您的插件名称，如 CapacitorPlugin；用于实例化您的插件对象；
4. 在您的 CPP 文件中，添加 `REGISTER_PLUGIN_METHOD()` 注册您的插件方法，如 PluginHello；用于实现您的插件功能；
5. 如果您的插件中需要调用 ArkTS 侧的代码，需要调用 `executeArkTs`（同步）或者 `executeArkTsAsync`（异步）执行 ArkTS 侧代码，参数说明参考 Plugin 类注释说明，ArkTS 实现文件需要在 capacitor 模块的 build-profile.json5 下的 buildOption → arkOptions → runtimeOnly → sources 下导入该文件；
6. 如果您的 ArkTS 侧需要把执行结果通知到 C++ 侧的插件，在 ArkTS 侧需要调用 `onArkTsResult` 函数通知 C++ 侧，C++ 侧的插件也要注册和实现 `onArkTsResult` 这个函数；
7. 在完成您的插件研发后需要将您的 cpp 文件添加到 CMakeLists.txt 中，完成编译；

### 2. 配置

在 rawfile/capacitor.plugins.json 文件中，添加插件名和 c++ 插件实现类名。

```json
[
  {
    "pkg": "@capacitor/CapacitorPlugin",
    "classpath": "CapacitorPlugin"
  }
]
```

### 3. JS 调用

```javascript
const result = await window.Capacitor.Plugins.CapacitorPlugin.PluginHello({
    message: 'Exec PluginHello'
});
```

## Web 加载性能优化

### 1. 预启动 web 和预渲染

在应用启动后，在 EntryAbility 代码中，后台启动 web 引擎，并在后台渲染页面，进入 page 页面后，页面秒开，关闭页面后，页面进入后台，不会销毁 web，下次打开仍可秒开；需提醒的是，在使用 capacitor 的页面预渲染时，会初始化 capacitor 插件，有可能会出现在用户没有同意隐私政策前，初始化插件会访问系统资源。

该功能需要对 mainPage 的组件进行二次封装，自己可以根据需要修改代码，如需技术支持请联系本开发者，提供封装方法和源码如下：

参考链接：[https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization](https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization)

**1. 在 pages 中新建 ArkTS 文件，命名为 WebBuilder.ets，复制以下代码：**

```typescript
import { MainPage, MainPageCycle, PluginEntry } from 'openHarmony-capacitor';
import { BuilderNode, FrameNode, NodeController } from '@kit.ArkUI';
import { webview } from '@kit.ArkWeb';
import { TestPlugin } from '../plugins/TestPlugin';

//根据需要扩展参数，参数参考MainPage的参数
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
        pluginName: 'TestPlugin', //插件名称
        pluginObject:new TestPlugin() //实例化插件对象
      }
  ];
  private mainPageCycle:MainPageCycle = new MainPageCycle().setOnAboutToAppear((webviewController: webview.WebviewController,parentPage?:object)=>{
    let page = parentPage as MainPagePageNodeController;//page为当前页面对象，相当于当前页面的this指针，使用该对象，必须将this指针传入到mainPage中
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
    //可以根据不同的页面传入不同的参数，单页面视图不存在这种情况，需要技术支持联系本开发者
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

**2. 修改 EntryAbility.ets，添加预启动 web 和预渲染代码：**

```typescript
//省略了其它代码
onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('pages/Splash', (err) => {
      //启动预启动web和预渲染，多页面视图可以预选设置和初始化
      createNWeb('/www3/index.html', windowStage.getMainWindowSync().getUIContext());
      createNWeb('/www3/index2.html', windowStage.getMainWindowSync().getUIContext());
      createNWeb('/www3/index3.html', windowStage.getMainWindowSync().getUIContext());
    });
}
```

**3. 修改 Index.ets 启动 capacitor 封装的 mainPage 页面，此时秒开，效率和传统打开 mainPage 相比大大提高：**

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

### 2. 资源拦截替换的 JavaScript 生成字节码缓存（Code Cache）

使用 capacitor 框架，根据 capacitor 的标准，所有页面和 JS 文件都在本地，openHarmony-capacitor 内部已经使用了拦截和替换功能，如果您加载的是在线资源或者 JS 文件，并且强制使用了 capacitor 协议栈（通过 capacitor.config.json 配置或者 SetCordovaProtocolUrl 函数设置），capacitor 框架也进行了资源缓存，如果您加载的是在线页面，使用 webview 的协议栈，可以结合 MainPage 提供的生命周期函数 onInterceptWebRequest 进行拦截，对于在线的 js 文件，也可以直接打包到本地的沙箱目录下，通过 capacitor 提供的 SetResourceReplace 函数进行拦截替换，以提供加载页面速度。示例代码如下：

参考链接：[https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization#section172031338172719](https://developer.huawei.com/consumer/cn/doc/best-practices/bpta-web-develop-optimization#section172031338172719)

```typescript
//省略有其它代码，以下是js预编译示例代码
configs: Array< Config> = [
{
  url: 'https://www.tongecn.com/example.js',
  localPath: 'example.js',//文件在rawfile目录下
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

//省略其它代码，以下是拦截替换
onInterceptWebRequest(request: WebResourceRequest, webTag:string):ESObject {
    // webview内核处理替换
    if(url == "https://www.tongecn.com/v1.1.1/temp/test3.js") {
      //替换本地沙箱路径
      SetResourceReplace(webTag, url, "https://localhost/data/storage/el2/base/files/test.js");
      //替换本地rawfile文件
      //SetResourceReplace(webTag, url, "https://www.example.com/test.js");
    }
    return null;
}

//省略有其它代码
MainPage({isWebDebug:true, indexPage:"https://www.tongecn.com", lifeCycle:data.mainPageCycle, parentPage:this,onInterceptWebRequest:this.onInterceptWebRequest});
```

## 常见问题

### 1. 返回键不起作用

OpenHarmony 返回键不起作用，就是手势事件，从左往右快速滑动，app 不返回上一页面，或者到了顶层页面不退出应用。

不同的框架有不同的处理方式，如果不管使用的是什么框架，只在 capacitor 层处理的，需要监听返回键事件，代码如下：

```javascript
document.addEventListener("deviceready", onDeviceReady, false);
function onDeviceReady() {
    document.addEventListener("backbutton", onBackKeyDown, false);
}

function onBackKeyDown() {
    //自己处理返回
}
```

如果采用 ionic angularjs 框架，可以采用如下代码：

```javascript
function showConfirm() {
   //处理退出应用的逻辑
}
$ionicPlatform.registerBackButtonAction(function (e) {
// Is there a page to go back to?
  if ($location.path() == '/tab/message') { //到了顶层页面，/tab/message是顶层页面的路由，这里只是举个例子，实际情况根据您的项目设置
      showConfirm();
      return false
  } else if ($ionicHistory.backView()) {
      // Go back in history
      $ionicHistory.goBack(); //自己处理返回
  } else {
      // This is the last page: Show confirmation popup
      showConfirm();
      return false;
  }
  e.preventDefault();
  return false;
}, 101);
```

说明：无论采用什么框架都可以在 capacitor 层通过监听 backbutton 返回事件自己处理。

如果加载的页面不包含相关 js，需要传入控制 webview 路由 MainPageOnBackPress 对象控制返回。

### 2. 如何访问沙箱资源文件

采用 `cdvfile://` 访问沙箱文件，以 downloadImage.png 为例：

```
cdvfile:///data/storage/el2/base/files/chuzhitong/downloadedImage.png
```

如果是 `file://` 作为 MainPage 的入口页，也可以使用 `file://` 协议访问本地文件，沙箱资源文件可以是图片（png, jpg, svg 等）、js、html 等，请参考最后的 file:// 协议说明。

### 3. HTTP 协议的 cookie 说明

如果您使用 http 协议非 https 协议，请参考如下 cookie 说明：

**（1）同源请求：**

例如您是直接在 MainPage 传入网址例如传入 `http://www.tongecn.com`，capacitor 会自动处理 cookie，无需手动处理。

**（2）跨域请求：**

例如您加载的文件在沙箱路径或者 rawfile 目录下的文件，在 html 文件中使用的 http 发送的 GET/POST 请求，此时需要再在 capacitor.config.json 里面配置 http 请求的域名，以便以 capacitor 为 http 处理 cookie，配置如下：

```json
{
  "harmony": {
    "cordova-protocol-force": ["***.***.com"]
  }
}
```

在 ArkTS 侧运行态动态设置 http 的 cookie，http 的 GET/POST 自动携带 cookie，capacitor 不处理静态资源，静态资源有 webview 处理：

```typescript
//在ArkTs侧运行态动态设置http的cookie,http的GET/POST自动携带cookie，capacitor不处理静态资源，静态资源有webview处理
aboutToAppear() {
    SetCordovaProtocolUrl("***.****.com");
}
```

**（3）HTTPS 协议：**

您发送的请求是 https 协议，非 http 协议，capacitor 会自动处理 cookie，无需手动处理。

**（4）手动设置 cookie：**

如果您要在 ArkTS 侧运行态手动设置 cookie，请参考不常用的高级功能部分。

### 4. 虚拟域名 www.example.com、自定义域名、localhost、file 协议和 cdvfile 协议的详细说明

加载 rawfile 目录下的页面时，通过 DevTools 工具测试时或者在日志 log 中会看到 `https://www.example.com` 的域名，可能会感到疑虑或者惊慌，接下来详细介绍一下，为什么使用此域名：

- OpenHarmony 无法使用 file 协议直接加载 rawfile 目录的文件，因此使用 www.example.com 虚拟域名代替 file 协议，因此您看到 `https://www.example.com` 就理解为 `file://` 即可。
- 在 capacitor.config.json 里面 harmony 属性添加 `"Hostname": "app.com"` 使用自定义域名加载本地文件。
- 使用 `cdvfile://` 协议加载沙箱目录文件。

说明：如果您的 h5 程序中有使用 file:// 协议，在 MainPage 中就必须使用 file:// 协议进入首页，否则 file 协议无法加载本地文件，capacitor 完全支持 file:// 协议加载文件，无论是从资源文件夹加载还是从沙箱路径加载 OpenHarmony capacitor 完全支持。

### 5. 屏蔽跟随系统字体大小

- 在 app.json5 中增加 configuration 选项以屏蔽跟随系统字体大小，具体配置方法参考：[https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/quick-start/app-configuration-file.md](https://docs.openharmony.cn/pages/v6.0/zh-cn/application-dev/quick-start/app-configuration-file.md)
- 在 EntryAbility 的 onWindowStageCreate 函数中增加 `windowStage.setDefaultDensityEnabled(true);` 屏蔽跟随显示大小缩放，参考：[https://docs.openharmony.cn/pages/v5.0.3/zh-cn/application-dev/reference/apis-arkui/js-apis-window.md](https://docs.openharmony.cn/pages/v5.0.3/zh-cn/application-dev/reference/apis-arkui/js-apis-window.md)

### 6. 跨域错误

capacitor 已经解决了所有的跨域访问，同时会自动携带 cookie，并兼容所有的自定义 http 头，无需做任何配置，但是前端 withCredentials 设置为 true 时，需要相应的配置解决跨域。

在前端发送 POST、GET 请求，withCredentials 为 true 时，同时您的服务器依赖于自定义 http 头例如 X-Auth-Token、Test-Type，mainPage 要传入相应的参数如下：

```typescript
MainPage({customHttpHeaders:"X-Auth-Token,Test-Type",isAllowCredentials:true})
```

**如果没有设置会报跨域错误：**

```
Access to XMLHttpRequest at '*****' from origin '****' has been blocked by CORS policy: The value of the 'Access-Control-Allow-Origin' header in the response must not be the wildcard '*' when the request's credentials mode is 'include'. The credentials mode of requests initiated by the XMLHttpRequest is controlled by the withCredentials attribute.
```

**解决方法**：在 mainPage 增加参数 `isAllowCredentials:true`

```typescript
MainPage({isAllowCredentials:true})
```

```
Access to XMLHttpRequest at '*****' from origin '*****' has been blocked by CORS policy: Request header field ***** is not allowed by Access-Control-Allow-Headers in preflight response
```

**解决方法**：在 mainPage 增加自定义头，例如自定义 http 头 X-Auth-Token、Test-Type

```typescript
MainPage({customHttpHeaders:"X-Auth-Token,Test-Type", isAllowCredentials:true})
```

### 7. iframe 跨域设置 cookie

如果您使用的 iframe 加载了第三方页面，第三方页面直接使用 js 设置 cookie，并不是通过 http 头 Set-Cookie 设置 cookie 的，js 设置 cookie，一定要加上 `SameSite=None; Secure`，否则 iframe 会出现页面无法显示问题，因为请求 http 头不会自动携带 cookie，设置 cookie 示例如下：

```javascript
document.cookie = 'token=467d1510-xxxx-xxxx-xxxx-73852620effa1; path=/; SameSite=None; Secure';
```

如果第三方页面无法更改，请使用内置浏览器打开页面，或者直接使用 a 标签打开页面，a 标签在 OpenHarmony 的 capacitor 中会自动触发内置浏览器，Android 和 iOS 不具备该功能。示例代码如下：

```javascript
//内置浏览器打开，可以配置相关参数，需集成内置浏览器插件
window.open("https://www.*****.com/index.html", "title=测试标题");
```

```html
<!--a标签打开，会自动触发内置浏览器-->
<a href="https://www.*****.com/index.html" target="_blank">打开链接</a>
```

### 8. capacitor 内部缓存时长设置

默认请求下使用 capacitor 的协议栈访问网络，静态资源缓存一天，即 `24 * 60 * 60` 秒钟，如果您想自己配置缓存时长在 capacitor.config.json 里面添加如下配置：

```json
{
  "harmony": {
    "cordova-cache-duration":60
  }
}
```

## 目录结构

```
目录根目录/
└─src
    ├─main
    │  ├─cpp
    │  │  ├─CoreHarmony // openharmony适配层
    │  │  ├─getcapacitor // capacitor适配层
    │  │  ├─HotCodePushPlugin // 热更新
    │  │  ├─TsCordovaPlugin // arkts cordova插件
    │  │  └─types
    │  │      └─libcapacitor
    │  ├─ets
    │  │  └─components
    │  │      ├─AlertDialog // 弹框
    │  │      ├─CoreHarmony // openharmony适配层
    │  │      ├─getcapacitor // capacitor适配层
    │  │      ├─ImageCompress // 图片处理
    │  │      ├─InAppBrowser // 内置浏览器
    │  │      ├─Permission // 权限处理
    │  │      ├─PluginAction // 插件工具类
    │  │      └─SplashScreen // 闪屏
    │  └─resources // 资源目录
    ├─ohosTest // 测试目录
    │  └─ets
    │      └─test
    └─test // 测试目录
```

## 贡献代码

使用过程中发现任何问题都可以提 [Issue](https://gitcode.com/CPF-Ionic/openHarmony-capacitor/issues)，当然，也非常欢迎发 [PR](https://gitcode.com/CPF-Ionic/openHarmony-capacitor/pulls) 共建。

## 许可证

本插件基于 **MIT License** 开源，详见 [LICENSE](LICENSE) 文件。