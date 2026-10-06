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

import { Action, ActionType, RegisterActionTypesOptions } from "./LocalNotificationModel";
import preferences from "@ohos.data.preferences";
import { NotificationAction } from "./NotificationAction";
import { BusinessError } from "@kit.BasicServicesKit";
import capacitor from 'libcapacitor.so';
import { NativeAttribute } from "../PluginGlobal";

export const ACTION_TYPES_ID = "ACTION_TYPE_STORE";

export async function writeActionGroup(params: RegisterActionTypesOptions, pageIndex: NativeAttribute) {
  for (let index = 0; index < params.types.length; index++) {
    let jsObject: ActionType = JSON.parse(JSON.stringify(params.types[index])) as ActionType
    let actionGroupId = jsObject.id
    if (actionGroupId == null) {
      return
    }
    let actions: Action[] = jsObject.actions
    let typesArray: NotificationAction[] = [];
    if (actions != null) {
      for (let index = 0; index < actions.length; index++) {
        let notificationAction: NotificationAction =
          new NotificationAction(actions[index].id, actions[index].title, actions[index].input)
        typesArray.push(notificationAction)
      }
    }
    try {
      const editor = await preferences.getPreferences(getContext(), ACTION_TYPES_ID + actionGroupId)
      await editor.clear().catch((error: BusinessError)=>{
        console.error(error.message)
      })
      await editor.put('count', typesArray.length)
      for (let index = 0; index < typesArray.length; index++) {
        await editor.put('id' + index, typesArray[index].id).catch((error: BusinessError)=>{
          console.error(error.message)
        })
        await editor.put('title' + index, typesArray[index].title).catch((error: BusinessError)=>{
          console.error(error.message)
        })
        await editor.put('input' + index, typesArray[index].input).catch((error: BusinessError)=>{
          console.error(error.message)
        })
      }
      await editor.flush()
      capacitor.onArkTsResult(JSON.stringify({ result: "" }),
        pageIndex.pageObject, pageIndex.pageWebTag);
    } catch (error) {
      const err = error as BusinessError
      capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
        pageIndex.pageWebTag);
    }
  }
}