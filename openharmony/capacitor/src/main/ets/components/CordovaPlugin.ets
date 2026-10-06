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

import { CordovaInterface } from './CordovaInterface'
import { CallbackContext } from './CallbackContext'
import { CordovaWebView } from './CordovaWebView'
import { HashMap } from '@kit.ArkTS'
import { CordovaPreferences } from './CordovaPreferences'
import { CapacitorPlugin } from '../../../../Index'

export { CordovaInterface} from './CordovaInterface'
export { MessageStatus, PluginResult } from './PluginResult'
export { CallbackContext } from './CallbackContext'
export { CordovaWebView } from './CordovaWebView'

export  class CordovaPlugin {
  protected webId?:string;
  protected cordovaWebView?: CordovaWebView;
  protected preferences?:CordovaPreferences;
  protected cordovaInterface?: CordovaInterface;
  protected serviceName?: string;
  //HashMap<webId, HashMap<插件名称, 实例对象>>，插件内可以通过此属性获取所有webview的自定义插件，不包含系统系统自带插件
  protected mapWebIdToCustomPlugins?:HashMap<string, HashMap<string, CordovaPlugin>>;
  //插件内通过此属性获取所有webview的cordovaWebView,可以控制所有webview的属性
  protected mapWebIdToWebView?:HashMap<string, CordovaWebView>;
  //插件内通过此属性获取webview的CordovaInterface，可以控制所有webview的页面路由栈
  protected mapWebIdToInterface?:HashMap<string, CordovaInterface>;
  //webId和webTag的对应关系，webId可以在父组件中自定义，webTag为cordova内部使用，不能修改。
  protected mapWebIdToWebTag?:HashMap<string, string>;

  privateInitialize(webId:string,
            serviceName: string,
            preferences:CordovaPreferences,
            cordovaInterface: CordovaInterface,
            cordovaWebView:CordovaWebView,
            mapWebIdToCustomPlugins:HashMap<string, HashMap<string, CordovaPlugin>>,
            mapWebIdToWebView:HashMap<string, CordovaWebView>,
            mapWebIdToInterface:HashMap<string, CordovaInterface>,
    mapWebIdToWebTag:HashMap<string, string>): void {
    this.webId = webId;
    this.serviceName = serviceName;
    this.preferences = preferences;
    this.cordovaInterface = cordovaInterface;
    this.cordovaWebView = cordovaWebView;
    this.mapWebIdToCustomPlugins = mapWebIdToCustomPlugins;
    this.mapWebIdToWebView = mapWebIdToWebView;
    this.mapWebIdToInterface = mapWebIdToInterface;
    this.mapWebIdToWebTag = mapWebIdToWebTag;
    this.initialize(cordovaInterface, cordovaWebView);
  }

  initialize(cordovaInterface: CordovaInterface, cordovaWebView:CordovaWebView):void{
    return;
  }
  execute(action: string, args: ESObject[], callbackContext: CallbackContext):boolean {
    return true;
  }
  onMessage(id:string, data:string):void {
  }
  onDestroy():void {
  }
  onPause(multitasking:boolean):void {
  }
  onResume(multitasking:boolean):void {
  }
  onStart():void {
  }
  onPageStart():void {
  }
  onPageEnd():void {
  }
}

export interface PluginEntry {
  pluginName: string
  pluginObject:CordovaPlugin|CapacitorPlugin
}