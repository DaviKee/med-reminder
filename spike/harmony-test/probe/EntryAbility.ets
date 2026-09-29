/*
 * Copyright (c) 2026 Huawei Device Co., Ltd and iSoftStone Information Technology(Group)Co.,Ltd.
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
 
import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { webview } from '@kit.ArkWeb';
import { reminderAgentManager } from '@kit.BackgroundTasksKit';
import { notificationManager } from '@kit.NotificationKit';
import { BusinessError } from '@kit.BasicServicesKit';

const DOMAIN = 0x0000;

// ===== S-0 追加：代理提醒（reminderAgentManager）可用性实测 =====
// 目的：验证官方所说「普通应用没有代理提醒使用权限则上限为 0」是否成立，
//       以及 getValidReminders / getAllValidReminders / subscribeReminderState 的实际差异。
// 结果全部打到 hilog，tag = SPIKE-REMINDER
const SPIKE_TAG = 'SPIKE-REMINDER';

function slog(msg: string): void {
  hilog.info(DOMAIN, SPIKE_TAG, '%{public}s', msg);
}

async function runReminderSpike(): Promise<void> {
  slog('========== BEGIN reminder spike ==========');

  // 0) 通知是否使能（1700001 的前置条件）
  try {
    const en: boolean = await notificationManager.isNotificationEnabled();
    slog('0) isNotificationEnabled = ' + en);
  } catch (e) {
    const be = e as BusinessError;
    slog('0) isNotificationEnabled FAIL  code=' + be.code + '  msg=' + be.message);
  }

  // 1) 发布一个 2 分钟后的 TIMER 提醒（最核心的一条）
  let rid: number = -1;
  try {
    const req: reminderAgentManager.ReminderRequestTimer = {
      reminderType: reminderAgentManager.ReminderType.REMINDER_TYPE_TIMER,
      triggerTimeInSeconds: 120,
      title: 'SPIKE 测试提醒',
      content: '看到这条说明代理提醒可用',
      expiredContent: '该提醒已过期',
      notificationId: 1001,
      actionButton: [
        {
          title: '关闭',
          type: reminderAgentManager.ActionButtonType.ACTION_BUTTON_TYPE_CLOSE
        },
        {
          title: '稍后',
          type: reminderAgentManager.ActionButtonType.ACTION_BUTTON_TYPE_SNOOZE
        }
      ],
      wantAgent: {
        pkgName: 'com.medreminder.spike',
        abilityName: 'EntryAbility'
      }
    };
    rid = await reminderAgentManager.publishReminder(req);
    slog('1) publishReminder OK  id=' + rid);
  } catch (e) {
    const be = e as BusinessError;
    slog('1) publishReminder FAIL  code=' + be.code + '  msg=' + be.message);
  }

  // 2) 旧查询 API（Capacitor 插件的 getPending 就走这条）—— 预期被拒
  try {
    const list = await reminderAgentManager.getValidReminders();
    slog('2) getValidReminders OK  count=' + list.length);
  } catch (e) {
    const be = e as BusinessError;
    slog('2) getValidReminders FAIL  code=' + be.code + '  msg=' + be.message);
  }

  // 3) 新查询 API（@since 12，标注只需 PUBLISH_AGENT_REMINDER）—— 关键假设
  try {
    const list = await reminderAgentManager.getAllValidReminders();
    slog('3) getAllValidReminders OK  count=' + list.length);
  } catch (e) {
    const be = e as BusinessError;
    slog('3) getAllValidReminders FAIL  code=' + be.code + '  msg=' + be.message);
  }

  // 4) 订阅提醒状态变化（@since 23）
  try {
    await reminderAgentManager.subscribeReminderState((states) => {
      slog('4) subscribeReminderState 回调 len=' + (states ? states.length : -1));
    });
    slog('4) subscribeReminderState OK');
  } catch (e) {
    const be = e as BusinessError;
    slog('4) subscribeReminderState FAIL  code=' + be.code + '  msg=' + be.message);
  }

  // 5) 排除日期（对应我们的「跳过某天」）
  if (rid >= 0) {
    try {
      await reminderAgentManager.addExcludeDate(rid, new Date());
      const ds: Array<Date> = await reminderAgentManager.getExcludeDates(rid);
      slog('5) addExcludeDate/getExcludeDates OK  count=' + ds.length);
    } catch (e) {
      const be = e as BusinessError;
      slog('5) excludeDates FAIL  code=' + be.code + '  msg=' + be.message);
    }
  } else {
    slog('5) excludeDates SKIP（没有 rid）');
  }

  // 6) API 26 新增参数 repeatInterval + repeatCount
  try {
    const req2: reminderAgentManager.ReminderRequestTimer = {
      reminderType: reminderAgentManager.ReminderType.REMINDER_TYPE_TIMER,
      triggerTimeInSeconds: 30,
      repeatInterval: 60,
      repeatCount: 3,
      title: 'SPIKE 周期提醒',
      content: 'repeatInterval/repeatCount 测试'
    };
    const id2: number = await reminderAgentManager.publishReminder(req2);
    slog('6) publishReminder(repeat) OK  id=' + id2);
  } catch (e) {
    const be = e as BusinessError;
    slog('6) publishReminder(repeat) FAIL  code=' + be.code + '  msg=' + be.message);
  }

  // 7) 取消（验证管理链路）
  if (rid >= 0) {
    try {
      await reminderAgentManager.cancelReminder(rid);
      slog('7) cancelReminder OK');
    } catch (e) {
      const be = e as BusinessError;
      slog('7) cancelReminder FAIL  code=' + be.code + '  msg=' + be.message);
    }
  }

  slog('========== END reminder spike ==========');
}

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    webview.WebviewController.initializeWebEngine();//webview引擎初始化
    try {
      this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
    } catch (err) {
      hilog.error(DOMAIN, 'testTag', 'Failed to set colorMode. Cause: %{public}s', JSON.stringify(err));
    }
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onCreate');
  }

  onDestroy(): void {
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    // Main window is created, set main page for this ability
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onWindowStageCreate');

    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, 'testTag', 'Succeeded in loading the content.');
      // S-0 追加：等 Web 侧跑完首屏测试（约 5s 后）再跑代理提醒测试，避免日志混杂
      setTimeout(() => {
        runReminderSpike();
      }, 8000);
    });
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onBackground');
  }
}