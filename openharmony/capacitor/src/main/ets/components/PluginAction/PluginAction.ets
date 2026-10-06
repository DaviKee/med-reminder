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

import { CordovaInterface } from '../CordovaInterface';
import { CallbackContext, CordovaPlugin, PluginEntry } from '../CordovaPlugin';
import { CordovaWebView } from '../CordovaWebView';
import {
  capacitorPluginsGlobe,
  cordovaInterfaceGlobe, cordovaPlugsGlobe,
  cordovaWebIdToWebviewGlobe,
  cordovaWebTagToObjectGlobe,
  cordovaWebViewGlobe,
  NativeAttribute} from '../PluginGlobal';
import { HashMap } from '@kit.ArkTS';
import capacitor from 'libcapacitor.so';
import { CordovaPreferences } from '../CordovaPreferences';
import { NormalizeError } from '../NormalizeError';
import { CapacitorPlugin } from '../getcapacitor/CapacitorPlugin';
import { PluginCall } from '../getcapacitor/PluginCall';

interface IPluginEntry {
  serviceName:string;
  className:string;
  onload:boolean;
}

export function InitArkTsPlugins(webId:string, webTag:string, cordovaPreferences:CordovaPreferences, cordovaInterface:CordovaInterface, cordovaWebView:CordovaWebView, cordovaPlugs: Array<PluginEntry>, capacitorPlugins?:Array<PluginEntry>): void {
  try {
    if (!cordovaPlugsGlobe.hasKey(webTag)) {
      let strPluginEntry: string = capacitor.GetPluginEntry();
      let pluginEntryArray: Array<IPluginEntry> = JSON.parse(strPluginEntry) as Array<IPluginEntry>;
      let pluginNames: Array<string> = new Array<string>();
      if (cordovaPlugs != null && cordovaPlugs != undefined) {
        for (let pluginEntry of cordovaPlugs) {
          pluginNames.push(pluginEntry.pluginName);
          let plugin: CordovaPlugin = pluginEntry.pluginObject as CordovaPlugin;
          plugin!.privateInitialize(webId, pluginEntry.pluginName, cordovaPreferences, cordovaInterface, cordovaWebView,
            cordovaPlugsGlobe,
            cordovaWebViewGlobe, cordovaInterfaceGlobe, cordovaWebIdToWebviewGlobe);
          let i = 0;
          for (; i < pluginEntryArray.length; i++) {
            if (pluginEntryArray[i].serviceName == pluginEntry.pluginName) {
              break;
            }
          }

          if (i >= pluginEntryArray.length) {
            console.warn("Warn:" + pluginEntry.pluginName + " is not configured in the config.xml");
          }
          if (cordovaPlugsGlobe.hasKey(webTag)) {
            let mapPlug: HashMap<string, CordovaPlugin> = cordovaPlugsGlobe.get(webTag);
            mapPlug.set(pluginEntry.pluginName, plugin);
            cordovaPlugsGlobe.set(webTag, mapPlug);
          } else {
            let mapPlug: HashMap<string, CordovaPlugin> = new HashMap<string, CordovaPlugin>();
            mapPlug.set(pluginEntry.pluginName, plugin);
            cordovaPlugsGlobe.set(webTag, mapPlug);
          }
        }
      }

      let capPlugins: Array<object> = new Array<object>();
      if(capacitorPlugins != null && capacitorPlugins != undefined) {
        for (let pluginEntry of capacitorPlugins) {
          let plugin: CapacitorPlugin = pluginEntry.pluginObject as CapacitorPlugin;
          plugin!.privateInitialize(webId, pluginEntry.pluginName, cordovaPreferences, cordovaInterface, cordovaWebView,
            capacitorPluginsGlobe,
            cordovaWebViewGlobe, cordovaInterfaceGlobe, cordovaWebIdToWebviewGlobe);
          let objPlug:object = new Object();
          objPlug[pluginEntry.pluginName] = plugin.getAllMethods();
          capPlugins.push(objPlug);
          if (capacitorPluginsGlobe.hasKey(webTag)) {
            let mapPlug: HashMap<string, CapacitorPlugin> = capacitorPluginsGlobe.get(webTag);
            mapPlug.set(pluginEntry.pluginName, plugin);
            capacitorPluginsGlobe.set(webTag, mapPlug);
          } else {
            let mapPlug: HashMap<string, CapacitorPlugin> = new HashMap<string, CapacitorPlugin>();
            mapPlug.set(pluginEntry.pluginName, plugin);
            capacitorPluginsGlobe.set(webTag, mapPlug);
          }
        }
      }
      capacitor.SetPluginEntry(webTag, JSON.stringify(pluginNames), JSON.stringify(capPlugins));
    }

    cordovaWebViewGlobe.set(webId, cordovaWebView);
    cordovaInterfaceGlobe.set(webId, cordovaInterface);
    cordovaWebIdToWebviewGlobe.set(webId, webTag);

  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`InitArkTsPlugins failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}

export function ExecServicePlugin(pageIndex:NativeAttribute):void {
  if(pageIndex.pageIndex.indexOf("/service") != 0) {
    return;
  }

  let pageArgs:string[] = pageIndex.pageIndex.substring(1).split("/");
  let webTag:string = pageArgs[1];
  let actionName:string = pageArgs[2];
  let callbackId:string = pageArgs[3];
  let jsonArgs:ESObject = new Object();
  try {
    jsonArgs = JSON.parse(pageIndex.pageArgs);
  } catch (error) {
    console.warn(`JSON.parse(${pageIndex.pageArgs})`);
  }
  let callbackContext = new CallbackContext(webTag, pageIndex.pageObject, callbackId);

  //执行所有插件
  try {
    if (webTag == "*") {
      //消息通知所有ArkTs侧插件
      if (actionName == "onMessage") {
        cordovaPlugsGlobe.forEach((mapPlug, key) => {
          mapPlug?.forEach((cordovaPlugin, key) => {
            cordovaPlugin?.onMessage(jsonArgs?.["id"], jsonArgs?.["data"]);
          });
        });
      }
      if (actionName == "onPause") {
        cordovaPlugsGlobe.forEach((mapPlug, key) => {
          mapPlug?.forEach((cordovaPlugin, key) => {
            cordovaPlugin?.onPause(Boolean(pageIndex.pageValue));
          });
        });
      }
      if (actionName == "onResume") {
        cordovaPlugsGlobe.forEach((mapPlug, key) => {
          mapPlug?.forEach((cordovaPlugin, key) => {
            cordovaPlugin?.onResume(Boolean(pageIndex.pageValue));
          });
        });
      }
      return;
    }

    if (cordovaPlugsGlobe.hasKey(webTag)) {
      let mapPlug: HashMap<string, CordovaPlugin> = cordovaPlugsGlobe.get(webTag);
      if (actionName == "onStart") {
        mapPlug?.forEach((cordovaPlugin, key) => {
          cordovaPlugin?.onStart();
        });
        return;
      }
      if (actionName == "onPageStart") {
        mapPlug?.forEach((cordovaPlugin, key) => {
          cordovaPlugin?.onPageStart();
        });
        return;
      }
      if (actionName == "onPageEnd") {
        mapPlug?.forEach((cordovaPlugin, key) => {
          cordovaPlugin?.onPageEnd();
        });
        return;
      }
      if (mapPlug.hasKey(pageIndex.pageObject)) {
        let plugin: CordovaPlugin = mapPlug.get(pageIndex.pageObject);
        plugin!.execute(actionName, jsonArgs, callbackContext);
      }
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`ExecServicePlugin failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }

}

export function ExecCapacitorPlugin(pageIndex:NativeAttribute):void {
  if(pageIndex.pageIndex.indexOf("/capacitor") != 0) {
    return;
  }
  let pageArgs:string[] = pageIndex.pageIndex.substring(1).split("/");
  let webTag:string = pageArgs[1];
  let actionName:string = pageArgs[2];
  let callbackId:string = pageArgs[3];
  let jsonArgs:object = new Object();
  try {
    jsonArgs = JSON.parse(pageIndex.pageArgs);
  } catch (error) {
    console.warn(`JSON.parse(${pageIndex.pageArgs})`);
  }

  let call:PluginCall = new PluginCall(webTag, pageIndex.pageObject, actionName, callbackId, jsonArgs);

  try {
    if (webTag == "*") {
      if (actionName == "handleOnPause") {
        capacitorPluginsGlobe.forEach((mapPlug, key) => {
          mapPlug?.forEach((capacitorPlugin, key) => {
            capacitorPlugin?.handleOnPause();
          });
        });
      }
      if (actionName == "handleOnResume") {
        capacitorPluginsGlobe.forEach((mapPlug, key) => {
          mapPlug?.forEach((capacitorPlugin, key) => {
            capacitorPlugin?.handleOnResume();
          });
        });
      }
      return;
    }
    
    if (capacitorPluginsGlobe.hasKey(webTag)) {
      let mapPlug: HashMap<string, CapacitorPlugin> = capacitorPluginsGlobe.get(webTag);
      if (actionName == "handleOnStart") {
        mapPlug?.forEach((capacitorPlugin, key) => {
          capacitorPlugin?.handleOnStart();
        });
        return;
      }
      if (actionName == "handleOnPageStart") {
        mapPlug?.forEach((capacitorPlugin, key) => {
          capacitorPlugin?.handleOnPageStart();
        });
        return;
      }
      if (actionName == "handleOnEnd") {
        mapPlug?.forEach((capacitorPlugin, key) => {
          capacitorPlugin?.handleOnEnd();
        });
        return;
      }

      if (actionName == "handleOnDestroy") {
        mapPlug?.forEach((capacitorPlugin, key) => {
          capacitorPlugin?.handleOnDestroy();
        });
        return;
      }
      
      if (mapPlug.hasKey(pageIndex.pageObject)) {
        let plugin: CapacitorPlugin = mapPlug.get(pageIndex.pageObject);
        plugin!.callMethod(actionName, call);
      }
    }
    
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`ExecCapacitorPlugin failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}

export function NotifyArkTsPluginDestroy(webTag:string) {
  try {
    if(cordovaPlugsGlobe.hasKey(webTag)) {
      let mapPlug:HashMap<string, CordovaPlugin> = cordovaPlugsGlobe.get(webTag);
      mapPlug?.forEach((cordovaPlugin, key)=>{
        cordovaPlugin?.onDestroy();
      });
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`NotifyArkTsPluginDestroy failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }

  try {
    if(capacitorPluginsGlobe.hasKey(webTag)) {
      let mapPlug:HashMap<string, CapacitorPlugin> = capacitorPluginsGlobe.get(webTag);
      mapPlug?.forEach((capacitorPlugin, key)=>{
        capacitorPlugin?.handleOnDestroy();
      });
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`NotifyArkTsPluginDestroy failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }

}

export function removeWebAndPlugins(webId:string) {
  try {
    if(cordovaWebViewGlobe.hasKey(webId)) {
      cordovaWebViewGlobe.remove(webId);
    }
    if(cordovaInterfaceGlobe.hasKey(webId)) {
      cordovaInterfaceGlobe.remove(webId);
    }

    let webTag:string = "";
    if(cordovaWebIdToWebviewGlobe.hasKey(webId)) {
      webTag = cordovaWebIdToWebviewGlobe.get(webId);
      cordovaWebIdToWebviewGlobe.remove(webId);
    }

    if(webTag != "") {
      if(cordovaWebTagToObjectGlobe.hasKey(webTag)) {
        cordovaWebTagToObjectGlobe.remove(webTag);
      }

      if(cordovaPlugsGlobe.hasKey(webTag)) {
        cordovaPlugsGlobe.remove(webTag);
      }
    }
  } catch (error) {
    const cordovaError = NormalizeError(error);
    console.error(`removeWebAndPlugins failed, code is ${cordovaError.code}, message is ${cordovaError.message}`);
  }
}
