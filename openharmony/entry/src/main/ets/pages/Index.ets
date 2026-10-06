/*
 * Copyright (c) 2026 Huawei Device Co., Ltd and iSoftStone Information Technology(Group)Co.,Ltd.
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

import { MainPage, pageBackPress, pageHideEvent, pageShowEvent, PluginEntry,MainPageOnBackPress} from 'harmony-capacitor';

@Entry
@Component
struct Index {
  //ArkTs侧的自定义插件：配置插件名称和对象,请查看自定义查看开发部分
  cordovaPlugs:Array< PluginEntry> = [];
  mainPageOnBackPress:MainPageOnBackPress = new MainPageOnBackPress();

  
  onPageShow(){
    pageShowEvent(); //页面显示通知capacitor
  }
  onBackPress() {
    return this.mainPageOnBackPress.backPress();
  }
  onPageHide() {
    pageHideEvent(); //页面隐藏通知capacitor
  }
  build() {
    RelativeContainer() {
      //默认加载rawfile/www/index.html
      //如果要指定加载页面参考高级功能部分
      MainPage({isWebDebug:false,cordovaPlugs:this.cordovaPlugs,backPress:this.mainPageOnBackPress});
    }
    .height('100%')
    .width('100%')
  }
}