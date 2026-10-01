# MedReminder 移植鸿蒙系统 · 调研报告

> 调研时间：2026-09-17 ｜ 状态：**只做调研，未动任何代码**
> 结论先行：**有现成的路可走**，而且我们前 7 轮的架构决策在这里兑现了 —— web 层几乎可以原样复用。

---

## 0. ⚠️ 先回答一个前提问题（决定后面所有事）

**你的手机是哪个鸿蒙？** 这两种情况差别是天壤之别：

| 系统 | 能不能装现在的 APK | 要做的事 |
|---|---|---|
| **HarmonyOS 4.x 及以前**（基于 AOSP，兼容安卓应用） | ✅ **直接能装、能用** | **什么都不用移植** —— 现在的 v1.4.0 装上就行 |
| **HarmonyOS 5 / NEXT**（纯血鸿蒙，不再兼容 APK） | ❌ 装不了 | 必须重新打包成 **HAP**，就是这份报告讲的事 |

**怎么查**：手机「设置 → 关于手机（或关于本机）→ 看 HarmonyOS 版本号」。

- 显示 **4.x / 3.x / 2.x** → 现在这个 APK 直接能用，移植可以从容安排
- 显示 **5.x 或 6.x（NEXT）** → 走下面的方案

> 如果方便，把那一行版本号念给我，我就能给出确定的路线。

---

## 1. 结论：Capacitor 有鸿蒙适配，而且**我们用的插件全都有**

这是本次调研最重要的发现。

### 1.1 谁做的

| 项 | 内容 |
|---|---|
| 平台适配包 | `@capacitor-ohos/ohos` **8.0.2** |
| CLI | `hionic` **2.1.16**（"Ionic CLI with OpenHarmony support"，2026-07-23 发布） |
| 维护方 | 包作者字段写的是 **"Huawei Device Co., Ltd and iSoftStone Information Technology(Group)Co.,Ltd."** —— **华为 + 软通动力**，不是野生项目 |
| 仓库 | `gitcode.com/CPF-Ionic/capacitor-*`（CPF-Ionic 社区，归属 OpenHarmony 跨平台框架 PMC） |
| 官方实测环境 | SDK **5.0.5(API 17)** / DevEco Studio **6.0.0** / ROM **5.1.0.150** |

### 1.2 我们的四个插件，鸿蒙侧都有对应包

| 现在用的（Android） | 鸿蒙对应包 | 版本 |
|---|---|---|
| `@capacitor/app` | `@capacitor-ohos/app` | 8.0.2 |
| `@capacitor/camera` | `@capacitor-ohos/camera` | 8.0.2 |
| `@capacitor/filesystem` | `@capacitor-ohos/filesystem` | 8.0.2 |
| `@capacitor/local-notifications` | `@capacitor-ohos/local-notifications` | 8.0.1 |
| （平台层，对应现在的 `android/` 目录） | `@capacitor-ohos/ohos` | 8.0.2 |

**版本号 8.x 与上游 `@capacitor/*` 8.x 对齐** —— 说明适配层是跟着上游走的，不是一次性 fork 完就不管了。

> ⚠️ **2026-09-29 补记（本轮用 npm 官方源核实，本文档原版漏了这条）**：
> `@capacitor-ohos/ohos` 的 `peerDependencies` 是 **`@capacitor/core: ^8.0.0`**。
> **鸿蒙适配层要求 Capacitor 8，而本项目在 Capacitor 6**（实测 `node_modules/@capacitor/core` = **6.2.2**）。
>
> 这意味着 §4 里「web 层 ≈ 0 工作量」的结论**只对 `www/js/` 成立**；
> **工程配置与依赖层要先跨一次 6 → 8 的大版本升级**（`@capacitor/*` 破坏性变更 + `index.html` 手动引入的插件 JS 路径 + `capacitor.config.json` schema）。
> 详见 `MedReminder-后续任务计划.md` §24.2。
>
> 另外，5 个包 + `hionic` 的**存在性与版本号已逐条 npm 核实，全部属实**：
> `ohos`/`app`/`camera`/`filesystem` = 8.0.2、`local-notifications` = 8.0.1、`hionic` = 2.1.16，
> 作者字段确为 `Huawei Device Co., Ltd and iSoftStone Information Technology (Group)`。

### 1.3 这直接说明了什么

我们 7 轮下来最重要的一个架构纪律是：**所有原生能力的调用都收敛到 4 个插件 + `window.Capacitor` 这一个抽象后面**，
web 层（`app.js` / `notify.js` / `photo.js` / `backup.js`）里**没有任何 `if (isAndroid)` 这样的平台判断**。

这条纪律现在兑现了：**`www/` 下的代码基本可以原样搬到鸿蒙**。

---

## 2. 能力对照：哪些能用、哪些必须改

我是把 npm 上的包拉下来、读它的 `README.md` 与 `.ets` 实现得到的结论（不是看二手文章）。

### 2.1 本地通知（我们的核心功能）—— **大部分能用**

**✅ 支持，而且我们都在用**：
`schedule`、`cancel`、`getPending`、`areEnabled`、`checkPermissions`、`requestPermissions`、
`registerActionTypes`、`getDeliveredNotifications`、`removeDeliveredNotifications`、`removeAllDeliveredNotifications`

> ⭐ **一个特别重要的好消息**：**`getPending` 支持**。
> 我们 v1.2.1 修「通知一次冒出一堆」时，整套清场机制（`purge` / `clearDelivered`）就是建立在
> 「问插件要权威清单再清」之上的。这条路在鸿蒙上依然是通的 —— **不用重新设计**。
> 而且鸿蒙包里有 `NotificationStorage.ets`，设计上明显对齐了 Android 版。

**❌ 不支持，且我们有依赖**：

| 不支持的 API | 我们用它做什么 | 影响与对策 |
|---|---|---|
| **`addListener('localNotificationActionPerformed')`** | 锁屏/通知栏上「已服用」「10 分钟后」两个按钮的回调 | **锁屏按钮会点了没反应**。对策：鸿蒙上改为「点通知 → 打开 App → 在 App 内打卡」 |
| `createChannel` / `listChannels` | `probe()` 的第三路信号（判断"用户把通知渠道单独关掉了"） | **已自动降级** —— 我们的 `probe()` 本来就写了 `typeof LN.listChannels !== 'function'` 的分支，不用改 |
| `allowWhileIdle` | 调度参数 | 忽略即可（鸿蒙的定时提醒机制不同） |

其余不支持项（`repeats` / `on` / `every` / `count` / `attachments` / `silent` 等）我们**都没用**。

### 2.2 文件系统 —— **必须改**（我们的自动备份正好踩在差异上）

鸿蒙版的 `Directory` 映射与 Android **完全不同**（源码：`FilesystemAction.ets` 的 `getPathDir()`）：

| Directory | Android 上的含义 | 鸿蒙上的实际落点 |
|---|---|---|
| `EXTERNAL` | app 专属外部目录（电脑 USB 可见） | **没有这个分支** → 落到 `default` → **沙箱 `filesDir`** |
| `DOCUMENTS` | 公共文档目录 | `Environment.getUserDocumentDir()`（**用户可见、卸载不删**）；设备不支持该能力时退到沙箱 |
| `DATA` / `LIBRARY` | 私有目录 | `filesDir`（沙箱） |
| `CACHE` | 缓存 | `cacheDir` |
| `TEMPORARY` | 临时 | `tempDir` |

**影响**：我们的**自动备份**（v1.4.0）用的是 `Directory.External` →
在鸿蒙上会**静默落到应用沙箱**，用户和电脑都看不到 —— 那备份就失去意义了。

**改法（很小）**：`backup.js` 里的
```js
var DIRS = ['EXTERNAL', 'DATA'];      // Android
var DIRS = ['DOCUMENTS', 'DATA'];     // 鸿蒙（降级链逻辑完全不用动）
```

### 2.3 相机 —— 基本一致

- `getPhoto` ✅、`source`（`PROMPT` / `CAMERA` / `PHOTOS`）✅ —— 语义与 Android 一致
- 我们 v1.2.2 修的那个 `'CAMERA'` 全大写写法，在鸿蒙侧同样适用
- 只不支持 `pickLimitedLibraryPhotos` / `getLimitedLibraryPhotos`（**我们没用**）

### 2.4 App 与平台层 —— 甚至比 Android 更丰富

`@capacitor-ohos/app` 提供 App 状态与事件；平台包（`@capacitor-ohos/ohos`）提供 ArkWeb 容器与 JSBridge，
另外还有一批 Android 侧没有的高级能力（README 里列了 13 条），其中几条对我们有直接价值：

- **字体大小缩放百分比，README 明确写着「支持适老化」** → 我们的字号档位（A-1 无障碍批次）可以借力
- 自定义启动路径（rawfile / resfile / 沙箱路径）
- 拦截自定义 scheme
- 动态设置 WebView 属性
- 多 WebView（平板等大屏）

---

## 3. 环境与流程

### 3.1 环境（官方实测组合）

| 组件 | 版本 |
|---|---|
| DevEco Studio | **6.0.0**（华为官方 IDE，相当于 Android Studio） |
| HarmonyOS SDK | **5.0.5（API 17）** 或更高 |
| 手机 ROM | 5.1.0.150（实测通过） |
| Node.js | 16+ |

> ⚠️ 社区反复提到的一个坑：**DevEco 的安装路径和项目路径都不能有中文或空格**。
> 我当初装 Android 工具链时也踩过类似的路径坑，这条我会提前避开。

### 3.2 流程（大体对应现在的 `npx cap`）

```bash
hionic platform add openharmony        # 生成 openharmony/ 工程（对应现在的 android/）
hionic plugin add @capacitor-ohos/camera @capacitor-ohos/filesystem \
                   @capacitor-ohos/local-notifications @capacitor-ohos/app
# 把 www/ 放进 entry/src/main/resources/rawfile/
hionic sync openharmony
hionic build openharmony               # 产出 HAP
```

再到 DevEco Studio 里签名、装到手机。

**与 Android 流程的差异**：
- 多一步：需要把 `capacitor.config.json` + `capacitor.plugins.json` 拷进 `rawfile`
  （因为鸿蒙版是**照 Android 的配置文件**来初始化插件的）
- WebView 是 **ArkWeb**，不是系统 WebView
- 需要签名（可以先自动生成调试证书）

---

## 4. 工作量评估

| 部分 | 工作量 | 说明 |
|---|---|---|
| **web 层**（`app.js` ~2600 行 + 4 个模块） | **≈ 0** | 原样复用 —— 这是最大的红利 |
| 工程搭建（DevEco + 平台包 + 4 插件 + 签名） | 中 | 首次主要在装环境和跑通签名 |
| `notify.js` 适配 | 小 | 去掉对 action listener 的依赖；`probe()` 已自带降级 |
| `backup.js` 适配 | 很小 | `EXTERNAL` → `DOCUMENTS`，一行 |
| `photo.js` 适配 | 小 | 落盘路径与 `convertFileSrc` 的对应关系要实测 |
| 验证体系 | 中 | 197 条 web 层断言**与平台无关、直接复用**；`verify-apk.py` 要扩一个 HAP 版 |

**首个能跑的版本：约 3-5 天**（含装环境和签名），之后与 Android 并行维护。

---

## 5. 风险与不确定项（如实说）

1. **⚠️ 最需要实测的一条**：`@capacitor-ohos/*` 由 CPF-Ionic 社区维护，README 只声明在
   SDK 5.0.5 / ROM 5.1.0.150 测过。**它能否用在你的具体机型上，只有装一次才知道** ——
   这也是我建议「先搭通最小可跑版本」的原因。
2. **锁屏按钮会退化**：鸿蒙侧不支持通知动作回调，「已服用 / 10 分钟后」这两个按钮要么去掉、
   要么改成「点通知打开 App 再操作」。这是**交互上的真实损失**，得你认可。
3. **备份位置变封闭**：鸿蒙的沙箱比 Android 的 `/Android/data/` 更难访问；
   `DOCUMENTS` 能否拿到 `getUserDocumentDir()` 取决于设备能力，要实测。
4. **平台包里带 `HotCodePushPlugin`（热更新）**：**华为应用市场对热更新有严格限制**。
   我们不需要这个能力，移植时**不要启用**。
5. **上架**：如果要上华为应用市场（AGC），需要用华为的证书与审核流程；
   只是自己/家人用则调试证书即可。

---

## 6. 建议的推进方式

### 6.1 三步走，**保证 Android 这条线完全不受影响**

1. **先确认手机系统版本**（第 0 节）—— 这一条决定要不要做
2. **搭通「能跑」**：装环境 → 空工程 → 加载现在的 `www/` → 看到界面、能点
3. **逐个通原生能力**（按从易到难）：
   `app`（生命周期，最简单）→ `filesystem`（改目录枚举即可）→ `camera`（路径要实测）→
   **`local-notifications`（最难，涉及提醒权限与清场机制）**

### 6.2 「边更新边移植」的关键纪律

我们现在这套代码**已经很适合双端了**，靠的就是把平台差异全部挡在插件层。

**要继续保持的两条**：
- ✅ **不要往 web 层写平台判断**（`if (isAndroid)` / `if (isHarmony)`）——
  差异要由**插件层吸收**，web 层只认 `window.Capacitor`
- ✅ **新功能先问一句「鸿蒙侧有没有对应能力」**，再决定怎么设计交互
  （比如当初通知按钮的设计，如果在鸿蒙上就不可能实现，那从一开始就不该依赖它）

**要警惕的两条**：
- ⚠️ 鸿蒙侧 `Directory` 语义与 Android **不同名同义**，别假设
- ⚠️ 我们那 197 条断言是**跑真实源码**的，与平台无关 —— 这是双端最可靠的安全网，
  每加一个功能就补一组，两个平台都受益

### 6.3 下一步（等你定）

| 选项 | 说明 |
|---|---|
| **A. 先查手机版本** | 最快 —— 一条版本号就能决定要不要做 |
| **B. 我先把环境装起来** | 装 DevEco Studio + HarmonyOS SDK（和当初装 Android 工具链一样，全程我来） |
| **C. 开一个 `harmony` 分支做试验** | 不动 `main`，android 这条线继续正常发版 |
| **D. 先只做 web 层的「双端准备」** | 把 `backup.js` 的目录枚举改成可配置、把通知按钮的交互改成不依赖 action listener —— 这些**在 Android 上也更好**，属于无损改进 |

> 我个人建议顺序：**A → D → B → C**。
> 其中 **D 是纯赚的**：就算最后不移植鸿蒙，这两处改动也让 Android 版更健壮
> （目录枚举可配、交互不依赖平台特有能力）。
