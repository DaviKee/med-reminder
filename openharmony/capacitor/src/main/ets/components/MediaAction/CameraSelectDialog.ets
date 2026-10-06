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

import { StartCamera } from "../ImageCompress/CameraAction";
import { MainPage } from "../MainPage";
import { cordovaWebTagToObjectGlobe, NativeAttribute } from "../PluginGlobal";
import { PromptActionClass } from '../PluginAction/PromptActionClass';
import { SelectMedia } from "./MediaAction";
import { ComponentContent } from "@kit.ArkUI";
import capacitor from 'libcapacitor.so';
import window from '@ohos.window';

interface CustomDialogOption {
  PromptLabelHeader: string;
  PromptLabelPicture: string;
  PromptLabelPhoto: string;
  PromptLabelCancel: string;
}

// 将 PromptActionClass 与本次调用的安全区高度封装在一起，避免使用全局变量
class CameraDialogParam {
  promptAction: PromptActionClass = new PromptActionClass();
  safeAreaBottom: number = 0;
  // 预解析一次，避免 @Builder 里多次重复调用 JSON.parse
  config: CustomDialogOption = {
    PromptLabelHeader: 'Photo',
    PromptLabelPicture: 'Take Picture',
    PromptLabelPhoto: 'From Photos',
    PromptLabelCancel: 'Cancel'
  };
}

function parseMaybeJSON<T>(val: string | object | undefined, fallback: T): T {
  if (typeof val === "string") {
    try {
      return JSON.parse(val) as T;
    } catch {
      return fallback;
    }
  }
  if (val && typeof val === "object") {
    return val as T;
  }
  return fallback;
}

// pageArgs 是 JSON 字符串，其中 promptConfig 字段才是 CustomDialogOption
function getDialogConfig(promptActionClass: PromptActionClass): CustomDialogOption {
  const def: CustomDialogOption = {
    PromptLabelHeader: 'Photo',
    PromptLabelPicture: 'Take Picture',
    PromptLabelPhoto: 'From Photos',
    PromptLabelCancel: 'Cancel'
  };
  try {
    if (promptActionClass.data === undefined || promptActionClass.data === null) {
      return def;
    }
    const jsonRoot =
      JSON.parse((promptActionClass.data as NativeAttribute).pageArgs) as Record<string, string | object | undefined>;
    return parseMaybeJSON<CustomDialogOption>(jsonRoot['promptConfig'], def);
  } catch {
    return def;
  }
}

@Builder
function CustomerCameraPromptDialog(param: CameraDialogParam): void {
  Column() {
    Text(param.config.PromptLabelHeader || 'Photo')
      .width('100%')
      .fontSize(14)
      .fontColor('#FF757575')
      .textAlign(TextAlign.Start)
      .padding({ top: 8, bottom: 8, left: 8, right: 8 })
      .backgroundColor('#FFFFFFFF')

    Text(param.config.PromptLabelPhoto || 'From Photos')
      .width('100%')
      .fontSize(14)
      .fontColor('#FF000000')
      .textAlign(TextAlign.Start)
      .padding({ top: 12, bottom: 12, left: 12, right: 12 })
      .backgroundColor('#FFFFFFFF')
      .onClick(() => {
        param.promptAction.closeDialog();
        SelectMedia(param.promptAction.data as NativeAttribute);
      })

    Text(param.config.PromptLabelPicture || 'Take Picture')
      .width('100%')
      .fontSize(14)
      .fontColor('#FF000000')
      .textAlign(TextAlign.Start)
      .padding({ top: 12, bottom: 12, left: 12, right: 12 })
      .backgroundColor('#FFFFFFFF')
      .onClick(() => {
        param.promptAction.closeDialog();
        StartCamera(param.promptAction.data as NativeAttribute);
      })

    Text(param.config.PromptLabelCancel || 'Cancel')
      .width('100%')
      .fontSize(14)
      .fontColor('#FF000000')
      .textAlign(TextAlign.Start)
      .padding({ top: 12, bottom: 12, left: 12, right: 12 })
      .backgroundColor('#FFFFFFFF')
      .onClick(() => {
        param.promptAction.closeDialog();
        capacitor.onArkTsResult(JSON.stringify({ cancelled: true, value: "" }),
          (param.promptAction.data as NativeAttribute).pageObject,
          (param.promptAction.data as NativeAttribute).pageWebTag);
      })
  }
  .width('100%')
  .backgroundColor('#FFFFFFFF')
  .padding({ top: 16, bottom: 16, left: 16, right: 16 })
}

export function NotificationCameraAndPhotos(pageIndex: NativeAttribute): void {
  try {
    if (!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    let uiContext: UIContext = mainPage.getUIContext();

    // showDialog 通过闭包捕获局部变量 safeAreaBottom，不依赖全局状态
    const showDialog = (safeAreaBottom: number) => {
      let cameraPromptActionClass = new PromptActionClass();
      cameraPromptActionClass.setData(pageIndex);

      let param = new CameraDialogParam();
      param.promptAction = cameraPromptActionClass;
      param.safeAreaBottom = safeAreaBottom;
      param.config = getDialogConfig(cameraPromptActionClass);
      let audioContentNode: ComponentContent<Object> =
        new ComponentContent(uiContext, wrapBuilder(CustomerCameraPromptDialog), param);
      cameraPromptActionClass.setContext(uiContext);
      cameraPromptActionClass.setContentNode(audioContentNode);
      cameraPromptActionClass.setOptions({
        alignment: DialogAlignment.Bottom,
        // 向上偏移安全区高度，弹窗内容天然停在导航栏上方，下方区域不属于弹窗
        offset: { dx: 0, dy: -safeAreaBottom },
        onWillDismiss: (dismissDialogAction: DismissDialogAction) => {
          if (dismissDialogAction.reason === DismissReason.TOUCH_OUTSIDE) {
            cameraPromptActionClass.closeDialog();
            capacitor.onArkTsResult(JSON.stringify({ cancelled: true, value: "" }),
              (cameraPromptActionClass.data as NativeAttribute).pageObject,
              (cameraPromptActionClass.data as NativeAttribute).pageWebTag);
          }
        }
      });
      cameraPromptActionClass.openDialog();
    };

    // 异步获取底部导航栏安全区高度后再打开弹窗
    let hostContext = uiContext.getHostContext();
    if (hostContext !== undefined) {
      window.getLastWindow(hostContext)
        .then((win) => {
          let avoidArea = win.getWindowAvoidArea(window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR);
          showDialog(uiContext.px2vp(avoidArea.bottomRect.height));
        })
        .catch(() => {
          showDialog(0);
        });
    } else {
      showDialog(0);
    }
  } catch (err) {
    capacitor.onArkTsResult(JSON.stringify({ errorMsg: err?.message ?? 'show prompt fail' }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }
}