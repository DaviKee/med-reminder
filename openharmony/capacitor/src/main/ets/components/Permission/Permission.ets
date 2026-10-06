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

import { abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';
import { bundleManager } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { NativeAttribute } from '../PluginGlobal';


export async function checkPermissionGrant(permission: Permissions): Promise<abilityAccessCtrl.GrantStatus> {
  let atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
  let grantStatus: abilityAccessCtrl.GrantStatus = abilityAccessCtrl.GrantStatus.PERMISSION_DENIED;
  // 获取应用程序的accessTokenID
  return new Promise((resolve, reject) => {
    try {
      bundleManager.getBundleInfoForSelf(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION).then((info) => {
        try {
          // 校验应用是否被授予权限
          grantStatus = atManager.verifyAccessTokenSync(info.appInfo.accessTokenId, permission);
          resolve(grantStatus);
        } catch (error) {
          const err: BusinessError = error as BusinessError;
          console.error(`Failed to check access token. Code is ${err.code}, message is ${err.message}`);
          resolve(grantStatus);
        }
      });
    } catch (error) {
      const err: BusinessError = error as BusinessError;
      console.error(`Failed to get bundle info for self. Code is ${err.code}, message is ${err.message}`);
      resolve(grantStatus);
    }
  });
}

// 使用UIExtensionAbility：将common.UIAbilityContext 替换为common.UIExtensionContext
export async function reqPermissionsFromUser(permissions: Array<Permissions>, context: common.UIAbilityContext, pageIndex?:NativeAttribute):Promise<number> {
  let atManager: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
  // requestPermissionsFromUser会判断权限的授权状态来决定是否唤起弹窗
  return new Promise((resolve, reject) => {
    atManager.requestPermissionsFromUser(context, permissions).then((data) => {
      let grantStatus: Array<number> = data.authResults;
      let length: number = grantStatus.length;
      for (let i = 0; i < 1; i++) {
        if (grantStatus[i] === 0) {
          // 用户授权，可以继续访问目标操作
          resolve(1);
        } else {
          // 用户拒绝授权，提示用户必须授权才能访问当前页面的功能，并引导用户到系统设置中打开相应的权限
          atManager.requestPermissionOnSetting(context, permissions).then((data: Array<abilityAccessCtrl.GrantStatus>) => {
            if(data.length > 0 && data[0] == 0) {
              resolve(1);
            } else {
              resolve(0);
            }
            console.info('data:' + JSON.stringify(data));
          }).catch((err: BusinessError) => {
            console.error('data:' + JSON.stringify(err));
            resolve(0);
          });
        }
      }
      // 授权成功
    }).catch((err: BusinessError) => {
      console.error(`Failed to request permissions from user. Code is ${err.code}, message is ${err.message}`);
      resolve(0);
    })
  });
}
