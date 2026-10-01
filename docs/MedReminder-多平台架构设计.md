# MedReminder 多平台架构设计（2026-10-01）

> **目的**：把软件结构重新模块化，为**多平台**（Android / 鸿蒙 HAP / 将来）做准备。
> **制定人**：小八 ｜ **状态**：**待秦老师确认后执行**
> **本文所有"现状"均为 2026-10-01 实测**（行号、调用点、次数都来自真实代码，不是回忆）。
> 接续 `MedReminder-后续任务计划.md` **§18（架构重构）**，不是另起炉灶。

---

## 0. 一句话结论

**平台差异其实已经被"收"得不错了 —— 但只收了一半，而且收在了错误的位置（技术形态上）。**

真正要做的不是推倒重来，而是三件事：
1. 把**已有的平台层显式化**（建 `platform/` 目录，把 3 个全局脚本搬进去并 module 化）；
2. 把**漏在 app.js / overlay.js 里的"平台生命周期"**（返回键、恢复事件、插件清单）搬进平台层；
3. 然后才拆 `app.js` 这座 2519 行的 IIFE 巨兽（= §18 的第 6/7/8 步）。

---

## 1. 现状诊断（实测数据）

### 1.1 代码规模

| 文件 | 行数 | 形态 | 平台依赖 |
|---|---:|---|---:|
| `www/js/app.js` | **2519** | 单个 IIFE | 4 处* |
| `www/js/notify.js` | 465 | IIFE + `window.MedNotify` | 5 |
| `www/js/photo.js` | 362 | IIFE + `window.MedPhoto` | 1 |
| `www/js/backup.js` | 212 | IIFE + `window.MedAutoBackup` | 1 |
| `www/js/core/schedule.js` | 386 | **ES module** | **0** ✅ |
| `www/js/core/store.js` | 159 | **ES module** | **0** ✅ |
| `www/js/core/util.js` | 61 | **ES module** | **0** ✅ |
| `www/js/ui/overlay.js` | 152 | **ES module** | 1 |

### 1.2 四个关键发现

**① ✅ `core/` 层是干净的** —— `util / store / schedule` 三个模块**零平台依赖、零 localStorage 直连**。
这是本项目最值钱的资产：**纯 JS、无框架、可单测**，任何平台路线下都保留。

**② ⚠️ 项目现在跑的是"双轨制"**

`index.html` 的真实装配（实测）：

```html
<!-- 传统全局脚本 -->
<script src="vendor/capacitor.js"></script>
<script src="vendor/plugin-app.js"></script>
<script src="vendor/plugin-local-notifications.js"></script>
<script src="vendor/plugin-camera.js"></script>
<script src="vendor/plugin-filesystem.js"></script>
<script src="js/notify.js"></script>      <!-- ← 全局脚本 -->
<script src="js/backup.js"></script>      <!-- ← 全局脚本 -->
<script src="js/photo.js"></script>       <!-- ← 全局脚本 -->

<!-- ES module -->
<link rel="modulepreload" href="js/core/util.js">
<link rel="modulepreload" href="js/core/store.js">
<link rel="modulepreload" href="js/core/schedule.js">
<link rel="modulepreload" href="js/ui/overlay.js">
<script type="module" src="js/app.js"></script>
```

→ **`core/`+`ui/` 是新体系，`notify/photo/backup` 是旧体系**，两者靠 `window.MedXxx` 全局变量通信。
这不是设计，是**重构做到一半的历史状态**。

**③ ✅ 但"平台差异收在插件层"这条纪律，实际上已经执行得不错**

`app.js` 对业务能力的调用**全部**走平台层：

```
window.MedPhoto.ready() / gc() / src() / dirStats() / fromRestored()   ← 147 处中的拍照流程
window.MedNotify.sync() / cancelOne() / probe() / purge() / stat()     ← 通知
window.MedAutoBackup.schedule()                                        ← 备份
```

`photo.js` 的注释自己就写着：**"把原生插件差异收在这一层，app.js 只管打卡流程"** —— 方向是对的。

**④ ❌ 真正漏掉的是"平台生命周期"** —— 它们散在**业务文件**里：

| 位置 | 内容 | 应有的归属 |
|---|---|---|
| `app.js:1886-1891` | **已注册插件清单**（能力探测） | `platform/capabilities.js` |
| `app.js:2323-2325` | `appRestoredResult` 监听（相机 Activity 回来） | `platform/lifecycle.js` |
| `app.js:2429-2430` | **返回键**注册（`Plugins.App.addListener('backButton')`） | `platform/lifecycle.js` |
| `app.js:2485` | `Plugins.App` 又一次直接取用 | `platform/lifecycle.js` |
| `ui/overlay.js:104` | `Plugins.App`（返回键判定） | `platform/lifecycle.js` |

**这 5 处是"多平台"最致命的**：它们是**平台语义**（Android 有物理返回键、有相机 Activity 恢复；鸿蒙不一样），
却住在**业务层**。加一个平台要在业务代码里动刀 —— 正是工程纪律 §4.1 想避免的事。

**⑤ ⚠️ 辅助状态散落**：`app.js` 里 12 处 `localStorage` 都是**辅助 key**（`PENDING_KEY` 拍照留痕、
`FS_KEY` 字号、`FS_HINT_KEY`），主状态已正确走 `core/store.js`。
这些辅助 key 在多平台/数据迁移（影子状态文件、鸿蒙卡片读数据）时也要一起考虑。

### 1.3 一句话总结现状

```
业务逻辑    core/  ────────────────────  ✅ 干净、可单测
平台能力    notify/photo/backup ───────  ✅ 概念对，但形态旧（全局脚本）
平台生命周期 ← 漏在 app.js / overlay.js   ❌ 多平台的真正障碍
视图         app.js 2519 行 IIFE ───────  ❌ 改动定位成本高、render 全量重建
数据         主状态 ✓ / 辅助状态散落 ⚠️
```

---

## 2. 目标架构

```
www/js/
├── platform/                    ★ 平台适配层：唯一允许出现平台差异的地方
│   ├── capabilities.js          插件清单 / 平台判定（app.js:1886 搬来）
│   ├── lifecycle.js             appRestoredResult / 返回键 / 前后台（app.js+overlay 5 处搬来）
│   ├── notifications.js         ← notify.js（原样搬入 + module 化）
│   ├── camera.js                ← photo.js（原样搬入 + module 化）
│   ├── storage.js               ← backup.js（原样搬入 + module 化）
│   └── README.md                本层契约（见 §3.2）
│
├── core/                        ✅ 已就位
│   ├── util.js  store.js  schedule.js
│   └── actions.js               ⬜ §18 第 7 步（打卡/删除/重排，需钩子）
│
├── data/                        ⬜ 数据层（P2）：辅助 key 收口 + 影子状态文件
│
├── ui/                          视图层
│   ├── overlay.js               ✅
│   └── today.js / records.js / meds.js   ⬜ §18 第 6 步
│
└── main.js                      ⬜ 入口 + 装配 + 订阅式重绘（§18 第 8 步）
```

### 依赖方向（铁律，不许违反）

```
platform  ──┐
            ├──→  core  ──→  (util)
data      ──┘        ↑
                     │
ui  ─────────────────┘
main ──→ 装配以上所有（唯一允许"知道全部"的地方）
```

- **`core/` 永不 import `platform/`、`ui/`、`data/`**（已达标，保持）
- **`platform/` 永不 import `ui/`**；`platform/` 之间可互相依赖
- **`ui/` 只 import `core/`（读数据）+ `platform/`（调能力）**
- `core/` 需要上层副作用时**走钩子注入**（已确立的 `saveHooks` 模式）

---

## 3. 关键设计决策

### 3.1 ⭐ 现在**不做**"多平台抽象"

**这是本方案最重要的一条。**

现状只有 **Android 一个真实平台**（鸿蒙还在等 AGC 审批，尚未落地）。
此时写 `adapters/ohos.js`、设计"平台接口"是**凭空猜**，必然猜错 —— 到时候要同时改接口和实现。

**本方案的做法：只做"物理收口"，不做"接口抽象"。**

- ✅ 把平台代码**全部搬进 `platform/` 一个目录** → 将来加平台时，改动被物理限制在一处
- ✅ 每个模块保留自己现有的函数签名（`MedNotify.sync(list)` 等）→ **零行为变化**
- ❌ **不**发明统一的 `PlatformAdapter` 接口
- ❌ **不**写 `ohos.js` 空壳

> **抽象的正确时机**：第二个真实平台落地时，**照着两个真实实现的差异**来抽 —— 那时才知道接口该长什么样。
> 这与既定战略「Capacitor 壳 + 渐进原生模块」一脉相承。

### 3.2 `platform/` 层的契约（写进 `platform/README.md`）

1. **本层是唯一允许出现 `window.Capacitor` / 平台判断的地方**（业务层出现即违规）
2. 每个能力模块必须**自带降级**（插件不存在 → 返回安全默认值，不抛错）
3. **不 import `ui/`**；需要通知界面时走**回调参数**（现有做法，保持）
4. 每个模块导出一个**对象**（保持现有 `MedXxx` 的方法名，便于逐字搬迁）
5. **能力探测必须用"实际能不能用"判断，不能用 `Capacitor.isNativePlatform()`**（恒 `true`，已踩过）

### 3.3 三个全局脚本怎么搬

**逐字搬迁 + 只改包装，不改函数体**（§18 已确立的纪律）：

| 原 | 新 | 改动 |
|---|---|---|
| `www/js/notify.js` | `www/js/platform/notifications.js` | 去掉最外层 IIFE，加 `export`，`window.MedNotify` 保留为兼容层（过渡期） |
| `www/js/photo.js` | `www/js/platform/camera.js` | 同上 |
| `www/js/backup.js` | `www/js/platform/storage.js` | 同上 |

> ⚠️ 过渡期**同时保留 `window.MedXxx`**，让 `app.js` 那 20+ 处调用**一行都不用改** →
> 把"搬迁"与"改调用点"分成两步，符合 §18.5「一次只改一件事」。

---

## 4. 分期计划

| 期 | 内容 | 风险 | 前置 |
|---|---|---|---|
| **P0** | **平台层显式化**：建 `platform/`，搬入 3 个全局脚本并 module 化 + 把 5 处平台生命周期搬进 `platform/lifecycle.js` | 低（纯搬运） | — |
| **P1** | **拆 app.js**（= §18 第 6/7/8 步）：`ui/today.js` → `ui/records.js` → `core/actions.js` → `main.js` + 订阅式重绘 | 中 | P0 |
| **P2** | **数据层**：辅助 key 收口进 `data/`；**影子状态文件**（为鸿蒙卡片/Agent 铺路，见《数据层迁移预研》） | 中 | P1 |

**建议先做 P0**：它风险最低、且直接服务于"多平台"这个目标；
P1 是"代码可维护性"，P2 依赖鸿蒙落地进度（可等审批）。

### P0 的细分步骤与进度

| # | 步骤 | 状态 |
|---|---|---|
| 0.1 | 建 `platform/` + README（写契约） | ✅ `df88433` |
| 0.2 | 搬 `notify.js` → `platform/notifications.js` | ✅ `df88433` |
| 0.3 | 搬 `photo.js` → `platform/camera.js` | ✅ `df88433` |
| 0.4 | 搬 `backup.js` → `platform/storage.js` | ✅ `df88433` |
| 0.5 | 抽 `platform/capabilities.js`（`app.js:1886` 的插件清单） | ⬜ **下一步** |
| 0.6 | 抽 `platform/lifecycle.js`（返回键 / `appRestoredResult` / 前后台，共 5 处） | ⬜ |
| 0.7 | 出包 + 真机验证 | 🟡 **v1.4.12 已出包**（`df88433`），待真机确认 |

> 📌 **0.2–0.4 的实际做法**（与最初设想不同，更稳）：
> 用的是 **`git mv` + 保持 IIFE 形态**，**没有**在搬迁的同一步里做 module 化。
> 理由：§18.5「一次只改一件事」—— 把"搬位置"与"改形态"分开，
> 出问题才分得清是哪一步的错。
> 结果：`git` 把 3 个文件都识别为 **100% 相似度的 rename**（证明确实"一字未改"），
> 13 套 478 条 spec 全绿、条数不变。
>
> 搬迁同时更新了 **9 处引用**（`index.html` / `sw.js` ×2 / `harness/gates.py` /
> 4 个 spec / `verify-apk.py` ×3）—— 这是无打包器项目里最容易漏的地方，
> 详见 `www/js/platform/README.md` 末尾的清单。
>
> ⏭ **module 化**（加 `export` + `app.js` 改 `import`、去掉 `window.MedXxx`）**留到后一步**，
> 届时需要同步改 4 个 spec 的源码读取方式（它们现在直接按路径读文本喂给 `vm`）。

### 无打包器的代价（每加一个模块必须做四件事，§18.3）

1. 写 `www/js/<层>/<名>.js`，用 `export`
2. `index.html` 加 `<link rel="modulepreload">` 或 `<script>`
3. `sw.js` 的 `ASSETS` 加文件 + `CACHE` 版本号 **+1**
4. `tests/sources.js` 的 `MODULES` + `tools/verify-apk.py` 的 `APP_MODULES` 各加一行

---

## 5. 验收纪律（沿用 §18.4，三条独立证据）

1. `harness/harness.py` → **16 关口全绿，且 spec 条数必须与重构前一致**（结构改动不改条数）
2. `node --check` 按 ES module 过一遍 + **真 import 模块**跑关键函数
3. 改了 `verify-apk.py` 就跑 `selftest-verify-apk.py`（合成 APK 验证验收脚本本身）
4. **每个平台搬迁步骤后必加一条**：`grep -rn "window.Capacitor" www/js/core www/js/ui` 必须**为空**（验证平台代码没漏在业务层）

---

## 6. 明确**不做**的事

| 不做 | 原因 |
|---|---|
| 引入打包器（webpack/vite） | 现有体系已跑通且真机验证过；引入构建步骤会改变整个出包链路，收益不抵风险 |
| 现在写 `adapters/ohos.js` | 只有 Android 一个真实平台，抽象必然猜错（见 §3.1） |
| 改成 TypeScript | 同上，且无打包器下 TS 需要额外编译步骤 |
| 引入框架（Vue/React） | 违反"纯 JS、可单测"的既有优势；重写成本极高 |
| 全量重写 ArkTS | 已于 2026-09-30 决策否决（双端维护翻倍） |

---

## 7. 待秦老师确认的点

1. **是否同意"只收口、不抽象"**（§3.1）—— 这是本方案的核心判断
2. **先做 P0（平台层）还是先做 P1（拆 app.js）** —— 建议 P0
3. 是否同意**过渡期保留 `window.MedXxx` 兼容层**（避免一次改 20+ 处调用点）
