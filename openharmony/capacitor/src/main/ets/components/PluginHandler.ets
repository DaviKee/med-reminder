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

import { cordovaWebTagToObjectGlobe, NativeAttribute} from './PluginGlobal'
import { ExecServicePlugin, ExecCapacitorPlugin} from './PluginAction/PluginAction';
import {
  BackHistory,
  ClearCache,
  ClearHistory,
  LoadUrl,
  OverrideButton} from './CoreHarmony/CoreHarmony';
import { ShowSplashScreen } from './SplashScreen/SplashScreen';
import { MainPage } from './MainPage';
import { NormalizeError } from './NormalizeError';

export async function PluginHandler(nativeAttribute:NativeAttribute): Promise<void> {
  if(nativeAttribute.pageIndex.indexOf("/service") == 0) {
    ExecServicePlugin(nativeAttribute);
    return;
  }

  if(nativeAttribute.pageIndex.indexOf("/capacitor") == 0) {
    ExecCapacitorPlugin(nativeAttribute);
    return;
  }

  //最后一个斜杠之后是函数名，最后一个斜杠之前是模块名
  if(nativeAttribute.pageIndex.indexOf("/pages") != 0) {
    let pos:number = nativeAttribute.pageIndex.lastIndexOf("/");
    let module:string = nativeAttribute.pageIndex.substring(0, pos);
    let functionName = nativeAttribute.pageIndex.substring(pos+1);
    import(module).then((ns:ESObject) => {
      ns[functionName](nativeAttribute);
    });
    return;
  }

  if(nativeAttribute.pageIndex == "/pages/Exit") {
    try {
      if (!cordovaWebTagToObjectGlobe.hasKey(nativeAttribute.pageWebTag)) {
        return;
      }
      let mainPage = cordovaWebTagToObjectGlobe.get(nativeAttribute.pageWebTag) as MainPage;
      mainPage.getUIContext().getHostContext()!.getApplicationContext().killAllProcesses();
    }catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to PluginHandler. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  if(nativeAttribute.pageIndex == "/pages/ClearCache") {
    ClearCache(nativeAttribute);
  }

  if(nativeAttribute.pageIndex == "/pages/ClearHistory") {
    ClearHistory(nativeAttribute);
  }

  if(nativeAttribute.pageIndex == "/pages/BackHistory") {
    BackHistory(nativeAttribute);
  }

  if(nativeAttribute.pageIndex == "/pages/OverrideButton") {
    OverrideButton(nativeAttribute);
  }

  if(nativeAttribute.pageIndex == "/pages/LoadUrl") {
    LoadUrl(nativeAttribute);
  }

  if(nativeAttribute.pageIndex == "/pages/SplashScreenShow") {
    ShowSplashScreen(nativeAttribute);
  }
}