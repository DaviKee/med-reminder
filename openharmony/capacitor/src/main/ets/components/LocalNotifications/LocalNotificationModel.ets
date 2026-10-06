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

type ScheduleEvery = 'year' | 'month' | 'two-weeks' | 'week' | 'day' | 'hour' | 'minute' | 'second';

export interface LocalNotificationDescriptor {
  id: number
}

export interface Attachment {
  id: string;
  url: string;
}

export interface ScheduleResult {
  notifications: LocalNotificationDescriptor[]
}

export interface LocalNotificationSchema {
  title: string;
  body: string;
  largeBody: string;
  summaryText: string;
  id: number;
  schedule: TimeSchedule;
  sound: string;
  smallIcon: string;
  largeIcon: string;
  iconColor: string;
  attachments: Attachment[];
  actionTypeId: string;
  extra: Object;
  threadIdentifier: string;
  summaryArgument: string;
  group: string;
  groupSummary: boolean;
  channelId: string;
  ongoing: boolean;
  autoCancel: boolean;
  inboxList: string[];
  silent: boolean;
  contentType: number;
}

export interface ScheduleOptions {
  notifications: LocalNotificationSchema[]
}

export interface PendingLocalNotificationSchema {
  title: string;
  body: string;
  id: number;
  schedule: TimeSchedule;
  extra?: Object;
}

export interface PendingResult {
  notifications: PendingLocalNotificationSchema[]
}

export interface DeliveredNotificationSchema {
  id: number;
  tag: string;
  title: string;
  body: string;
  group: string;
  groupSummary: boolean;
  data: string;
  extra: Record<string, Object>;
  attachments: Attachment[];
  actionTypeId: string;
  schedule: TimeSchedule;
  sound: string;
  inboxList: string[]
}


export interface DeliveredNotifications {
  notifications: DeliveredNotificationSchema[]
}

export interface CancelOptions {
  notifications: LocalNotificationDescriptor[]
}

enum Weekday {
  Sunday = 1,
  Monday,
  Tuesday,
  Wednesday,
  Thursday,
  Friday,
  Saturday
}

export interface ScheduleOn {
  year: number;
  month: number;
  day: number;
  weekday: Weekday;
  hour: number;
  minute: number;
  second: number;
}

export interface TimeSchedule {
  at: string;
  repeats?: boolean;
  allowWhileIdle?: boolean;
  on?: ScheduleOn;
  every?: ScheduleEvery;
  count?: number
}

export interface Action {
  id: string
  title: string
  input: boolean
}

export interface ActionType {
  id: string
  actions: Action[]
}

export interface RegisterActionTypesOptions {
  types: ActionType[]
}

type PermissionState = 'prompt' | 'prompt-with-rationale' | 'granted' | 'denied';

export interface PermissionStatus {
  display: PermissionState
}

export interface EnabledResult {
  value: boolean
}
