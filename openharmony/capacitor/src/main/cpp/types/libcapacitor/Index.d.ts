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

import { resourceManager } from "@kit.LocalizationKit";

export const RegisterCustomSchemes: (customSchemes:string) => void;
export const SetSchemeHandler: (customSchemes:string, schemeHandler:string) => void;
export const InitCordova:(webTag:string, tmpUrl:string, onRouteJump:Function, databaseDir:string,resManager: resourceManager.ResourceManager, customHttpHeaders:string, isAllowCredentials:boolean, basePath:string) => string;
export const GetPluginEntry: () => string;
export const SetPluginEntry:(webTag:string, strPlugin:string, strCapacitorPlugin:string) => void;
export const SetResourceReplace:(strWebTag:string, strRes:string, strObj:string) => void;
export const SetCordovaProtocolUrl:(url:string) => void;
export const onJsPrompt:(webTag:string, origin:string,message:string,defaultValue:string) => string;
export const onArkTsResult:(jsonResult:string, service:string, webTag:string) => void;
export const getCapacitorInjectJs:(webTag:string, basePath:string) => string;