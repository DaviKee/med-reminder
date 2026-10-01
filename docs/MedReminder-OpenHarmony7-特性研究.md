# OpenHarmony 7 / HarmonyOS 7 (API 26) 特性研究 —— 面向 MedReminder 移植

> 研究日期：**2026-09-29**
> 研究对象：**OpenHarmony 7.0 Release = API 26.0.0**（开源侧）／**HarmonyOS 7.0**（商业侧）
> 目的：回答"**鸿蒙 7 能给我们什么**"，为下一阶段（S-0 之后的正式移植）定方向。
>
> **证据分级**（本文每条结论都标注来源等级，请勿混用）：
> - 🟢 **本地 SDK 实测** —— 直接读 `D:\Program Files\Huawei\DevEco Studio\sdk\...` 的 `.d.ts`，最权威
> - 🔵 **华为官方文档** —— developer.huawei.com 官方页面
> - 🟡 **二手资料** —— 社区文章/媒体，**可能有误或滞后，采纳需谨慎**

---

## 0. 一句话结论

> **鸿蒙 7 给本项目最大的礼物不是"新功能"，而是「系统级代理提醒」——它能把我们整套"排程 + 提醒"从应用进程里搬进系统服务，比我们现在依赖的 `@capacitor/local-notifications` 可靠得多。**
> 同时有两个**重要的「否」**：**实况窗我们根本申请不了**（场景不适配 + 月活门槛），**热更新是红线**（Capacitor 要特别小心）。
>
> ⚠️ **但代理提醒不是白给的**（**09-29 晚已实测**，见 §2.5）：
> - `publishReminder` **必须先去 AGC 申请「代理提醒」开放能力 + 手动签名**，否则报 `1700002`；
>   ✅ 好消息：**准入类型包含「医疗类 / 运动健康类 / 生活服务类」，我们在范围内**，审批约 **8 个工作日**。
> - ✅ 附带好消息：`getPending` 的"Permission denied"**只是没声明权限**，声明后三个查询 API 全部可用
>   → **通知清场机制不用改设计**（我之前的判断是错的）。

---

## 1. 版本坐标（先对齐事实）

| 项 | 值 | 来源 |
|---|---|---|
| 版本名 | **OpenHarmony 7.0 Release**，2026-08 正式发布 | 🟡 百度百科 |
| API version | **26.0.0** | 🟢 本地 SDK `sdk-pkg.json` |
| 版本号格式 | 改为语义化 `X.Y.Z`（取代 `X.Y.Z (N)`） | 🟡 |
| 与前代关系 | 在 `6.1.1 (24)` 基础上增强 | 🟡 |
| **关键变化** | **手机级标准设备支持纯 ArkTS 应用运行，移除了 Android APK 兼容层** | 🟡 百度百科 |
| 安全 | 获 CC EAL5+ 与等保三级认证 | 🟡 |
| 内核/运行时 | 重构，"方舟运行时"GC 策略更新，内存占用降低，**后台保活能力增强** | 🟡 |
| **ArkWeb 内核** | **Chromium 132 → 144** | 🟡 |
| 我们的模拟器 | **HarmonyOS 7.0.0 / API 26**（实测 `ArkWeb/7.0.0.105`） | 🟢 spike 实测 |

**增强领域**（相对 6.1.1）：应用程序框架、界面交互、窗口管理、网页引擎、音视频处理。

### ⚠️ 三条对我们有直接后果的事实

1. **APK 装不了** —— 纯血鸿蒙移除了兼容层 → **必须做 HAP**，没有退路。
2. **ArkWeb 是 Chromium 144** —— 我们的 `www/` 跑在一个**比 Chrome 主流版本还新**的内核上。
   好消息：现代 CSS/JS 全支持；**要留意的**：`-webkit-` 前缀、老旧 API 的移除。
3. **适配层只在 API 17 测过，我们要跑在 API 26** —— 这是 S-0 已知的落差，**在 API 26 上向上兼容性没被官方验证过**。

---

## 2. ★★★ 代理提醒（reminderAgentManager）—— 本次研究最有价值的发现

**来源：🟢 本地 SDK `sdk/default/openharmony/ets/api/@ohos.reminderAgentManager.d.ts`（1033 行，逐行读过）+ 🔵 官方错误码文档**

### 2.1 它是什么

鸿蒙的**系统级定时提醒服务**。应用把提醒"托付"给系统服务后，**自己可以被杀、可以退出**，到点系统照样弹提醒。

> 🟡 社区对比表（形象但准确）：
> | 维度 | 传统 `setTimeout` | 代理提醒 |
> |---|---|---|
> | 后台存活依赖 | 必须保活，极易被系统查杀 | **完全脱钩，系统服务托管** |
> | 资源占用 | 持续占 CPU/内存 | **仅触发时唤醒** |
> | 触发可靠性 | 低 | **极高（系统底层时钟）** |

**这正好命中我们的核心痛点。** 我们的排程现在是 `localStorage` + 应用内定时器 + `@capacitor/local-notifications` 排程；而应用被杀后（我们在 Android 上已经踩过这个坑 —— v1.4.10 拍照 bug 就是相机 Activity 期间 App 被杀）排程的可靠性一直是我们最脆弱的一环。

### 2.2 能力清单（🟢 SDK 实测，逐项）

**三种提醒类型**（`ReminderType`）：
- `REMINDER_TYPE_TIMER = 0`（倒计时）
- `REMINDER_TYPE_CALENDAR`（日历：指定日期时间，支持 `repeatMonths` / `repeatDays` / `daysOfWeek` / `endDateTime`）
- `REMINDER_TYPE_ALARM`（闹钟：`hour` / `minute` / `daysOfWeek`）

**`ReminderRequest` 可用字段**（远超我们的需求）：

| 字段 | 作用 | 对我们的价值 |
|---|---|---|
| `actionButton` | 提醒上的按钮，**第三方应用最多 2 个** | ⚠️ 见下方 2.3 的重要限制 |
| `snoozeTimes` | 稍后提醒次数（默认 0） | ✅ **原生"稍后提醒"**，不用自己实现 |
| `timeInterval` | 稍后间隔，**最小 30 秒** | ✅ 对应我们的 snooze |
| `wantAgent` / `maxScreenWantAgent` | 点击跳转目标 Ability | ✅ 点击通知打开 App 打卡 |
| `ringDuration` / `ringChannel` / `customRingUri` | 铃声时长/通道/自定义铃声 | ✅ 适老化：可设长响铃 |
| `slotType` / `snoozeSlotType` | 通知渠道 | ✅ 对应我们已做的渠道管理 |
| `groupId` / `notificationId` | 分组/ID | ✅ 替代我们的自建台账 |
| `expiredContent` / `snoozeContent` | 过期/稍后时显示的内容 | ✅ |
| `title` / `content` / `titleResourceId` / `contentResourceId` | 文案（含资源 ID 版本） | ✅ 多语言友好 |
| `tapDismissed` / `autoDeletedTime` / `fixedTimeZone` | 点击消失/自动删除/固定时区 | ✅ 时区是跨天排程的坑 |
| `notificationRequestProxy` | 通知请求代理 | ℹ️ 未细究 |

**管理 API**（🟢）：

| API | 说明 | 关键点 |
|---|---|---|
| `publishReminder()` | 发布提醒 | 返回系统 `reminderId`（**不是自定义 ID**） |
| `cancelReminder(id)` | 取消指定 | 用系统返回的 id |
| **`cancelReminderOnDisplay(id)`** | **取消正在显示的提醒** | 我们**没有**这个能力 |
| `cancelAllReminders()` | 全部取消 | 对应我们的清场 |
| **`updateReminder(id, req)`** | **原地更新** | 不用"取消+重建" |
| `addNotificationSlot` / `removeNotificationSlot` | 通知渠道管理 | |
| **`addExcludeDate` / `deleteExcludeDates` / `getExcludeDates`** | **排除日期** | ✅ **"跳过某天"原生支持！** |
| `getValidReminders()` | 查有效提醒 | ⚠️ **实测被拒**（见 §2.4） |
| **`getAllValidReminders()`** | 查全部有效提醒（返回 `ReminderInfo[]`） | ⭐ **@since 12，需 `PUBLISH_AGENT_REMINDER` —— 可能是解药** |
| **`subscribeReminderState()` / `unsubscribeReminderState()`** | **订阅提醒状态变化** | ⭐ **@since 23**（API 23 新增，我们 API 26 可用） |

**★ API 26 新增：`repeatInterval` + `repeatCount`**（`ReminderRequestTimer` 专用）

> 🟢 SDK 原文：`must be used together with repeatCount` / `together with repeatInterval`
> 🔵 官方博客原话：*"后台代理提醒对于倒计时场景原本只提供倒计时的秒数……如今新增 repeatInterval 和 repeatCount 两个参数，不但支持设定倒计时的次数，还支持设置会在多长的周期内重复倒计时"*

🟡 社区文章举的应用场景**原文就是**：「**用药应用按周期提醒用户，并在疗程结束后自动停止**」—— 和我们的场景一模一样。

### 2.3 ⚠️ 必须知道的限制

**① 按钮只有两种**（🟢 SDK `ActionButtonType` 枚举全文）：
```
ACTION_BUTTON_TYPE_CLOSE  = 0   （关闭）
ACTION_BUTTON_TYPE_SNOOZE = 1   （稍后提醒）
```
**没有"自定义"类型。** 第三方应用最多 2 个按钮。

> **对我们的直接后果**：Android 上我们现在可以"锁屏直接点『已服用』打卡"。**鸿蒙原生提醒做不到** —— 只能给「关闭 / 稍后」。
> 要"打卡"就得：**点通知 → 打开 App → 在 App 内打卡**（多一步）。
> 我们已识别的"不依赖 action listener"改造，与这里的结论**方向一致**（但那是 web 层的降级方案，这里是原生限制）。

**② 数量上限**（🔵 官方错误码文档）：

| API 版本 | 单个普通应用上限 |
|---|---|
| **API 26.0.0 及以上** | **64 个提醒** |
| API 25 及以下 | 30 个提醒 |
| 全部应用总和 | ≤ 12000（API 10+） |

**⚠️⚠️ 最要命的一条**（🟡 同一份官方文档的"处理步骤"段）：
> *"因管控限制，**普通应用如果没有代理提醒的使用权限，视为这个普通应用提醒数量上限为 0**。"*
> *"首先，确认是否申请了代理提醒的使用权限。"*

→ 说明 `PUBLISH_AGENT_REMINDER` 是**需要单独申请的受管控权限**。
🟡 社区帖也说要"通过邮件形式申请代理提醒使用权限"。

**但**（重要）：我查了官方《应用受限开放权限参考》全文 90+ 项清单（🔵），**`PUBLISH_AGENT_REMINDER` 不在其中** —— 说明它走的是**另一条申请通道**（可能是开放权限 + 管控名单）。**申请路径与难度是本项目移植的最大未知数，必须优先验证。**

**③ 跳转限制**（🟡）：点击提醒通知后跳转的应用**必须是申请代理提醒的本应用**。

**④ 周期性提醒的过期语义不同**（🟡）：
- 单次提醒：**点了"关闭"按钮就算过期**
- **周期性提醒（如每天）：无论是否点关闭，都保持有效**

> 这对我们的"清场"逻辑有影响 —— 周期性提醒不会被"点掉"。

**⑤ 模拟器支持**：🟡 官方指南称"**从 API version 20 开始，本能力支持模拟器开发**"（我们是 API 26 ✅）。
但 ⚠️ **模拟器与真机的管控不同**：🟡 社区帖明确记录"**模拟器用的 6.0 测试没问题，真机 5.0.5 报 1700002**"。
→ **我们在 spike 里 `schedule()` 成功，不能推断真机也成功。**

**⑥ 错误码**（🔵 官方）：

| 码 | 含义 | 常见原因 |
|---|---|---|
| `1700001` | 通知使能未开启 | 没申请通知权限 / 用户关了通知开关 |
| `1700002` | **提醒数量超限** | 数量超限，**或没有权限（上限视为 0）** |
| `1700003` | 提醒不存在 | 已过期/已删除 |
| `1700004` | 包名不存在 | |
| `1700007` | 参数错误 | |

### 2.4 ★ 这可能就是 `getPending` 问题的解药（**09-29 晚已实测验证 → 见 §2.5 修正一**）

回顾 S-0 实测（模拟器）：`@capacitor/local-notifications` 的 **`getPending()` 报 `Permission denied.`**，根因是插件内部调 `reminderAgentManager.getValidReminders()`，需要系统级权限 `NOTIFICATION_AGENT_CONTROLLER`。

**但我在 SDK 里发现了另外两条路**（🟢 逐行读过）：

| API | 权限声明 | 差异 |
|---|---|---|
| `getValidReminders()` | **无 `@permission` 注解** | 但实测 `Permission denied` → 实际需要系统权限 |
| **`getAllValidReminders()`** | **`@permission ohos.permission.PUBLISH_AGENT_REMINDER`** | ⭐ **@since 12**，返回更丰富的 `ReminderInfo[]` |
| **`subscribeReminderState()`** | **`@permission ohos.permission.PUBLISH_AGENT_REMINDER`** | ⭐ **@since 23**，订阅状态变化 |

**关键推论**：`PUBLISH_AGENT_REMINDER` 是**我们自己就能声明使用的权限**（发布提醒本来就靠它），而 `getAllValidReminders` / `subscribeReminderState` 只需要这个权限 —— **不像 `getValidReminders` 那样要系统权限**。

→ **假设（待实测）**：在 `module.json5` 声明 `PUBLISH_AGENT_REMINDER` 后，
**`getAllValidReminders()` 可用** → 我们的"通知清场"机制就**不用降级**。

⚠️ 但要注意 §2.3 那条矛盾：如果普通应用**没有**代理提醒使用权限、上限为 0，那就**根本发不了提醒**，"能不能查"就没意义了。**这两个问题必须一起验。**

---

### 2.5 ★★★ 实测结果（2026-09-29 晚 · 模拟器）—— 含**两条错误结论的修正**

**做法**：在 spike 工程的 `module.json5` 声明 `PUBLISH_AGENT_REMINDER`，
并在 `EntryAbility.ets` 里直接调 `reminderAgentManager` 的 7 个 API，结果打到 hilog。
（原生侧实测，绕过 Capacitor 插件，能看清 API 本身的行为）

#### 结果表

| # | 测试项 | 结果 |
|---|---|---|
| 0 | `isNotificationEnabled()` | ✅ `true` |
| 1 | **`publishReminder()`（TIMER）** | ❌ **`1700002` The number of reminders exceeds the limit** |
| 2 | **`getValidReminders()`** | ✅ **成功，count = 0** |
| 3 | `getAllValidReminders()` | ✅ 成功，count = 0 |
| 4 | `subscribeReminderState()` | ✅ 成功（**回调真的被触发**，len=0） |
| 5 | `addExcludeDate` / `getExcludeDates` | ⏭ SKIP（没拿到 rid） |
| 6 | `publishReminder` + `repeatInterval`/`repeatCount` | ❌ 401 参数错误（次要，见下） |
| 7 | `cancelReminder` | ⏭ SKIP（同上） |

**权限状态核查**（`bm dump -n <bundle>`）：
```
"reqPermissionStates": [ 0 ]                    ← 0 = 已授予
"reqPermissions": ["ohos.permission.PUBLISH_AGENT_REMINDER"]
```

#### ★★ 修正一：`getPending` 的根因**不是**系统权限（我之前判断错了）

| | 之前记的（**错**） | **实测真相** |
|---|---|---|
| 根因 | 需要系统级权限 `ohos.permission.NOTIFICATION_AGENT_CONTROLLER` | **只需在 `module.json5` 声明 `PUBLISH_AGENT_REMINDER`** |
| 依据 | 在 `permissions.d.ts` 里搜到有这个名字就推断的 —— **是推测，不是证据** | A/B 对照：无声明 → `Permission denied`；有声明 → **成功** |
| 对项目的影响 | 以为"通知清场机制要重新设计" | ✅ **不用改设计**，只是移植时必须声明权限 |

> **web 侧同步验证**：同样的 spike 测试页，
> - 声明权限**前**：`✗ 通知排程链路失败 / 错误: Permission denied`
> - 声明权限**后**：`✓ 19 ｜ ✗ 0 ｜ ⚠ 1` —— **全部通过**
> ⚠️ 教训：这是**今天第三次**"从线索推断根因、没做实验验证"（前两次：内存截断、Hyper-V 旧日志）。
> **有线索 ≠ 有证据。**

#### ★★★ 修正二：真正的阻塞是 **`publishReminder` 报 1700002** —— 需要 AGC 申请

**官方原文**（《代理提醒(ArkTS)》开发指南，🔵）：

> **代理提醒开放能力申请**
> 1. 登录 AppGallery Connect → 选择"开发与服务"
> 2. 项目下的应用列表中选择需要申请代理提醒的应用
> 3. 进入"**项目设置**" → "**开放能力管理**"，点击"**代理提醒**"卡片对应的"**申请**"按钮
> 4. 填写申请原因，**上传「代理提醒功能场景截图」和「应用分类信息截图」**，提交
> 5. 按钮变"申请中"，**8 个工作日反馈**
> 6. 通过后勾选能力开关 → 保存
> 7. **"此时，调试和发布应用必须重新生成 Profile 文件并使用手动签名"**
> 8. 申请 `ohos.permission.PUBLISH_AGENT_REMINDER` 权限
> 9. 请求通知授权

🟡 社区补充（与我们的错误码**完全对上**）：
> *"申请通过后，需要手动签名，重新生成 Profile 文件，并对应用进行手动签名，
> 否则 IDE 仍没有权限调用此权限，导致调试和发布失败，**显示比如错误代码 1700002** 等错误信息。"*

**✅ 一条好消息：准入类型包含我们**

🔵 官方：代理提醒**仅对以下类型应用开放申请**：
工具类 · 商务类 · 效率类 · 金融理财类 · 教育类 · **生活服务类** · 旅游类 · **医疗类** · **运动健康类** · 游戏类。
❌ 不支持：营销类场景（电商/红包/优惠券抢购、直播预约等）。

→ **我们的服药提醒属于「医疗类 / 运动健康类 / 生活服务类」，完全在开放范围内**，
且是**正当的健康场景**（非营销），场景截图材料好准备。

#### ⚠️ 修正三：模拟器**也会**检查权限管控

上一轮记的"模拟器不检查权限管控（社区说模拟器过、真机不过）" —— **不适用于代理提醒**：
我们**在模拟器上**就拿到了 `1700002`。
→ **"模拟器能跑 ≠ 真机能跑"仍然成立，但方向反了**：这次是**模拟器就已经拦住**了。

#### ⚠️ 附带发现：Capacitor 插件的 `schedule()` 有**静默失败 bug**

读 `LocalNotifications.ets` 源码（🟢）：

```js
// line 66 schedule()：拿不到 reminderId 就不 push，但外层仍报 success
let reminderId = await publishReminderNotification(...)
if (reminderId !== null && reminderId !== undefined) { resultArray.push({ id: reminderId }) }
...
capacitor.onArkTsResult(JSON.stringify({ result: "success", content: { notifications: resultArray } }), ...)

// line 495 publishReminderNotification()：catch 里既不 return 也不 throw
return await reminderAgentManager.publishReminder(calendar).catch((error) => {
  capacitor.onArkTsResult(JSON.stringify({ result: "failed", errorMsg: error.message }), ...)
  // ← 没有 return / throw → 返回 undefined
})
```

**后果**：`publishReminder` 失败 → 外层**仍然报 `result: "success"`**（内容是空数组）。

**这正是我们测试页显示"`schedule()` 登记成功`"却查不到任何提醒的原因** —— **排程从来没成功过**。

> ⚠️ **这对项目是个警告**：我们 v1.2.1 的排程/清场机制在鸿蒙上会**静默失效**。
> 移植时**必须**：
> ① 检查 `schedule()` 返回的 `notifications` 数组**是否为空**（不能只看有没有抛错）
> ② 用 `getPending()` 做**发放后校验**（现在它可用了）

---

## 3. 有潜力、但要跨门槛的能力

### 3.1 语音播报 TTS（CoreSpeechKit）—— 门槛低，价值高

**来源：🟢 本地 SDK `sdk/default/hms/ets/kits/@kit.CoreSpeechKit.d.ts`**

```ts
// 🟢 SDK 原文
import speechRecognizer from '@hms.ai.speechRecognizer';
import textToSpeech from '@hms.ai.textToSpeech';
export { speechRecognizer, textToSpeech };
```

**关键事实**：
- ⚠️ **它在 HMS 侧**（`sdk/default/hms/`），不在 openharmony 侧 —— 说明是华为移动服务能力（华为设备上默认可用）
- 🟡 支持**中文/英文**，**离线合成**（`online: 1`）
- 🟡 音色：聆小珊（女声）、劳拉（英语）、凌飞哲（男声）
- 🟡 文本 ≤ 10000 字符
- 🟡 支持播报策略标记：`[h1]` 逐字母、`[n2]` 数值播报、`[p500]` 静音 500ms、`[=zhuo2]` 指定多音字
- 🟡 参数：`speed` / `volume` / `pitch` / `playType`

**对我们的意义**（对应 **S-10**）：
- 🟡 竞品「爸妈的药盒」有 TTS 念药名，**我们完全没有**
- 对适老化是**刚需**：老人看不清屏幕也能知道吃哪种药
- **门槛看起来低**（HMS 系统能力，不需单独申请）

⚠️ **但有一个结构性问题**：TTS 是**原生 ArkTS API**，我们的 web 层调不到 —— 需要**写 Capacitor 插件桥接**（或找到现成的鸿蒙插件）。这是**平台差异**，会打破"web 层不写平台判断"的纪律，需要设计成插件层。

### 3.2 服务卡片 / Widget（FormKit）—— 需要写原生 ArkTS

**来源：🟢 本地 SDK `sdk/default/openharmony/ets/kits/@kit.FormKit.d.ts` + 🟡 官方文档**

🟢 SDK 导出：
```ts
FormExtensionAbility, formBindingData, formError, formInfo, formProvider,
FormEditExtensionAbility, LiveFormExtensionAbility, LiveFormInfo
```

**关键事实**（🟡 官方文档）：
- **三种卡片**：静态卡片（`isDynamic: false`）/ **动态卡片**（可交互、实时数据）/ **互动卡片**（API 20+，`LiveFormExtensionAbility`）/ 场景动效卡片
- **上限 16 个卡片**（超过被忽略）
- **五元组是唯一标识**，变更会导致**卡片被删除**
- **刷新机制**：
  - 定时刷新：`updateDuration`（单位是 30 分钟！设 2 = 1 小时）
  - 定点刷新：`scheduledUpdateTime`（如 "10:30"）
  - 条件刷新 / 主动刷新（`updateForm`）
  - **最短刷新间隔 5 分钟**（`setFormNextRefreshTime`）
  - ⚠️ 定时刷新优先级**高于**定点刷新
- ⚠️ **卡片进程只有 5 秒活动时间** —— 不能在里面做重活
- 卡片尺寸：1×2 到 4×4

**对我们的意义**（对应 **S-9**）：
- 竞品「药管家」「爸妈的药盒」和**系统自带**都有卡片 → 我们缺
- 适老化价值高：**不打开 App 就能看到"下次吃药时间"**
- ⚠️ **但卡片必须用 ArkTS 写**（是独立的 `FormExtensionAbility`）—— **我们的 `www/` 网页不能直接变卡片**
- 需要**额外写原生 ArkTS 卡片** + 解决"卡片与 web 层数据同步"（卡片读的是本地存储，而我们的数据在 WebView 的 `localStorage` 里 ⚠️ **这是个真问题**）

> ⚠️ **一个必须先想清楚的架构问题**：我们在 Android 上的数据在 `localStorage`（WebView 沙箱内），鸿蒙卡片是原生 ArkTS 进程 —— **两者怎么共享数据？**
> `localStorage` 原生侧读不到。可能方案：① 用 `@capacitor/preferences` 换成原生存储 ② 写插件让 web 每次变更时同步给原生 ③ 卡片只做"只读展示"，数据由 web 推送。
> **这是个不小的改造，建议在 S-9 立项时先做方案设计。**

### 3.3 手表端（S-11）—— 可能**不用开发**

🟡 官方《智能穿戴应用开发》要点：
- 手表应用需 `"deviceTypes": ["wearable"]`
- **官方推荐"独立 HAP 精准适配"**（手表与手机页面差异显著）
- 儿童智能表需 **API 23+**
- 穿戴支持 ArkTS 与 JS；**轻量级穿戴仅支持 JS**
- 硬件约束极强：**1.2–1.78 英寸、内存常 < 512MB、后台任务最长 10 分钟**
- 发布时**仅能选"手表"设备类目**，无法区分智能穿戴/轻量级

**⭐ 但有个更省事的可能性**：
🟡 华为手机 + 华为手表的**通知同步是系统能力**（默认开启）。
→ **如果我们用代理提醒发系统通知，手表可能自动同步振动** —— **不需要写手表应用！**
→ ⚠️ 这条**必须真机实测**（我们的模拟器只手机会话，无法验手表）。

> **建议：S-11 不要先立项做手表 App。先验"系统通知能否自动同步到手表"。能同步 → S-11 直接消除；不能 → 再评估独立 HAP 的成本。**

---

## 4. 🚫 明确不可用 / 是红线（省得走弯路）

### 4.1 实况窗（Live View Kit）—— **我们用不了**，建议直接从排期划掉

**来源：🔵 华为官方《Live View Kit 简介》全文（逐条读过）**

看起来很美（锁屏/通知中心/状态栏常驻、胶囊态+卡片态），**但对我们全是死路**：

| 障碍 | 官方原文 |
|---|---|
| **场景不适配** | 实况窗要求"**时段性**"，且官方**明确排除**："天气提示、**待办**等**单点提醒**，则**不属于**实况窗" |
| **场景清单里没有「服药提醒」** | 官方开放 16 类场景：打车 / 配送 / 航班 / 火车 / 排队 / 取餐 / 赛事 / 租赁 / 计时 / 订阅计时 / 运动 / 导航 / 打卡 / 快递 / 进度 / 交易 —— **一个都不匹配** |
| 最接近的 TIMER 也不行 | "**仅限于工具类应用申请**"，且明确不适用："取餐计时提醒、**课程提醒、事项待办、会议日程提醒**" |
| **申请门槛我们达不到** | 🔵 官方申请页原文：*"若开发者的**应用月活数大于等于 1000 且为已上架应用**"* → **新应用不可能满足** |
| 生命周期短 | 单个实况窗**最长 8 小时**，超 4 小时未更新自动销毁 |
| 额外依赖 | 需要 **Push Kit 权益**（远程更新）+ 实况窗权益，审批约 7 个工作日 |
| 不支持模拟器 | 有资料明确说不支持（官方文档两处说法不完全一致） |

**→ 结论：不要考虑实况窗。** 想"常驻显示下次吃药时间"，**唯一的正道是服务卡片（§3.2）**。

### 4.2 热更新 —— **红线，绝对不要碰**

🟡 华为应用市场审核驳回码 **908166「存在热更新逻辑」**，处理方式是：
> *"删除 `eval()`、`new Function()`、反射加载 dex 逻辑"*
🔵 官方论坛答复原文：
> *"鸿蒙**禁止动态加载和执行远程 ArkTS/JS 代码**（包括 eval、new Function 等）。但允许以下有限更新：资源热更……HSP……**Web 容器：将部分页面用 H5 实现，通过 CDN 更新**。"*

**⚠️ 对 Capacitor 的特别提醒**：
- ✅ 我们的 `www/` 是**本地打包**进 HAP 的（不是远程下载），**这是合规的** —— 官方甚至明确允许"Web 容器 + CDN 更新"这种形态
- ⚠️ **但绝不能在运行期从网络下载并执行 JS**。我们**没有**这么做，✅ 安全。
- ⚠️⚠️ **`hionic` 装插件时自动引入了 `HotCodePushPlugin`**（S-0 观察到的）。**移植时必须确认它没被启用/没被打进 HAP** —— 名字就撞在审核红线上。

### 4.3 其他需要留意的审核红线

**来源：🔵 华为官方《应用审核指南》+ 🟡 社区驳回案例**

| 项 | 说明 | 对我们的影响 |
|---|---|---|
| **TOP 3 驳回：单一 H5/Web 页面** | 官方原文：*"应用不得是简单打包的网站页面或套用模板……"* | ⚠️ **直接命中我们**！Capacitor 本质是 WebView 壳。**必须证明不是"套壳网页"** —— 好在我们有相机打卡、本地通知、拍照留痕等原生能力 |
| **TOP 1 驳回：与系统功能重复** | 官方点名："日历、计时类"等 | ⚠️ 鸿蒙**系统自带"健康三叶草"服药提醒**！我们要突出差异（打卡制滚动排程/拍照留痕/依从率统计） |
| 隐私政策 | 必须是**独立文本**，主界面 **≤4 次点击**可达，首次启动醒目提示 | 我们需要**新增隐私政策页** |
| 权限声明 | `module.json5` 必须写 `reason` + `usedScene` | 移植时必做 |
| 名称长度 | ≤15 汉字 / 30 其他字符；**一年内只能改 2 次** | 上架前定好 |
| 截图 | ≥3 张不同内容，1080P+，**禁止含状态栏信息与竞品 Logo** | |
| 资质 | **APP 核准（备案）** + 软著（名称需一致） | ⚠️ 秦老师需提前准备 |

> 🟢 **一条对我们的合规优势**（🟡 一篇上架实录作者的原话）：
> *"我不做账号系统、不收集任何儿童信息，日程数据仅保存在本地，这个设计也成了上架时的一项**合规优势**。"*
> → **我们的"无账号 / 数据不出手机"定位，在鸿蒙审核里是加分项**，与产品定位完全一致。

---

## 5. 其他值得一提的鸿蒙 7 新能力（与本项目弱相关，备查）

| 能力域 | 新东西 | 来源 |
|---|---|---|
| **NotificationKit** | 通知服务升级：可**更精准地获取用户的通知设置**（含锁屏通知、横幅通知） | 🟡 |
| 应用框架 | 支持**获取应用退出原因**、**控制通知铃声与锁屏通知** | 🟡 |
| **CoreFileKit** | 新增 **`listFileExt` / `listFileExtSync`**：递归列出 + 自定义文件名过滤（新 `FileFilter` 类型） | 🟡 官方博客 |
| BackgroundTasksKit | `repeatInterval` / `repeatCount`（见 §2.2） | 🟢 |
| **压缩解压缩模块** | 新增 | 🟡 |
| ArkUI | 沉浸光感组件、动态布局容器、Tabs 嵌套滚动、`@Consume` 默认值、数字翻牌动效、SymbolGlyph 增强 | 🟡 |
| 窗口管理 | **标准悬浮窗（闪控窗）**、按需销毁页面内容 | 🟡 |
| ArkWeb | Chromium 144、**URL 白名单控制**、隐私网络访问检查、自定义错误页、PDF 增强 | 🟡 |
| 音视频 | HDR、多屏录制、**空间音频**、Cinepak | 🟡 |
| Ability Kit | **`ModularObjectExtensionAbility`**（把功能以模块化对象开放给其它应用） | 🟡 |
| 安全 | 星盾机密风控引擎、**分布式数字身份（DID）**、数字盾 | 🟡 |
| **HealthServiceKit**（HMS） | `healthStore` / `healthService`（华为运动健康数据） | 🟢 |
| **LiveViewKit**（HMS） | 实况窗（§4.1，我们不可用） | 🟢 |
| **DeskTopExtensionKit**（HMS） | `quickBarManager` / `statusBarManager`（PC 侧） | 🟢 |
| **ArkAF / Agent Framework Kit（智能体）** ⭐ | **方舟智能开发框架**：意图框架 / Skill 框架 / **端侧 A2A 框架**（应用内置智能体，被小艺调用）；API 26 新增 `AgentAbilityExtension` 支持 A2A 协议通信（Task/Message/Artifact 全套）。**对本项目价值高，已专项调研** → 详见《MedReminder-Agent接入调研.md》 | 🔵+🟢 |

---

## 6. 对排期的重新评估

| 编号 | 项 | 原判断 | **本次研究后的判断** |
|---|---|---|---|
| — | **代理提醒** | 未立项 | ⭐ **应升格为移植的核心议题**。它比 `@capacitor/local-notifications` 更可靠，且原生支持"稍后提醒""排除日期""更新提醒"。**但权限申请是最大未知数** |
| **S-9** | 桌面卡片 | 竞品都有 → 补 | ✅ **值得做，但要先在 ArkTS 侧开发**。⚠️ 前置问题：**WebView 的 `localStorage` 与原生卡片如何共享数据**（需架构设计） |
| **S-10** | 语音播报 | 竞品有 → 补 | ✅ **优先级可提高**。CoreSpeechKit 是 HMS 系统能力，门槛低、适老化价值高。需写 Capacitor 插件桥接 |
| **S-11** | 手表端 | 竞品有 → 补 | ⚠️ **先别立项**。**先验"系统通知能否自动同步到手表"** —— 能同步则免费获得；不能再评估独立 HAP |
| — | 实况窗 | 曾考虑 | 🚫 **划掉**（§4.1，场景不适配 + 月活 1000 门槛） |
| — | 热更新 | — | 🚫 **红线**。确认 `HotCodePushPlugin` 未被启用 |
| — | **Agent（智能体接入）** | 未立项 | ⭐ **值得做，建议与 S-9 合并为「原生化外放」阶段**（共享数据层迁移前置；一期只读技能、二期语音打卡）。⚠️ 不能放 HAR（须在 entry 模块）、**不支持模拟器（必须真机）**。详见《MedReminder-Agent接入调研.md》§5/§7 |

**建议的下一步优先级**：
```
① 验代理提醒权限（生死线）→ ② 验手表通知同步（可能白捡）→
③ TTS（门槛低、价值高）→ ④ 卡片（需先解决数据共享架构）
```

> 📌 **2026-09-29 补记**：Agent 专项调研完成后，上述队列修订为 ——
> ① 代理提醒 AGC 申请 → ② **数据层迁移预研**（卡片+Agent 共同前置）→ ③ TTS → ④ 手表同步 →
> ⑤ 「原生化外放」= S-9 卡片 + Agent 一期（只读）→ ⑥ Agent 二期（语音打卡）+ 意图注册。
> 依据见《MedReminder-Agent接入调研.md》§7。

---

## 7. 待实测清单（模拟器可做，按优先级）

| # | 要验什么 | 状态 | 结论 |
|---|---|---|---|
| **1** | **`publishReminder` 到底能不能用** | ✅ **已验（09-29 晚）** | ❌ **不行** —— 声明权限 + 权限已授予（`grantStatus=0`）**仍然报 `1700002`**。**必须走 AGC「开放能力管理 → 代理提醒」申请 + 手动签名**（详见 §2.5） |
| **2** | `getPending` / `getAllValidReminders` / `subscribeReminderState` 是否可用 | ✅ **已验** | ✅ **三个全部可用**（`getPending` 的根因只是**没声明权限**，§2.5 修正一） |
| **3** | 代理提醒的"点击跳转 + 按钮"实际行为 | ⏳ 待办 | 需先拿到能力、能发出提醒才能验 |
| **4** | `repeatInterval` + `repeatCount` 在 API 26 上是否真的可用 | ⚠️ 已尝试 | 报 `401 参数错误`（可能因未获授权，也可能参数组合不对；**需拿到授权后复验**） |
| **5** | **相机**插件在模拟器/真机的真实可用性 | ⏳ 待办 | S-0 唯一没测的插件 |
| **6** | 服务卡片能否在 Capacitor 工程里加 + 数据怎么共享 | ⏳ 待办 | S-9 的可行性前置 |
| **7** | CoreSpeechKit 的 TTS 在模拟器上能否出声 | ⏳ 待办 | S-10 的可行性前置 |

> ⚠️ **模拟器 ≠ 真机的三条**（本次研究新增）：
> ① **代理提醒的权限管控在模拟器上不检查**（社区实测：模拟器过、真机报 1700002）
> ② 实况窗模拟器不支持（我们反正用不了）
> ③ 手表相关**完全无法在模拟器验证**
> → **凡是"权限/管控"类的结论，模拟器通过都不算数。**

---

## 8. 来源与可靠性

| 结论类别 | 来源 | 等级 |
|---|---|---|
| 代理提醒全部字段/API/枚举 | `sdk/default/openharmony/ets/api/@ohos.reminderAgentManager.d.ts`（本地，逐行读） | 🟢 |
| TTS / 实况窗 / 健康 能力域 | `sdk/default/hms/ets/kits/*.d.ts`（本地） | 🟢 |
| 卡片能力域 | `sdk/default/openharmony/ets/kits/@kit.FormKit.d.ts`（本地） | 🟢 |
| 提醒数量上限 64/30、错误码含义 | 《reminderAgentManager 错误码》developer.huawei.com/consumer/cn/doc/harmonyos-references/errorcode-reminderagentmanager | 🔵 |
| **★★ 代理提醒的完整申请流程（AGC 开放能力 + 手动签名 + 8 工作日 + 准入类型清单）** | **《代理提醒(ArkTS)》开发指南** developer.huawei.com/consumer/cn/doc/harmonyos-guides/agent-powered-reminder | 🔵 |
| 申请通过后仍报 1700002 的原因（未手动签名/未重生成 Profile） | HarmonyOS 开发者社区《项目申请开放能力后，如何调用该权限，以"代理提醒"为例》 | 🟡 |
| **§2.5 全部实测结果**（原生侧 7 个 API + 权限授予状态 + web 侧 A/B 对照） | **本机 spike 工程实测**（`spike/harmony-test/`，模拟器 Mate 90 Pro / API 26） | 🟢 |
| 受限权限清单（90+ 项） | 《应用受限开放权限参考》 | 🔵 |
| 实况窗场景准入与申请门槛 | 《Live View Kit 简介》+《申请实况窗正式权限》 | 🔵 |
| 上架审核规则、驳回码 908166 | 《应用审核指南》+ 官方论坛答复 | 🔵 |
| 手表开发要点 | 《智能穿戴应用开发》 | 🔵 |
| 版本号/发布日期/移除 APK 兼容层 | 百度百科「OpenHarmony 7.0」 | 🟡 |
| `repeatInterval`/`repeatCount` 的引入动机 | 华为开发者联盟博客《一文读懂 HarmonyOS 7.0 带来的十大 API 重要升级》 | 🔵/🟡 |
| 提醒数量上限的实战表现、部分踩坑 | CSDN / 华为云社区帖子 | 🟡 |

> ⚠️ **本文的诚实边界**：
> - **除 §1 表格里标 🟢 的行外，本文的鸿蒙侧结论均未经本机实测** —— 全部来自本地 SDK 声明文件与官方/社区文档。
> - **`PUBLISH_AGENT_REMINDER` 的申请路径与难度，本文没有查清**（它不在受限权限清单里，但官方错误码文档说要"申请"）—— **这是下一步必须解决的第一件事**。
> - 二手来源（🟡）的版本号、时效性可能有滞后，**以本地 SDK 与官方文档为准**。

---

## 附录：本次研究的"三句话"给下一阶段

1. **代理提醒是鸿蒙给我们的真礼物** —— 系统级托管、原生支持稍后提醒/排除日期/更新提醒，比 `local-notifications` 插件可靠得多。**但它的权限管控是生死线，必须先验。**
2. **两个"否"省了大量时间** —— **实况窗用不了**（场景不适配 + 月活门槛）、**热更新是红线**（且要确认 hionic 带的 `HotCodePushPlugin` 没被打进去）。
3. **TTS 门槛最低、卡片价值最高但要先解数据共享** —— 而**手表可能根本不用开发**（先验系统通知自动同步）。

---

## 9. ★ S-9 卡片数据共享架构调研（2026-09-30，文档实证）

> 回答的问题：**ArkTS 卡片读不到 WebView 里的 localStorage，"下次吃药时间"等摘要数据怎么过去？**
> 结论：可行，且有官方标准通道。MVP 不依赖任何受控权限。

### 9.1 已实证的机制（全部来自本地文档 `devecocli docs search`，回显一致性已核对）

| 机制 | 要点 | 来源 |
|---|---|---|
| **FormBindingData** | `formBindingData.createFormBindingData()` 创建卡片数据对象；`onAddForm` **10 秒内必须返回**（FormExtensionAbility 生命周期极短，禁耗时操作） | API 参考 formBindingData / FormExtensionAbility |
| **四种刷新方式** | 定时刷新（`updateDuration`，**最小 30 分钟**）、定点刷新（**最多 24 个时间点**）、下一次刷新、**代理刷新** | FAQ faqs-form-8 / faqs-form-16 |
| **编译期校验** | `form_config.json` 的 `updateEnabled=true` 时 `updateDuration` 与 `scheduleUpdateTime` **不能同时为空** | FAQ faqs-compiling-and-building-40 |
| **卡片→应用** | `postCardAction`（点击卡片跳转/传消息回应用） | ArkTS卡片页面刷新概述 |
| **卡片↔应用共享状态** | 官方给的路子：`@ohos.commonEventManager`（公共事件）+ 共享存储 | FAQ faqs-local-database-management-66 |
| **主动推送** | 卡片提供方可主动 `updateForm` 推 FormBindingData（不局限于被动等系统拉） | ArkTS卡片页面刷新概述 |

### 9.2 关键约束 → 架构决策

| 约束 | 对我们的含义 | 决策 |
|---|---|---|
| 定时刷新 ≥30 分钟、定点 ≤24 个 | "到点卡片变色"做不到精确 | **卡片只负责展示，不承担提醒** —— 到点提醒本来就归通知（等 AGC） |
| onAddForm 10 秒限制 | 卡片侧不能现算排程 | 摘要由 **web 层算好落盘**，卡片侧只读文件 |
| 卡片读不到 WebView localStorage | 必须经文件/事件中转 | 摘要 JSON 落沙箱固定文件（Filesystem 插件），同应用同沙箱可读 |
| formId 动态分配 | 主动推送需要 formId | `onAddForm` 时把 formId 持久化到 preferences |

### 9.3 MVP 架构（数据流）

```
[WebView JS]  cardSummary.js（新 web 模块，纯 JS，双端通用）
  触发时机：启动 / 每次打卡 / 每次 save()
  计算：{ nextDose: {time, medName}, todayDone, todayTotal, date }
    ↓ Filesystem 插件写
  Directory.Data/card/card_state.json        ← 同沙箱，ArkTS 侧可读
    ↓
[ArkTS 卡片侧]
  FormExtensionAbility.onUpdateForm / onAddForm
    读 card_state.json → createFormBindingData → formProvider.updateForm(formId, data)
    （formId 在 onAddForm 时存 preferences）

刷新策略（MVP）：
  ① 应用内打卡/改动后 → 宿主主动 updateForm（卡片立即变，应用必活着——用户在操作）
  ② form_config.json 定点刷新 0:00 → 跨天重置当日进度
  ③ 代理刷新 → 二期再评估（权限要求待核实，MVP 不依赖）
  ④ 卡片点击 → postCardAction router 跳 App

卡片内容 MVP：下次吃药时间 + 药名 + 今日进度 n/m
```

### 9.4 工作量预估

| 部分 | 量 | 说明 |
|---|---|---|
| web 层 `cardSummary.js` | ~100 行 + spec | **双端通用**（Android 将来的 widget 也能用同一份数据源），挂在 `store.save()` 钩子上 |
| ArkTS 卡片（UI + FormExtensionAbility） | ~200 行 ArkTS | **我们的第一份 ArkTS 代码**，新领域，留学习余量 |
| form_config.json + 工程接线 | 小 | 注意 updateEnabled 编译期校验 |

### 9.5 待核实（下次查）

- **代理刷新的权限要求**（是否与推送服务绑定、是否受控）—— MVP 不依赖，二期再说
- 卡片里能否直接读沙箱文件（vs 必须经 FormExtensionAbility）—— 影响不大，按"经 FormExtensionAbility"设计
- `devecocli docs read` 具体文档全文还没读过（本轮只用了 search 摘要），S-9 立项时把 formBindingData / FormExtensionAbility / 卡片页面刷新三篇读全

### 9.6 补充（读全文后新确认的实现细节）

- **卡片与提供方是独立进程**：推送的数据卡片侧**只能经 `LocalStorageProp` 接收**，且会被转成 **string**；
  卡片内不能用 `getContext`。→ card_state.json 的摘要字段保持扁平、全部字符串化，卡片页面直接绑定。
- 已验证 `devecocli docs read "<完整路径>"` 可读全文（本条来自 ArkTS卡片页面刷新概述 原文）。
- ⚠️ **接线坑（编译期校验）**：`module.json5` 里 type=form 的 ExtensionAbility，`metadata` 字段
  **不能留空也不能是空数组**（FAQ faqs-compiling-and-building-164）—— 建卡片工程时第一个会撞的错。
