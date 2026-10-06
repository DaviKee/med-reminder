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
import { abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';
import { ArkTsAttribute, NativeAttribute } from '../PluginGlobal';
import capacitor from 'libcapacitor.so';
import { checkPermissionGrant, reqPermissionsFromUser } from '../Permission/Permission';

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
    let context = getContext() as common.UIAbilityContext;
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
  }
}

interface PermissionResult {
  permission: Permissions;
  result: boolean;
}

interface ArkTsAttributePermissionEx {
  content: string;
  result: PermissionResult[];
}

export async function HarmonyPermission(pageIndex:NativeAttribute) {
  if(pageIndex.pageValue == 0) {
    let grantResult:boolean = false;
    let permissions:Array<Permissions> = JSON.parse(pageIndex.pageArgs);

    const results: PermissionResult[] = [];
    for(let i=0; i<permissions.length; i++) {
      const p = permissions[i];
      let grantStatus: abilityAccessCtrl.GrantStatus = await checkPermissionGrant(p);
      if(grantStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        grantResult = true;
      } else {
        grantResult = false;
      }
      results.push({ permission: p, result: grantResult });
    }

    let result: ArkTsAttributePermissionEx = {
      content: "checkPermission",
      result: results
    };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  }

  if(pageIndex.pageValue == 1) {
    let context = getContext() as common.UIAbilityContext;
    let grantResult:boolean = false;
    let permissions:Array<Permissions> = new Array<Permissions>();
    permissions.push(pageIndex.pageArgs as Permissions);
    const results: PermissionResult[] = [];
    for(let i=0; i<permissions.length; i++) {
      let permission:Array<Permissions> = new Array<Permissions>();
      permission.push(permissions[i]);
      let grantStatus = await reqPermissionsFromUser(permission, context, pageIndex);
      grantResult = grantStatus==1?true:false;
      results.push({ permission: permissions[i], result: grantResult });
    }

    let result: ArkTsAttributePermissionEx = {
      content: "requestPermission",
      result: results
    };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  }

  if(pageIndex.pageValue == 2) {
    let context = getContext() as common.UIAbilityContext;
    let grantResult:boolean = false;
    let permissions:Array<Permissions> = JSON.parse(pageIndex.pageArgs);
    const results: PermissionResult[] = [];
    for(let i=0; i<permissions.length; i++) {
      let permission:Array<Permissions> = new Array<Permissions>();
      permission.push(permissions[i]);
      let grantStatus = await reqPermissionsFromUser(permission, context, pageIndex);
      grantResult = grantStatus==1?true:false;
      results.push({ permission: permissions[i], result: grantResult });
    }

    let result: ArkTsAttributePermissionEx = {
      content: "requestPermission",
      result: results
    };
    capacitor.onArkTsResult(JSON.stringify(result), pageIndex.pageObject, pageIndex.pageWebTag);
  }

}
