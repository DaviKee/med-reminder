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

import { ArkTsAttribute, cordovaWebTagToObjectGlobe, NativeAttribute } from '../PluginGlobal';
import capacitor from 'libcapacitor.so';
import { MainPage } from '../MainPage';
import { PromptActionClass } from '../PluginAction/PromptActionClass';
import { ComponentContent } from '@kit.ArkUI';
import { media } from '@kit.MediaKit';
import audio from '@ohos.multimedia.audio';
import { NormalizeError } from '../NormalizeError';

let alertCustomDialogOption:NativeAttribute|null = null;
interface CustomDialogOption {
  title:string;
  message:string;
  buttonLabel:string;
  action:string;
  defaultText:string;
}


// 蜂鸣声播放器类
class ArkBeepPlayer {
  private audioRenderer: audio.AudioRenderer | null = null;
  private isPlaying: boolean = false;
  
  /**
  * 播放蜂鸣声（使用正弦波）
  * @param frequency 频率（Hz）
  * @param durationMs 持续时间（毫秒）
  * @param volume 音量（0.0-1.0）
  */
  async playBeep(frequency: number = 1000,durationMs: number = 200,volume: number = 0.5): Promise<boolean> {
    if (this.isPlaying) {
      console.warn('当前正在播放，请等待完成');
      return false;
    }
      
    try {
      // 1. 配置音频渲染器参数
      const audioRendererOptions: audio.AudioRendererOptions = {
        streamInfo: {
          samplingRate: audio.AudioSamplingRate.SAMPLE_RATE_44100,
          channels: audio.AudioChannel.CHANNEL_1,
          sampleFormat: audio.AudioSampleFormat.SAMPLE_FORMAT_S16LE,
          encodingType: audio.AudioEncodingType.ENCODING_TYPE_RAW
        },
        rendererInfo: {
          usage: audio.StreamUsage.STREAM_USAGE_MEDIA,
          rendererFlags: 0
        }
      };
        
      // 2. 创建音频渲染器
      this.audioRenderer = await audio.createAudioRenderer(audioRendererOptions);
        
      // 3. 配置音量
      this.audioRenderer.setVolume(volume);
        
      // 4. 生成音频数据
      const audioData = this.generateSineWave(frequency, durationMs);

      // 5. 开始播放
      await this.audioRenderer.start();
      this.isPlaying = true;

      // 6. 写入音频数据
      const writeResult = await this.audioRenderer.write(audioData);
      if (writeResult < 0) {
        console.error('写入音频数据失败:', writeResult);
        await this.stop();
        return false;
      }

      // 7. 等待播放完成
      await this.sleep(durationMs);

      // 8. 停止播放
      await this.stop();

      return true;
        
    } catch (error) {
      console.error('播放蜂鸣声失败:', error);
      await this.stop();
      return false;
    }
  }
  
  /**
  * 生成正弦波音频数据
  */
  private generateSineWave(frequency: number,durationMs: number): ArrayBuffer {
    const sampleRate = 44100;
    const numSamples = Math.floor((durationMs / 1000) * sampleRate);
    const buffer = new ArrayBuffer(numSamples * 2); // 16-bit = 2 bytes
    const view = new DataView(buffer);
      
    const angularFrequency = 2 * Math.PI * frequency / sampleRate;
    const maxAmplitude = 32767 * 0.5; // 50% 振幅
      
    for (let i = 0; i < numSamples; i++) {
      const sample = Math.sin(angularFrequency * i);
      const intValue = Math.floor(sample * maxAmplitude);
      view.setInt16(i * 2, intValue, true);
    }
      
    return buffer;
  }
  
  /**
  * 停止播放
  */
  async stop(): Promise<void> {
    try {
      if (this.audioRenderer) {
        if (this.isPlaying) {
          await this.audioRenderer.stop();
        }
        await this.audioRenderer.release();
        this.audioRenderer = null;
      }
      this.isPlaying = false;
    } catch (error) {
      console.error('停止播放失败:', error);
    }
  }
  
  /**
  * 休眠函数
  */
  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}

let changeText:string = "";
@Builder
function CustomerAlertDialog(promptActionClass:PromptActionClass) {
  Column(){
    Column() {
      Text((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).title).fontSize(14).margin({ top: 20, bottom: 5 }).fontWeight(FontWeight.Bold)
      Text((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).message).fontSize(16).margin({ top: 5, bottom: 20 })
      if((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).action == "alert") {
        Text().border({ width: { bottom: 1 }, color: "#E0E0E0" }).width('100%')
        Flex({ justifyContent: FlexAlign.SpaceAround }) {
          Button((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).buttonLabel)
            .onClick(() => {
              promptActionClass.closeDialog();
              let result: ArkTsAttribute = { content: "alert", result: [] };
              capacitor.onArkTsResult(JSON.stringify(result), alertCustomDialogOption!.pageObject, alertCustomDialogOption!.pageWebTag);
            }).backgroundColor(0xffffff).fontColor(Color.Blue).height(40)
        }
      }

      if((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).action == "confirm") {
        Text().border({ width: { bottom: 1 }, color: "#E0E0E0" }).width('100%')
        Flex({ justifyContent: FlexAlign.SpaceAround }) {
          ForEach(JSON.parse((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).buttonLabel) as Array<string>,
            (item: string, index?: number) => {
              Stack({ alignContent: Alignment.Center }) {
                Button(item).onClick(() => {
                  promptActionClass.closeDialog();
                  let result: ArkTsAttribute = {
                    content: "confirm",
                    result: [(JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).buttonLabel, item]
                  };
                  capacitor.onArkTsResult(JSON.stringify(result), alertCustomDialogOption!.pageObject, alertCustomDialogOption!.pageWebTag);
                }).backgroundColor(0xffffff).fontColor(Color.Blue)

                if (index !== undefined && index < (JSON.parse((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).buttonLabel) as Array<string>).length - 1) {
                  Divider().vertical(true).height(40).color('#182431').opacity(0.6).position({ x: '100%' }).margin({ left: -0.5 }) // 微调位置，让分割线在中间
                }
              }
              .width('100%') // 每个按钮容器平均分配宽度
              .layoutWeight(1) // 重要：让每个容器平均分配空间

            }, (item: string) => item)
        }
      }

      if((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).action == "prompt") {
        TextInput({
          text: (JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).defaultText
        })
          .width("90%").type(InputType.Normal)
          .onChange((value: string) => {
            changeText = value;
          }).margin({bottom: 10 })
        Text().border({ width: { bottom: 1 }, color: "#E0E0E0" }).width('100%')
        Flex({ justifyContent: FlexAlign.SpaceAround }) {
          ForEach(JSON.parse((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).buttonLabel) as Array<string>,
            (item: string, index?: number) => {
              Stack({ alignContent: Alignment.Center }) {
                Button(item).onClick(() => {
                  promptActionClass.closeDialog();
                  let result: ArkTsAttribute = {
                    content: "prompt",
                    result: [(JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).buttonLabel,
                      item, changeText]
                  };
                  capacitor.onArkTsResult(JSON.stringify(result), alertCustomDialogOption!.pageObject, alertCustomDialogOption!.pageWebTag);
                }).backgroundColor(0xffffff).fontColor(Color.Blue)

                if (index !== undefined && index < (JSON.parse((JSON.parse(alertCustomDialogOption!.pageArgs) as CustomDialogOption).buttonLabel) as Array<string>).length - 1) {
                  Divider().vertical(true).height(40).color('#182431').opacity(0.6).position({ x: '100%' }).margin({ left: -0.5 }) // 微调位置，让分割线在中间
                }

              }
              .width('100%') // 每个按钮容器平均分配宽度
              .layoutWeight(1) // 重要：让每个容器平均分配空间

            }, (item: string) => item)
        }
      }
    }.backgroundColor(0xffffff).width('90%').borderRadius(20);
  }.width('100%').height('100%')
  .justifyContent(FlexAlign.Center)
  .alignItems(HorizontalAlign.Center)
  .constraintSize({ maxWidth: 500 })
}

export async function NotificationAlert(pageIndex:NativeAttribute) {
  if(pageIndex.pageArgs == "beep") {
    for(let i=0; i<pageIndex.pageValue; i++) {
      const beepPlayer = new ArkBeepPlayer();
      await beepPlayer.playBeep(1000, 200, 1);
    }
    return;
  }
  
  try {
    if(!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
      return;
    }
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
    let uiContext:UIContext = mainPage.getUIContext();

    alertCustomDialogOption = pageIndex;

    let promptActionClass = new PromptActionClass();
    let audioContentNode: ComponentContent<Object> = new ComponentContent(uiContext, wrapBuilder(CustomerAlertDialog), promptActionClass);
    promptActionClass.setContext(uiContext);
    promptActionClass.setContentNode(audioContentNode);
    promptActionClass.setOptions({ alignment: DialogAlignment.Top, offset: { dx: 0, dy: 0},onWillDismiss:(dismissDialogAction: DismissDialogAction) => {
      // 处理侧滑关闭（PRESS_BACK包含左滑/右滑、三键back、键盘ESC）
      if (dismissDialogAction.reason === DismissReason.PRESS_BACK) {
        dismissDialogAction.dismiss(); // 主动关闭弹窗
      }
      // 处理下拉关闭（如半模态弹窗）
      if (dismissDialogAction.reason === DismissReason.SLIDE_DOWN) {
        dismissDialogAction.dismiss();
      }
    }
    });
    promptActionClass.setData(pageIndex);
    promptActionClass.openDialog();
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`Failed to injectScriptFileInAppBrowser. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
  }
}
