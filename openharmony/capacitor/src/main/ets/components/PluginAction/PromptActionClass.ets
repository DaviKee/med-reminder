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

import { BusinessError } from '@kit.BasicServicesKit';
import { ComponentContent, promptAction } from '@kit.ArkUI';
import { UIContext } from '@ohos.arkui.UIContext';

export class PromptActionClass {
  ctx?: UIContext;
  contentNode?: ComponentContent<Object>;
  options?: promptAction.BaseDialogOptions;
  data?:Object;

  setContext(context: UIContext) {
    this.ctx = context;
  }

  setContentNode(node: ComponentContent<Object>) {
    this.contentNode = node;
  }

  setOptions(options: promptAction.BaseDialogOptions) {
    this.options = options;
  }
  setData(data: Object) {
    this.data = data;
  }
  openDialog() {
    if (this.contentNode !== null) {
      //主动关闭弹窗不会触发onWillDismiss函数，如果有回调需要手动处理
      this.ctx!.getPromptAction().openCustomDialog(this.contentNode, this.options)
        .then(() => {
          console.info('OpenCustomDialog complete.')
        })
        .catch((error: BusinessError) => {
          let message = (error as BusinessError).message;
          let code = (error as BusinessError).code;
          console.error(`OpenCustomDialog args error code is ${code}, message is ${message}`);
        })
    }
  }

  closeDialog() {
    if (this.contentNode !== null) {
      this.ctx!.getPromptAction().closeCustomDialog(this.contentNode)
        .then(() => {
          console.info('CloseCustomDialog complete.')
        })
        .catch((error: BusinessError) => {
          let message = (error as BusinessError).message;
          let code = (error as BusinessError).code;
          console.error(`CloseCustomDialog args error code is ${code}, message is ${message}`);
        })
    }
  }

  updateDialog(options: promptAction.BaseDialogOptions) {
    if (this.contentNode !== null) {
      this.ctx!.getPromptAction().updateCustomDialog(this.contentNode, options)
        .then(() => {
          console.info('UpdateCustomDialog complete.')
        })
        .catch((error: BusinessError) => {
          let message = (error as BusinessError).message;
          let code = (error as BusinessError).code;
          console.error(`UpdateCustomDialog args error code is ${code}, message is ${message}`);
        })
    }
  }
}



@Builder
function PromptDialog(params:object) {
  Row() {
    Column() {
      Text(params?.["title"]).fontSize(14).margin({ top: 10, bottom: 10 })
      Text(params?.["message"]).fontSize(16).margin({ top: 10, bottom: 20 })
      TextInput({
        text: params?.["defaultValue"]
      })
        .onChange((value: string) => {
          params["confirmValue"] = value;
        })
        .margin({ bottom: 10 })
      Row() {
        Button("cancel").onClick(() => {
          if(params?.["promptActionClass"]){
            let promptActionClass:PromptActionClass = params?.["promptActionClass"];
            promptActionClass.closeDialog();
          }
        }).backgroundColor(0xffffff).fontColor(Color.Blue).width(100)
        Button("ok").onClick(() => {
          if(params?.["promptActionClass"]){
            let promptActionClass:PromptActionClass = params?.["promptActionClass"];
            promptActionClass.closeDialog();
          }
          params["result"]?.handlePromptConfirm(params["confirmValue"]);
        }).backgroundColor(0xffffff).fontColor(Color.Blue).width(100)
      }.width('100%').justifyContent(FlexAlign.SpaceAround)
    }
    .width('95%')
    .height(200)
    .backgroundColor("#ffffff").borderRadius(16)
    .justifyContent(FlexAlign.Center)
  }.width('100%').height('100%')
  .justifyContent(FlexAlign.Center)
}

export function ShowPromptDialog(uiContext: UIContext, params:object) {
  let promptActionClass = new PromptActionClass();
  params["promptActionClass"] = promptActionClass;
  let contentNode: ComponentContent<Object> = new ComponentContent(uiContext, wrapBuilder(PromptDialog), params);
  promptActionClass.setContext(uiContext);
  promptActionClass.setContentNode(contentNode);
  promptActionClass.setOptions({ alignment: DialogAlignment.Top, offset: { dx: 0, dy: 50 } });
  promptActionClass.openDialog();
  setTimeout(()=>{
    promptActionClass.closeDialog();
  },50000)
}