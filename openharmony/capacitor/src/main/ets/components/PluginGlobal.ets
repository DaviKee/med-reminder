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

import { HashMap, HashSet } from "@kit.ArkTS";
import { CordovaInterface, CordovaPlugin, CordovaWebView } from "./CordovaPlugin";
import { CapacitorPlugin } from "./getcapacitor/CapacitorPlugin";

//全局变量适用于所有插件和所有mainPage对象
export let cordovaPlugsGlobe:HashMap<string, HashMap<string, CordovaPlugin>> = new HashMap<string, HashMap<string, CordovaPlugin>>();
export let cordovaWebViewGlobe:HashMap<string, CordovaWebView> = new HashMap<string, CordovaWebView>();
export let cordovaInterfaceGlobe:HashMap<string, CordovaInterface> = new HashMap<string, CordovaInterface>();
export let cordovaWebIdToWebviewGlobe:HashMap<string, string> = new HashMap<string, string>();
export let cordovaWebTagToObjectGlobe:HashMap<string, object> = new HashMap<string, object>();

export let capacitorPluginsGlobe:HashMap<string, HashMap<string, CapacitorPlugin>> = new HashMap<string, HashMap<string, CapacitorPlugin>>();

// 定义Native侧和ArkTS进行交互的数据对象。
export  interface NativeAttribute {
  pageIndex: string;
  pageValue: number;
  pageArgs: string;
  pageObject:string;
  pageWebTag:string;
}

// 定义创建函数的返回值，用于ArkTS侧和Native侧的交互。
export interface ArkTsAttribute {
  result: Array<string>;
  content: string;
}