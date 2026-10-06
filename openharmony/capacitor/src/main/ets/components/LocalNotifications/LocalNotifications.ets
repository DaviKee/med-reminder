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

import { cordovaWebTagToObjectGlobe, NativeAttribute } from "../PluginGlobal";
import { notificationManager } from '@kit.NotificationKit';
import { BusinessError } from "@kit.BasicServicesKit";
import capacitor from 'libcapacitor.so';
import { common, WantAgent, wantAgent } from "@kit.AbilityKit";
import { JSON } from "@kit.ArkTS";
import {
  DeliveredNotifications,
  DeliveredNotificationSchema,
  LocalNotificationDescriptor,
  LocalNotificationSchema,
  PendingLocalNotificationSchema,
  ScheduleOptions,
  ScheduleResult,
  TimeSchedule,
  RegisterActionTypesOptions,
  EnabledResult,
  PermissionStatus
} from "./LocalNotificationModel";
import { reminderAgentManager } from "@kit.BackgroundTasksKit";
import bundleManager from '@ohos.bundle.bundleManager';
import { image } from "@kit.ImageKit";
import { MainPage } from "../MainPage";
import { ACTION_TYPES_ID, writeActionGroup } from "./NotificationStorage";
import { preferences } from "@kit.ArkData";

let slotChannelId: number = 1

export async function load(pageIndex: NativeAttribute) {
  try {
    let mainPage = cordovaWebTagToObjectGlobe.get(pageIndex.pageWebTag) as MainPage
    let configObj = mainPage.getJsonObjectFromRawFileSync('capacitor.config.json')
    if (configObj !== null && configObj !== undefined) {
      const plugins = (configObj as Record<string, object>).plugins
      const notification = plugins ? (plugins as Record<string, object>).localNotifications : undefined;
      if (notification === undefined || notification === null || (Object.keys(notification).length == 0)) {
        return;
      }
      slotChannelId = (notification as Record<string, number>).channelId
    }
  } catch (error) {
    const err = error as BusinessError
    console.error(err.message)
  }
}

/**
 * 安排一个或多个本地通知。
 * @param pageIndex
 */
export async function schedule(pageIndex: NativeAttribute) {
  let result = await check(pageIndex, false)
  if (result) {
    let value = pageIndex.pageArgs
    let scheduleObj = JSON.parse(value) as ScheduleOptions
    let resultArray: LocalNotificationDescriptor[] = []
    for (let index = 0; index < scheduleObj.notifications.length; index++) {
      let timeSchedule: TimeSchedule = scheduleObj.notifications[index].schedule
      if (isScheduled(timeSchedule)) {
        if (compareDate(timeSchedule)) {
          let reminderId = await publishReminderNotification(scheduleObj.notifications[index], pageIndex)
          if (reminderId !== null && reminderId !== undefined) {
            resultArray.push({ id: reminderId as number })
          }
        } else {
          publishNow(scheduleObj.notifications[index], pageIndex)
        }
      } else {
        publishNow(scheduleObj.notifications[index], pageIndex)
      }
    }
    capacitor.onArkTsResult(JSON.stringify({ result: "success", content: { notifications: resultArray } }),
      pageIndex.pageObject, pageIndex.pageWebTag);
  } else {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: 'Notifications disabled' }),
      pageIndex.pageObject,
      pageIndex.pageWebTag);
  }
}

async function publishNow(notificationObj: LocalNotificationSchema,pageIndex: NativeAttribute) {
  if(notificationObj.id === undefined){
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: 'The ID in the parameters cannot be empty.' }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }
  let actionTypeId = notificationObj.actionTypeId
  let buttonArray: Array<notificationManager.NotificationActionButton> = await actionType(actionTypeId)
  let channelId = notificationManager.SlotType.SOCIAL_COMMUNICATION
  channelId = slotChannelId
  let contentType = notificationManager.ContentType.NOTIFICATION_CONTENT_BASIC_TEXT
  const notification: notificationManager.NotificationRequest = {
    id: notificationObj.id,
    notificationSlotType: channelId,
    sound: notificationObj.sound,
    content: {
      notificationContentType: contentType,
      //只能看到title和body
      normal: {
        title: notificationObj.title,
        text: notificationObj.body,
        additionalText: notificationObj.summaryText,
      }
    },
    groupName: notificationObj.group,
    tapDismissed: notificationObj.autoCancel,
    extraInfo: notificationObj.extra,
    actionButtons: buttonArray
  }
  if(notificationObj.largeBody !== undefined && notificationObj.largeBody !== ''){
    //只能看到longTitle和lines
    notification.content.longText = {
      title: notificationObj.title, //折叠时显示的title
      text: notificationObj.body,
      additionalText: notificationObj.summaryText,
      longText: notificationObj.largeBody, //文章内容
      briefText: notificationObj.summaryText,
      expandedTitle: notificationObj.title //展开时显示的title
    }
    notification.content.notificationContentType = notificationManager.ContentType.NOTIFICATION_CONTENT_LONG_TEXT
  }
  if(notificationObj.inboxList !== undefined && notificationObj.inboxList.length>0){
    //只能看到longTitle和lines
    notification.content.multiLine = {
      title: notificationObj.title, // 折叠时显示的title
      text: notificationObj.body, //折叠时显示的text
      briefText: notificationObj.summaryText,
      longTitle: notificationObj.title, //展开时显示的title
      lines: notificationObj.inboxList, //展开时显示的text最多三行
    }
    notification.content.notificationContentType = notificationManager.ContentType.NOTIFICATION_CONTENT_MULTILINE
  }
  if(notificationObj.smallIcon){
    let smallPixel: PixelMap | null = await CreatePixelMap(notificationObj.smallIcon)
    if(smallPixel !== null){
      notification.smallIcon = smallPixel
    }
  }
  if(notificationObj.largeIcon){
    let largePixel: PixelMap | null = await CreatePixelMap(notificationObj.largeIcon)
    if(largePixel !== null){
      notification.largeIcon = largePixel
    }
  }
  try {
    await notificationManager.publish(notification);
  } catch (error) {
    const err = error as BusinessError
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }
}

function isScheduled(obj: TimeSchedule): boolean {
  if(obj !== undefined && (obj.at !== undefined || obj.on !== undefined)){
    return true
  }
  return false
}

async function CreatePixelMap(imageName: string) {
  const context = getContext() as common.UIAbilityContext;
  const resourceManager = context.resourceManager;
  try {
    let resourceArray = await resourceManager.getMediaByName(imageName).catch((error: BusinessError) => {
      console.error(error.message)
    })
    if (resourceArray) {
      let imageSource = image.createImageSource(resourceArray.buffer);
      let opts: image.InitializationOptions = { editable: true, pixelFormat: 3, size: { height: 6, width: 6 } };
      const pixelMap = await imageSource.createPixelMap(opts);
      return pixelMap
    }
  } catch (error) {
    let err = error as BusinessError
    console.error(err.message)
  }
  return null
}

async function getWantAgent(): Promise<WantAgent> {
  let bundleInfo = await bundleManager.getBundleInfoForSelf(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION);
  let wantAgentInfo: wantAgent.WantAgentInfo = {
    wants: [{
      bundleName: bundleInfo.name,
      abilityName: 'EntryAbility',
      parameters: { key: 'test' }  // 可传递自定义参数
    }],
    actionType: wantAgent.OperationType.START_ABILITY,
    requestCode: 0
  };
  let wantAgentObj = await wantAgent.getWantAgent(wantAgentInfo);
  return wantAgentObj
}

async function actionType(actionTypeId: string) {
  let buttonArray: Array<notificationManager.NotificationActionButton> = []
  if (actionTypeId !== null && actionTypeId !== undefined) {
    try {
      const editor = await preferences.getPreferences(getContext(), ACTION_TYPES_ID + actionTypeId)
      let count = await editor.get('count', 0)
      for (let index = 0; index < count; index++) {
        let id = await editor.get('id' + index, '')
        let btnName = await editor.get('title' + index, '') as string
        let input = await editor.get('input' + index, false)
        let wantAgentObj = await getWantAgent()
        let userInput: notificationManager.NotificationUserInput = {
          inputKey: 'reply_key' // 定义用户输入的标识
        };
        if (input) {
          let actionButton: notificationManager.NotificationActionButton = {
            title: btnName,
            wantAgent: wantAgentObj,
            userInput: userInput,
            extras: [{ id: id }]
          }
          buttonArray.push(actionButton)
        } else {
          let actionButton: notificationManager.NotificationActionButton = {
            title: btnName,
            wantAgent: wantAgentObj,
            extras: [{ id: id }]
          }
          buttonArray.push(actionButton)
        }
      }
    } catch (error) {
      let err = error as BusinessError
    }
  }
  return buttonArray
}

export function registerActionTypes(pageIndex: NativeAttribute) {
  try {
    let params: RegisterActionTypesOptions = JSON.parse(pageIndex.pageArgs) as RegisterActionTypesOptions
    writeActionGroup(params, pageIndex)
  } catch (error) {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: 'Invalid JSON parameters' }), pageIndex.pageObject, pageIndex.pageWebTag);
  }
}

export async function getPending(pageIndex: NativeAttribute) {
  reminderAgentManager.getValidReminders().catch((error: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }).then((array) => {
    let reminderArray: Array<reminderAgentManager.ReminderRequestCalendar> =
      array as Array<reminderAgentManager.ReminderRequestCalendar>
    let tempArray: PendingLocalNotificationSchema[] = []
    for (let index = 0; index < reminderArray.length; index++) {
      let obj: reminderAgentManager.ReminderRequestCalendar = reminderArray[index]
      let paramsDate: Date = new Date()
      paramsDate.setFullYear(obj.dateTime.year)
      paramsDate.setMonth(obj.dateTime.month)
      paramsDate.setDate(obj.dateTime.day)
      paramsDate.setHours(obj.dateTime.hour)
      paramsDate.setMinutes(obj.dateTime.minute)
      paramsDate.setMilliseconds(obj.dateTime.second ?? 0)
      let content: PendingLocalNotificationSchema = {
        title: obj.title ?? '',
        body: obj.content ?? '',
        id: obj.notificationId ?? 0,
        schedule: {at: paramsDate.toISOString()}
      }
      tempArray.push(content)
    }
    capacitor.onArkTsResult(JSON.stringify({
      result: "success",
      content: { notifications: tempArray }
    }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  })
}

export async function cancelReminders(pageIndex: NativeAttribute) {
  const result: ScheduleResult = JSON.parse(pageIndex.pageArgs) as ScheduleResult
  if (result && result !== undefined) {
    for (let index = 0; index < result.notifications.length; index++) {
      const id = result.notifications[index].id;
      reminderAgentManager.cancelReminder(id).catch((err: BusinessError) => {
        capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
          pageIndex.pageWebTag);
      }).then(() => {
        capacitor.onArkTsResult(JSON.stringify({ result: "" }), pageIndex.pageObject, pageIndex.pageWebTag);
      })
    }
  } else {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: 'The notification id is incorrect' }),
      pageIndex.pageObject,
      pageIndex.pageWebTag);
  }
}

/**
 *检查是否启用了通知。
 * @param pageIndex
 */
export async function areEnabled(pageIndex: NativeAttribute) {
  notificationManager.isNotificationEnabled()
    .then((isEnabled: boolean) => {
      if (isEnabled) {
        capacitor.onArkTsResult(JSON.stringify({
          result: "success", content: ({ value: true } as EnabledResult)
        }), pageIndex.pageObject, pageIndex.pageWebTag);
      } else {
        capacitor.onArkTsResult(JSON.stringify({
          result: "success", content: ({ value: false } as EnabledResult)
        }), pageIndex.pageObject, pageIndex.pageWebTag);
      }
    })
    .catch((err: BusinessError) => {
      capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
        pageIndex.pageWebTag);
    });
}

/**
 * 获取通知屏幕上可见的通知列表。
 * @param pageIndex
 */
export async function getDeliveredNotifications(pageIndex: NativeAttribute) {
  notificationManager.getActiveNotifications().catch((error: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }).then((array: Array<notificationManager.NotificationRequest> | void) => {
    let tempArray: DeliveredNotificationSchema[] = []
    if (array) {
      for (let index = 0; index < array.length; index++) {
        let model = array[index]
        let title: string = ''
        let body: string = ''
        let lines: Array<string> = []
        if (model.content.normal != null && model.content.normal != undefined) {
          title = model.content.normal.title
          body = model.content.normal.text
        } else if (model.content.longText != null && model.content.longText != undefined) {
          title = model.content.longText.expandedTitle
          body = model.content.longText.longText
        } else if (model.content.multiLine != null && model.content.multiLine != undefined) {
          title = model.content.multiLine.longTitle
          lines = model.content.multiLine.lines
        }
        let content: DeliveredNotificationSchema = {
          id: model.id ?? 0,
          tag: "",
          title: title,
          body: body,
          group: model.groupName ?? '',
          groupSummary: false,
          extra: model.extraInfo ?? {},
          attachments: [],
          actionTypeId: "",
          sound: model.sound ?? '',
          data: "",
          schedule: { at: new Date(model.deliveryTime ?? 0).toISOString()},
          inboxList: lines
        }
        tempArray.push(content)
      }
      capacitor.onArkTsResult(JSON.stringify({
        result: "success",
        content: { notifications: tempArray }
      }), pageIndex.pageObject,
        pageIndex.pageWebTag);
    }
  })
}

/**
 * 从通知屏幕移除指定的通知。
 * @param pageIndex
 */
export async function removeDeliveredNotifications(pageIndex: NativeAttribute) {
  let model = JSON.parse(pageIndex.pageArgs) as DeliveredNotifications
  for (let index = 0; index < model.notifications.length; index++) {
    const element = model.notifications[index];
    notificationManager.cancel(element.id).catch((error: BusinessError) => {
      capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
        pageIndex.pageWebTag);
    }).then(() => {
      capacitor.onArkTsResult(JSON.stringify({ result: "success" }), pageIndex.pageObject, pageIndex.pageWebTag);
    })
  }
}

/**
 * 从通知屏幕移除所有通知
 * @param pageIndex
 */
export async function removeAllDeliveredNotifications(pageIndex: NativeAttribute) {
  notificationManager.cancelAll().catch((error: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  }).then(() => {
    capacitor.onArkTsResult(JSON.stringify({ result: "success" }), pageIndex.pageObject, pageIndex.pageWebTag);
  })
}

/**
 * 检查显示本地通知的权限。
 * @param pageIndex
 */
export async function checkPermissions(pageIndex: NativeAttribute) {
  check(pageIndex, true)
}

async function check(pageIndex: NativeAttribute, returnCall: boolean = true) {
  let isEnabled = await notificationManager.isNotificationEnabled().catch((err: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
    return false
  })
  if (returnCall) {
    if (isEnabled) {
      capacitor.onArkTsResult(JSON.stringify({
        result: "success", content: ({
          display: 'granted'
        } as PermissionStatus)
      }), pageIndex.pageObject, pageIndex.pageWebTag);
    } else {
      capacitor.onArkTsResult(JSON.stringify({
        result: "success", content: ({
          display: 'denied'
        } as PermissionStatus)
      }), pageIndex.pageObject, pageIndex.pageWebTag);
    }
  }
  return isEnabled
}

/**
 * 请求显示本地通知的权限。
 * @param pageIndex
 */
export async function requestPermissions(pageIndex: NativeAttribute) {
  let context = getContext() as common.UIAbilityContext;
  notificationManager.requestEnableNotification(context).then(() => {
    capacitor.onArkTsResult(JSON.stringify({
      result: "success", content: ({
        display: 'granted'
      } as PermissionStatus)
    }), pageIndex.pageObject, pageIndex.pageWebTag);
  }).catch((err: BusinessError) => {
    if (err.code === 1600004) {
      console.error('用户拒绝授权');
      capacitor.onArkTsResult(JSON.stringify({
        result: "success", content: ({
          display: 'denied'
        } as PermissionStatus)
      }), pageIndex.pageObject, pageIndex.pageWebTag);
    } else {
      console.error('请求失败，错误码: ' + err.code);
      capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: err.message }), pageIndex.pageObject,
        pageIndex.pageWebTag);
    }
  });
}

/**
 * 发送定时通知
 * @param notification
 * @param pageIndex
 * @returns
 */
async function publishReminderNotification(notification: LocalNotificationSchema, pageIndex: NativeAttribute):Promise<number | void> {
  let localDate: Date = new Date(notification.schedule.at);
  let lastChar = notification.schedule.at.slice(-1)
  if (lastChar === 'Z') {
    let utcStr = localDate.toUTCString()
    localDate = new Date(utcStr)
  }
  let bundleInfo = await bundleManager.getBundleInfoForSelf(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION);
  let calendar: reminderAgentManager.ReminderRequestCalendar = {
    reminderType: reminderAgentManager.ReminderType.REMINDER_TYPE_CALENDAR, // 提醒类型为日历类型
    notificationId: notification.id,
    dateTime: {
      // 指明提醒的目标时间
      year: localDate.getFullYear(),
      month: localDate.getMonth() + 1,
      day: localDate.getDate(),
      hour: localDate.getHours(),
      minute: localDate.getMinutes(),
      second: localDate.getSeconds(),
    },
    // 点击提醒通知后跳转的目标UIAbility信息
    wantAgent: { pkgName: bundleInfo.name, abilityName: 'EntryAbility' },
    ringDuration: 2,
    title: notification.title,
    content: notification.body,
    slotType: notificationManager.SlotType.SOCIAL_COMMUNICATION  // 指明提醒的Slot类型
  }
  return await reminderAgentManager.publishReminder(calendar).catch((error: BusinessError) => {
    capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), pageIndex.pageObject,
      pageIndex.pageWebTag);
  })
}

function compareDate(schedule: TimeSchedule): boolean {
  if(schedule.at !== undefined){
    let lastChar = schedule.at.slice(-1)
    let paramsDate: Date = new Date(schedule.at)
    if (lastChar === 'Z') {
      let utcStr = paramsDate.toUTCString()
      paramsDate = new Date(utcStr)
    }
    let currentDate: Date = new Date()
    //重复触发需要60秒的间隔
    if (paramsDate.getTime() < currentDate.getTime() + 60000) {
      return false
    } else {
      return true
    }
  } else {
    return false
  }
}


