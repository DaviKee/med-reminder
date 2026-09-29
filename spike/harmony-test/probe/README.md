# 代理提醒（reminderAgentManager）探针

> **目的**：验证 `publishReminder` 到底能不能用 —— 这是鸿蒙移植的**生死线**。
> **实测日期**：2026-09-29 晚 · 模拟器 Mate 90 Pro（HarmonyOS 7.0.0 / API 26）

---

## ⚠️ 为什么要把代码存在这里

`openharmony/` 整个目录是 **hionic 生成 + 被 `.gitignore` 排除**的（见仓库根 `.gitignore`）。
所以往里加的探针代码**不会被版本库跟踪**。这里存一份，**重生成工程后可直接覆盖回去**：

```bash
T=openharmony/entry/src/main
cp probe/EntryAbility.ets "$T/ets/entryability/EntryAbility.ets"
cp probe/module.json5      "$T/module.json5"
cp probe/string.json       "$T/resources/base/element/string.json"
```

（`string.json` 是为 `requestPermissions.reason` 加的字符串资源，少它会编译失败。）

---

## 结论（详见 `MedReminder-OpenHarmony7-特性研究.md` §2.5）

| 测试项 | 结果 |
|---|---|
| `isNotificationEnabled()` | ✅ `true` |
| **`publishReminder()`（TIMER）** | ❌ **`1700002` 提醒数量超限** |
| **`getValidReminders()`** | ✅ 成功（count=0） |
| `getAllValidReminders()` | ✅ 成功（count=0） |
| `subscribeReminderState()` | ✅ 成功（回调触发） |
| `publishReminder` + `repeatInterval`/`repeatCount` | ❌ `401` 参数错误（待获授权后复验） |

**三条要点**：

1. **`publishReminder` 必须先去 AGC 申请「代理提醒」开放能力 + 手动签名**，否则报 `1700002`。
   声明权限 + 权限已授予（`bm dump` 显示 `reqPermissionStates: [0]`）**都没用**。
   ✅ 准入类型含「医疗 / 运动健康 / 生活服务类」，我们在范围内；审批约 8 个工作日。
2. ✅ **`getPending` 的"Permission denied" 根因只是没声明权限** —— 声明后全部可用。
   → **通知清场机制不用改设计。**
3. ⚠️ **模拟器也会检查这个管控**（我们就是模拟器上报的 `1700002`）。

---

## 怎么看结果

探针在 `onWindowStageCreate` 里 **延迟 8 秒**执行（等 web 侧首屏跑完，避免日志混杂），
结果全部打到 hilog，tag = **`SPIKE-REMINDER`**：

```bash
D="/d/Program Files/Huawei/DevEco Studio"
HDC="$D/sdk/default/openharmony/toolchains/hdc.exe"
# 清缓冲 → 重启应用 → 等 → 读
"$HDC" -t 127.0.0.1:5555 shell hilog -r
"$HDC" -t 127.0.0.1:5555 shell aa force-stop com.medreminder.spike
"$HDC" -t 127.0.0.1:5555 shell aa start -b com.medreminder.spike -a EntryAbility
sleep 26
"$HDC" -t 127.0.0.1:5555 shell hilog -x -T SPIKE-REMINDER
```

**查权限授予状态**（验证"权限到底给没给"）：
```bash
"$HDC" -t 127.0.0.1:5555 shell bm dump -n com.medreminder.spike | grep -A2 reqPermissionStates
# 期望看到 [ 0 ]  ← 0 = 已授予；-1 = 未授予
```

---

## 拿到 AGC 授权后要复测什么

1. `publishReminder`（TIMER / CALENDAR / ALARM **三种类型分别测** —— 插件用的是 CALENDAR）
2. `repeatInterval` + `repeatCount`（本次报 401，需排除"未授权导致"）
3. `addExcludeDate` / `getExcludeDates`（本次 SKIP，因为没拿到 rid）
4. 提醒真的弹出来时的**实际样式与按钮**（`actionButton` 只有 CLOSE / SNOOZE）
5. 点击提醒 → 是否真能跳回本应用（`wantAgent`）

> ⚠️ 复测时**注意签名**：官方明确要求"调试和发布应用必须**重新生成 Profile 文件并使用手动签名**"。
> 只申请能力、不换签名，仍会报 `1700002`。
