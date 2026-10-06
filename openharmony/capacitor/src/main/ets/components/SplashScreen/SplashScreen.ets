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

import { BuilderNode, FrameNode, NodeController, window } from '@kit.ArkUI';
import { MainPage } from '../MainPage';
import { cordovaWebTagToObjectGlobe, NativeAttribute } from '../PluginGlobal';
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { NormalizeError } from '../NormalizeError';
import { matrix4 } from '@kit.ArkUI'
import capacitor from 'libcapacitor.so'

class SplashParameters{
  context?:Context;
  splashImgPath?:string;
  isFullScreen?:boolean;
  isShowProgress?:boolean;
  progressColor?:string;
  backgroundColor?:string;
  imageScaleType?:string;
  progressStyle?:string;
}

enum SpinnerStyle {
  HORIZONTAL = 'horizontal',
  SMALL = 'small',
  LARGE = 'large',
  INVERSE = 'inverse',
  SMALL_INVERSE = 'smallInverse',
  LARGE_INVERSE = 'largeInverse'
}

interface SpinnerConfigItem {
  width: number;
  height: number;
  strokeWidth: number;
  isInverse: boolean;
  color: string;
  type:ProgressType;
}

interface ImageConfigItem {
  scaleType: ImageFit;
  positionAlign: Alignment;
}

type SpinnerConfigEntry = [SpinnerStyle, SpinnerConfigItem];
type ImageConfigEnter  = [string,ImageConfigItem];

const SpinnerConfigMap: Map<SpinnerStyle, SpinnerConfigItem> = new Map([
  [SpinnerStyle.HORIZONTAL, {
    width: 200,
    height: 12,
    strokeWidth: 8,
    isInverse: false,
    color: '#007AFF',
    type:ProgressType.Linear
  }],
  [SpinnerStyle.SMALL, {
    width: 32,
    height: 32,
    strokeWidth: 4,
    isInverse: false,
    color: '#007AFF',
    type:ProgressType.Ring
  }],
  [SpinnerStyle.LARGE, {
    width: 64,
    height: 64,
    strokeWidth: 8,
    isInverse: false,
    color: '#007AFF',
    type:ProgressType.Ring
  }],
  [SpinnerStyle.INVERSE, {
    width: 46,
    height: 46,
    strokeWidth: 6,
    isInverse: true,
    color: '#FFFFFF',
    type:ProgressType.Ring
  }],
  [SpinnerStyle.SMALL_INVERSE, {
    width: 32,
    height: 32,
    strokeWidth: 3,
    isInverse: true,
    color: '#FFFFFF',
    type:ProgressType.Ring
  }],
  [SpinnerStyle.LARGE_INVERSE, {
    width: 64,
    height: 64,
    strokeWidth: 6,
    isInverse: true,
    color: '#FFFFFF',
    type:ProgressType.Ring
  }]
] as SpinnerConfigEntry[]); // 核心：通过as断言数组类型

const ImageConfigMap: Map<string,ImageConfigItem> = new Map([
  ["CENTER",{
    scaleType: ImageFit.None,
    positionAlign:Alignment.Center
  }],
  ["CENTER_CROP",{
    scaleType: ImageFit.Cover,
    positionAlign:Alignment.Center
  }],
  ["CENTER_INSIDE",{
    scaleType: ImageFit.Contain,
    positionAlign:Alignment.Center
  }],
  ["FIT_CENTER",{
    scaleType: ImageFit.Contain,
    positionAlign:Alignment.Center
  }],
  ["FIT_START",{
    scaleType: ImageFit.Contain,
    positionAlign:Alignment.TopStart
  }],
  ["FIT_END",{
    scaleType: ImageFit.Contain,
    positionAlign:Alignment.BottomEnd
  }],
  ["FIT_XY",{
    scaleType: ImageFit.Fill,
    positionAlign:Alignment.Center
  }],
  ["MATRIX",{
    scaleType: ImageFit.MATRIX,
    positionAlign:Alignment.Center
  }]] as ImageConfigEnter[]);

class ColorUtil {
  static getBgRelativeInverseColor(
    bgColor: string,
    lightColor = '#FFFFFF',
    darkColor = '#000000'
  ): string {

    return ColorUtil.getInverseColor(bgColor);
  }

  static getInverseColor(color: string): string {
    const hex6 = /^#([0-9a-fA-F]{6})$/;
    const hex8 = /^#([0-9a-fA-F]{8})$/;
    const rgb = /^rgb\s*\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)$/;
    const rgba = /^rgba\s*\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d*\.?\d+)\s*\)$/;

    let m: RegExpMatchArray | null;
    if ((m = color.match(hex6))) {
      const r = 255 - parseInt(m[1].substring(0, 2), 16);
      const g = 255 - parseInt(m[1].substring(2, 4), 16);
      const b = 255 - parseInt(m[1].substring(4, 6), 16);
      return (
        '#' +
        r.toString(16).padStart(2, '0') +
        g.toString(16).padStart(2, '0') +
        b.toString(16).padStart(2, '0')
      ).toUpperCase();
    } else if ((m = color.match(hex8))) {
      const a = m[1].substring(0, 2);
      const r = 255 - parseInt(m[1].substring(2, 4), 16);
      const g = 255 - parseInt(m[1].substring(4, 6), 16);
      const b = 255 - parseInt(m[1].substring(6, 8), 16);
      return (
        '#' +
        a.toUpperCase() +
        r.toString(16).padStart(2, '0').toUpperCase() +
        g.toString(16).padStart(2, '0').toUpperCase() +
        b.toString(16).padStart(2, '0').toUpperCase()
      );
    } else if ((m = color.match(rgb))) {
      const r = 255 - parseInt(m[1]);
      const g = 255 - parseInt(m[2]);
      const b = 255 - parseInt(m[3]);
      return `rgb(${r}, ${g}, ${b})`;
    } else if ((m = color.match(rgba))) {
      const r = 255 - parseInt(m[1]);
      const g = 255 - parseInt(m[2]);
      const b = 255 - parseInt(m[3]);
      const a = m[4];
      return `rgba(${r}, ${g}, ${b}, ${a})`;
    }
    // 其他格式直接返回原色
    return color;
  }
}


class ConfigUtil {

  public static getConfig(style: SpinnerStyle): SpinnerConfigItem {
    return SpinnerConfigMap.get(style) || SpinnerConfigMap.get(SpinnerStyle.INVERSE)!;
  }

  public static getImageScaleType(type: string): ImageFit {
    const config = ImageConfigMap.get(type);
    return config?.scaleType as ImageFit;
  }

  public static getImageAlignment(type: string): Alignment {
    const config = ImageConfigMap.get(type);
    return config?.positionAlign as Alignment;
  }

  public static getStyle(styleName:string): SpinnerStyle {
    if (styleName == SpinnerStyle.HORIZONTAL) {
      return SpinnerStyle.HORIZONTAL;
    } else if(styleName == SpinnerStyle.SMALL) {
      return SpinnerStyle.SMALL;
    } else if(styleName == SpinnerStyle.LARGE) {
      return SpinnerStyle.LARGE;
    } else if(styleName == SpinnerStyle.INVERSE) {
      return SpinnerStyle.INVERSE;
    } else if(styleName == SpinnerStyle.SMALL_INVERSE) {
      return SpinnerStyle.SMALL_INVERSE;
    } else if(styleName == SpinnerStyle.LARGE_INVERSE) {
      return SpinnerStyle.LARGE_INVERSE;
    } else {
      return SpinnerStyle.SMALL;
    }
  }


  public static getBgRelativeColor(style: SpinnerStyle, bgColor: string, spinnerColor:string): string {

    if (style.includes("inverse") || style.includes("Inverse")) {
      return ColorUtil.getBgRelativeInverseColor(bgColor);
    }

    return spinnerColor;
  }
}

function getImageSourceSafely(context: common.Context, imagePath: string): DrawableDescriptor | undefined {
  try {
    return context.resourceManager.getDrawableDescriptorByName(imagePath);
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`Load source fialed. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    return undefined;
  }
}

@Component
struct HorizontalSliderLoading {
  @Prop progressWidth: number = 200
  @Prop progressHeight: number = 12
  @Prop sliderColor: string = "#FFFFFF"
  private backgroundColorValue:string = "#EEEEEE"

  // 动画偏移量：驱动滑块移动
  @State sliderOffsetX: number = -80;
  // 动画开关：用@State保证状态变更可追踪
  @State isAnimated: boolean = false;
  // 动画锁：避免重复创建递归链（核心优化）
  private isAnimating: boolean = false;

  // 核心：停止动画（立即终止递归的关键）
  public stopSliderAnimation() {
    this.isAnimated = false;
    this.isAnimating = false; // 释放动画锁，终止递归
    console.log('[Loading组件] 外部调用停止动画，已终止递归');
  }

  // 启动动画（防抖：避免重复启动）
  public startSliderAnimation() {
    // 双重判断：避免重复创建递归链
    if (this.isAnimated || this.isAnimating) return;

    this.isAnimated = true;
    this.runSliderAnimation();
    console.log('[Loading组件] 外部调用启动动画');
  }

  // 重构后的动画递归逻辑（解决原问题）
  private runSliderAnimation() {
    if (!this.isAnimated) return; // 立即终止递归的关键判断

    this.isAnimating = true; // 加锁，避免重复执行
    animateTo({
      duration: 1000,
      curve: Curve.Linear,
      onFinish: () => {
        // 第一步：滑块滑到右侧（80）
        this.sliderOffsetX = 80;

        // 第二步：滑块滑回左侧（-80）
        animateTo({
          duration: 1000,
          curve: Curve.Linear,
          onFinish: () => {
            this.sliderOffsetX = -80;
            this.isAnimating = false; // 解锁

            // 只有动画开关开启时，才继续递归
            if (this.isAnimated) {
              this.runSliderAnimation();
            }
          }
        }, () => {
          this.sliderOffsetX = -80;
        });
      }
    }, () => {
      this.sliderOffsetX = 80;
    });
  }

  // 组件销毁：兜底停止动画
  aboutToDisappear() {
    this.stopSliderAnimation();
  }

  build() {
    Stack() {
      // 背景条
      Row()
        .width(this.progressWidth)
        .height(this.progressHeight)
        .backgroundColor(this.backgroundColorValue)
        .borderRadius(6);
      // 滑动条
      Row()
        .width(this.progressWidth/5)
        .height(this.progressHeight)
        .backgroundColor(this.sliderColor)
        .borderRadius(6)
        .translate({ x: this.sliderOffsetX })
        // 可见性变化：精准控制动画启停（兼容低版本）
        .onVisibleAreaChange([0.1], (isExpanding: boolean, currentRatio: number) => {
          if (isExpanding && currentRatio >= 0.1) {
            console.info('组件可见，启动动画');
            this.startSliderAnimation(); // 可见时主动启动
          } else if (currentRatio < 0.1) {
            console.info('组件不可见，停止动画');
            this.stopSliderAnimation(); // 不可见时停止
          }
        })
        // 首次出现时启动动画（正确方法名）
        .onAppear(() => this.startSliderAnimation())
        // 消失时兜底停止（修正：onDisAppear 首字母A大写）
        .onDisAppear(() => this.stopSliderAnimation());
    }
    .width('100%')
    .height(60)
    .alignContent(Alignment.Center);
  }
}

@Builder
function buildSplashScreen(data:SplashParameters) {

  Column() {
    Stack( { alignContent: Alignment.Center }) {

      if (getImageSourceSafely(data.context!, data.splashImgPath!) !== undefined) {
        if (data.imageScaleType === "MATRIX") {
          Image(getImageSourceSafely(data.context!, data.splashImgPath!))
            .objectFit(ImageFit.CENTER)
        } else {
          Image(getImageSourceSafely(data.context!, data.splashImgPath!))
            .objectFit(ConfigUtil.getImageScaleType(data.imageScaleType as string))
            .align(ConfigUtil.getImageAlignment(data.imageScaleType as string))
        }
      }

      if(data.isShowProgress) {

        if(data.progressStyle !== "horizontal") {

          Progress({
            value: 0, total: 100,
            type: ConfigUtil.getConfig(ConfigUtil.getStyle(data.progressStyle)).type
          })
          .width(ConfigUtil.getConfig(ConfigUtil.getStyle(data.progressStyle)).width)
          .height(ConfigUtil.getConfig(ConfigUtil.getStyle(data.progressStyle)).height)
          .color(ConfigUtil.getBgRelativeColor(ConfigUtil.getStyle(data.progressStyle as string),data.backgroundColor as string,data.progressColor as string))
          .style({
            strokeWidth: ConfigUtil.getConfig(ConfigUtil.getStyle(data.progressStyle)).strokeWidth,
            status: ProgressStatus.LOADING })
          .align(Alignment.Center)
        } else {
          HorizontalSliderLoading({
            progressWidth: ConfigUtil.getConfig(ConfigUtil.getStyle(data.progressStyle)).width,
            progressHeight: ConfigUtil.getConfig(ConfigUtil.getStyle(data.progressStyle)).height,
            sliderColor: data.progressColor
          });
        }
      }
    }
    .width("100%")
    .height("100%")
  }
  .width("100%")
  .height("100%")
  .backgroundColor(data.backgroundColor)
}

export class SplashScreenNodeController extends NodeController {
  private rootNode: BuilderNode<SplashParameters[]> | null = null;
  private root: FrameNode | null = null;
  private context?:Context;
  private splashImgPath?:string;
  private isFullScreen?:boolean;
  private isShowProgress?:boolean;
  private progressColor?:string;
  private backgroundColor?:string;
  private imageScaleType?:string;
  private progressStyle?:string;

  constructor(
    context:Context,
    splashImgPath:string,
    isFullScreen:boolean,
    isShowProgress:boolean,
    progressColor:string,
    backgroundColor:string,
    imageScaleType:string,
    progressStyle:string
    ) {

    super();
    this.context = context;
    this.splashImgPath = splashImgPath;
    this.isFullScreen = isFullScreen;
    this.isShowProgress = isShowProgress;
    this.progressColor = progressColor;
    this.backgroundColor = backgroundColor;
    this.imageScaleType = imageScaleType;
    this.progressStyle = progressStyle;
  }

  makeNode(uiContext: UIContext): FrameNode | null {
    try {
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
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`makeNode failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    }
    return null;
  }

  initNode(uiContext:UIContext) {
    if(this.rootNode != null) {
      return;
    }
    this.rootNode = new BuilderNode(uiContext);
    this.rootNode.build(wrapBuilder<SplashParameters[]>(buildSplashScreen), {
      context:this.context,
      splashImgPath:this.splashImgPath,
      isFullScreen:this.isFullScreen,
      isShowProgress:this.isShowProgress,
      progressColor:this.progressColor,
      backgroundColor:this.backgroundColor,
      imageScaleType:this.imageScaleType,
      progressStyle:this.progressStyle
    });
  }
}

export function ClearSplashFullScreen(context:common.UIAbilityContext, isFullScreen:boolean, properties?:window.SystemBarProperties) {
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
      if(properties) {
        windowClass.setWindowSystemBarProperties(properties);
      }
      if(!isFullScreen) {
        windowClass.setWindowLayoutFullScreen(false);
      } else {
        windowClass.setWindowLayoutFullScreen(true);
      }
    } catch (exception) {
      console.error(`Failed to set the window layout to full-screen mode. Cause code: ${exception.code}, message: ${exception.message}`);
    }
  });
}

export function SetSplashScreenFullScreen(context:common.UIAbilityContext) {
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
      windowClass.setWindowLayoutFullScreen(true);
    } catch (exception) {
      console.error(`Failed to set the window layout to full-screen mode. Cause code: ${exception.code}, message: ${exception.message}`);
    }
  });
}

export function SetSplashScreenImmersive(context:common.UIAbilityContext) {
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
      windowClass.setWindowSystemBarEnable([]);
    } catch (exception) {
      console.error(`Failed to set the window layout to full-screen mode. Cause code: ${exception.code}, message: ${exception.message}`);
    }
  });
}

export function ClearSplashScreenImmersive(context:common.UIAbilityContext,isKeepImmersiveModel?:boolean) {

  if(isKeepImmersiveModel){
    return;
  }

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
      windowClass.setWindowLayoutFullScreen(false);
      windowClass.setWindowSystemBarEnable(['status', 'navigation']);
      console.debug("退出 沉浸模式")
    } catch (exception) {
      console.error(`Failed to set the window layout to full-screen mode. Cause code: ${exception.code}, message: ${exception.message}`);
    }
  });
}

interface SplashPageArgs {
  autoHide?: boolean;
  fadeInDuration?: number;
  fadeOutDuration?: number;
  showDuration?: number;
}

interface ResultData {
  message?: string;
}

export function ShowSplashScreen(pageIndex:NativeAttribute) {

  let result:ResultData = {
    message:""
  }
  try {

    if (!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
      result.message ="args error!";
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
      return;
    }

    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    if (pageIndex.pageValue == 1) {
        // 解析并类型保护
        let jsonRoot: SplashPageArgs = {};
        try {
          jsonRoot = JSON.parse(pageIndex.pageArgs) as SplashPageArgs;
        } catch (e) {
          capacitor.onArkTsResult(JSON.stringify("args error!"), pageIndex.pageObject, pageIndex.pageWebTag);
          return;
        }

        const autoHide = typeof jsonRoot.autoHide === "boolean" ? jsonRoot.autoHide : false;
        const fadeInDuration = typeof jsonRoot.fadeInDuration === "number" ? jsonRoot.fadeInDuration : 0;
        const fadeOutDuration = typeof jsonRoot.fadeOutDuration === "number" ? jsonRoot.fadeOutDuration : 0;
        const showDuration = typeof jsonRoot.showDuration === "number" ? jsonRoot.showDuration : 0;

        mainPage.showSplashScreenWithFade(autoHide,fadeInDuration,fadeOutDuration,showDuration);
        result.message ="success";
        capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    }

    if (pageIndex.pageValue == 0) {
      let jsonRoot: SplashPageArgs = {};
      try {
        jsonRoot = JSON.parse(pageIndex.pageArgs) as SplashPageArgs;
      } catch (e) {
        return;
      }
      const fadeOutDuration = typeof jsonRoot.fadeOutDuration === "number" ? jsonRoot.fadeOutDuration : 0;
      mainPage.splashScreenHide(true,fadeOutDuration);
      result.message ="success";
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`ShowSplashScreen failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
    result.message = cordovaError.message;
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  }
}
