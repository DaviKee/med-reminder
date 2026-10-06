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

import { ArkTsAttribute, cordovaWebTagToObjectGlobe, NativeAttribute } from "../PluginGlobal";
import { abilityAccessCtrl, common, Permissions } from "@kit.AbilityKit";
import { checkPermissionGrant, reqPermissionsFromUser } from "./Permission";
import capacitor from 'libcapacitor.so';
import { MainPage } from "../MainPage";
import { NormalizeError } from "../NormalizeError";

export async function OHPermission(pageIndex:NativeAttribute) {
  if(pageIndex.pageValue == 0) {
    let grantResult:object = new Object();
    let permissions:Array<Permissions> = JSON.parse(pageIndex.pageArgs);
    for(let i=0; i<permissions.length; i++) {
      let grantStatus: abilityAccessCtrl.GrantStatus = await checkPermissionGrant(permissions[i]);
      if(grantStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        grantResult[permissions[i]] = true;
      } else {
        grantResult[permissions[i]] = false;
      }
    }
    let result: ArkTsAttribute = {content:"has", result:[JSON.stringify(grantResult)]};
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  }

  if(pageIndex.pageValue == 1) {
    try {
      if(!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
        return;
      }
      let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
      let context = mainPage.getUIContext().getHostContext() as common.UIAbilityContext;
      let grantResult:object = new Object();
      let permissions:Array<Permissions> = JSON.parse(pageIndex.pageArgs);
      for(let i=0; i<permissions.length; i++) {
        let permission:Array<Permissions> = new Array<Permissions>();
        permission.push(permissions[i]);
        let grantStatus = await reqPermissionsFromUser(permission, context, pageIndex);
        grantResult[permissions[i]] = grantStatus==1?true:false;
      }
      let result: ArkTsAttribute = {content:"request", result:[JSON.stringify(grantResult)]};
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to OHPermission. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }
}

export async function HarmonyPermission(pageIndex:NativeAttribute) {
  if(pageIndex.pageValue == 0) {
    let grantResult:boolean = false;
    let permissions:Array<Permissions> = JSON.parse(pageIndex.pageArgs);
    for(let i=0; i<permissions.length; i++) {
      let grantStatus: abilityAccessCtrl.GrantStatus = await checkPermissionGrant(permissions[i]);
      if(grantStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        grantResult = true;
      } else {
        grantResult = false;
      }
    }
    let result: ArkTsAttribute = {content:"checkPermission", result:[grantResult+""]};
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  }

  if(pageIndex.pageValue == 1) {
    try {
      if(!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
        return;
      }
      let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
      let context = mainPage.getUIContext().getHostContext() as common.UIAbilityContext;
      let grantResult:boolean = false;
      let permissions:Array<Permissions> = new Array<Permissions>();
      permissions.push(pageIndex.pageArgs as Permissions);
      for(let i=0; i<permissions.length; i++) {
        let permission:Array<Permissions> = new Array<Permissions>();
        permission.push(permissions[i]);
        let grantStatus = await reqPermissionsFromUser(permission, context, pageIndex);
        grantResult = grantStatus==1?true:false;
      }
      let result: ArkTsAttribute = {content:"requestPermission", result:[grantResult+""]};
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to HarmonyPermission. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }

  if(pageIndex.pageValue == 2) {
    try {
      if(!cordovaWebTagToObjectGlobe.hasKey(pageIndex.pageWebTag)) {
        return;
      }
      let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage;
      let context = mainPage.getUIContext().getHostContext() as common.UIAbilityContext;
      let grantResult:boolean = false;
      let permissions:Array<Permissions> = JSON.parse(pageIndex.pageArgs);
      for(let i=0; i<permissions.length; i++) {
        let permission:Array<Permissions> = new Array<Permissions>();
        permission.push(permissions[i]);
        let grantStatus = await reqPermissionsFromUser(permission, context, pageIndex);
        grantResult = grantStatus==1?true:false;
      }
      let result: ArkTsAttribute = {content:"requestPermissions", result:[grantResult+""]};
      capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
    } catch (error) {
      const cordovaError = NormalizeError(error);
      console.error(`Failed to HarmonyPermission. Cause code: ${cordovaError.code}, message: ${cordovaError.message}`);
    }
  }
}