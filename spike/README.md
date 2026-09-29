# S-0 鸿蒙可行性 spike

> 目的：**验证 4 个 Capacitor 插件在鸿蒙上真能跑**。这是排期表里唯一可能推翻技术选型的调研，
> 所以排在大批功能之前做。
>
> 背景与完整论证见 **`MedReminder-后续任务计划.md` §24**；
> 能力对照见 **`MedReminder-鸿蒙移植调研.md`**。
>
> ⚠️ 这个目录是**试验性质**，鸿蒙原生工程产物已在 `.gitignore` 里排除。

---

## 当前进度

| 阶段 | 状态 |
|---|---|
| ① 调研（读源码 + npm 核实） | ✅ 已完成（§24） |
| ② 环境就绪自检脚本 | ✅ 已完成（`env-check.sh`） |
| ③ **安装 DevEco Studio** | ✅ **已完成**（2026-09-29，秦老师装于 `D:\Program Files\Huawei\DevEco Studio`） |
| ④ **环境核实（工具链可执行性）** | ✅ **已完成** —— SDK API 26 / `ohpm` 26.0.0.630 / `hvigorw` 6.26.8 / `hdc` 3.2.0f 全部**实跑**通过 |
| ⑤ **模拟器跑通** | ✅ **已完成**（2026-09-29）—— `hdc` 看到 `127.0.0.1:5555`，**已截图确认是完整 HarmonyOS 7.0.0 桌面** |
| ⑥ 建 harmony 工程（含 Capacitor 6→8） | ✅ **已完成** —— `hionic` 全自动：建工程 + 复制框架源码 + 装 4 插件 + 改 CMake + 注册插件 |
| ⑦ 编译 HAP | ✅ **已完成** —— `BUILD SUCCESSFUL`，42.8 MB 未签名 Debug 包 |
| ⑧ 装 + 跑 + 实测 4 插件 | ✅ **已完成** —— **应用在模拟器里跑起来了，19 通过 / 1 失败** |

---

## ✅ 实测结果（2026-09-29，这是本项目第一次拿到鸿蒙侧真实运行数据）

**应用启动成功，网页由 `ArkWeb/7.0.0.105` 渲染，4 个插件全部注册。**

| 插件 | 结果 |
|---|---|
| **App** | ✅ 11 个方法全在 |
| **Filesystem** | ✅ **12/12 全过** —— DATA / DOCUMENTS / CACHE / **EXTERNAL 全部能写能读** + readdir + getUri |
| **LocalNotifications** | ⚠️ 18 个方法在；`requestPermissions()`→granted、`schedule()` **成功**、`areEnabled()`→true；**但 `getPending()` 报 `Permission denied.`** |
| **Camera** | ⏳ 未测（需手动点按钮） |

> ★ **一处必须修正的判断**：我一度写「推翻调研文档（EXTERNAL 能写能读）」—— **草率了**。
> 实测 `getUri()` 的真实落点：
>
> | Directory | URI |
> |---|---|
> | `DATA` | `.../data/storage/el2/base/files/...` |
> | `CACHE` | `.../data/storage/el2/base/cache/...` |
> | **`EXTERNAL`** | **`.../data/storage/el2/base/files/...`（与 DATA 完全相同）** |
> | `DOCUMENTS` | **`file://docs/storage/Users/currentUser/Documents/...`（真·用户可见）** |
>
> → **调研文档是对的**：`EXTERNAL` 无分支、**静默落沙箱**。
> → **我们的自动备份用 `EXTERNAL`，在鸿蒙上用户看不到** → **必须改用 `DOCUMENTS`**（已实测可行）。
> → **教训：「不报错」≠「落点正确」**，这类静默降级最危险。

### ★★★ 最重要的发现：`getPending()` 在鸿蒙上不可用

**根因（查到源码级）**：插件实现调 `reminderAgentManager.getValidReminders()`
（`LocalNotifications.ets:257`），该 API 需要 **`ohos.permission.NOTIFICATION_AGENT_CONTROLLER`**
—— **系统级权限，普通应用申请不到**。

**为什么极其重要**：我们 v1.2.1 修「一次冒出一堆通知」的**整套清场机制，完全建立在 `getPending` 上**。

**好消息**：`cancelAll()` 本来就有兜底（`.catch(() => false)` → 退回自建台账 `pendingIds`），**不会崩**。

**⚠️ 但有个坑**：我们的判断是 `typeof LN.getPending !== 'function'` ——
鸿蒙上**函数存在、只是调用被拒** → 这个判断不成立，靠的是后面那层 `.catch` 兜底。
**移植时不能只依赖 `typeof` 判断。**

**移植建议**：鸿蒙上完全依赖自建台账 → 台账要**每次 schedule 后立即持久化**；
可考虑"保守清场"（用足够大的 id 范围 cancel）；真机要再验一次。

---

## ⚠️ 七个坑（全部实测踩过，`spike/README` 与今日记忆里有完整版）

| # | 坑 | 解法 |
|---|---|---|
| 1 | `npm install` 卡死 8 分钟 | 用 `--registry=https://registry.npmmirror.com` → **29 秒** |
| 2 | `hionic buildapp` 强制要签名配置 | 绕过它，直接 `hvigorw assembleHap`（**Debug 包不需签名**） |
| 3 | `hdc install` 把绝对路径拼错 | **cd 进目录，用文件名** |
| 4 | `Cannot find module 'harmony-capacitor'` | 在 `openharmony/` 跑 **`ohpm install`** |
| 5 | ★ `ninja: error: libssl.so.3 missing` | **集成 openssl**：克隆 `openharmony-capacitor-openssl3.5`，拷 `libs/`→`capacitor/`、`openssl/`→`capacitor/src/main/cpp/`（**有 x86_64**） |
| 6 | 加 `compileSdkVersion` 报 00303313 | **删掉**（hvigor 用内置的，不要显式配） |
| 7 | `hionic sync` 没同步 www | **手动 `cp`** 到 `rawfile/www/` |

> ⚠️ **最大的教训**：第 5 条我一度以为"openssl 不是问题"—— 因为第一次编译时
> `BuildNativeWithNinja` 显示 `Finished ... after **6 ms**`。
> **6 毫秒不可能编译 C++，那是缓存跳过（`10 up-to-date`）。**
> **干净的重新编译立刻暴露了真错误。**
> → **「Finished」不等于「真干过活」。判断构建成功要看实际耗时和产物，别只看状态行。**

---

## 🔁 一键复现（照抄即可）

```bash
cd C:/WorkBuddy/med-reminder/med-reminder/spike/harmony-test
source ../env-harmony.sh
npm install --registry=https://registry.npmmirror.com --legacy-peer-deps --no-audit
./node_modules/.bin/hionic platform add openharmony
for p in app camera filesystem local-notifications; do ./node_modules/.bin/hionic plugin add "@capacitor/$p"; done

# ★ 集成 openssl —— 不做这步编译必失败
git clone --depth 1 https://gitcode.com/li_in/openharmony-capacitor-openssl3.5.git /tmp/openssl-repo
cd openharmony && ohpm install
cp -r /tmp/openssl-repo/libs    capacitor/
cp -r /tmp/openssl-repo/openssl capacitor/src/main/cpp/
cp ../www/index.html entry/src/main/resources/rawfile/www/     # ★ 手动同步网页

hvigorw assembleHap --mode module -p product=default -p buildMode=debug --no-daemon
cd entry/build/default/outputs/default
hdc -t 127.0.0.1:5555 install entry-default-unsigned.hap
hdc -t 127.0.0.1:5555 shell aa start -b com.medreminder.spike -a EntryAbility
```

---

## 🧪 最小测试工程 `harmony-test/`

**独立工程，完全不碰主项目（零风险）**。跑通的前半程（实测）：

| 步骤 | 命令 | 结果 |
|---|---|---|
| 装依赖 | `npm install --registry=https://registry.npmmirror.com --legacy-peer-deps` | ✅ **29 秒** |
| 建工程 | `hionic platform add openharmony` | ✅ 自动复制框架源码到 `openharmony/capacitor/` |
| 装 4 插件 | `hionic plugin add @capacitor/<名>` | ✅ 4/4，自动改 CMake + 注册插件 |
| 同步网页 | `hionic sync openharmony` | ✅ 测试页进 `rawfile/www/` |
| ohpm 依赖 | `ohpm install` | ✅ 1.4 秒 |

> ⚠️⚠️ **`npm install` 必须用国内镜像** —— 默认源**卡死 8 分钟**（`node_modules` 一直空），
> 换 `registry.npmmirror.com` **29 秒**完成。这一条能省你 8 分钟。

**测试页 `www/index.html`** 是分级探测设计（基础环境 → App → Filesystem → 通知 → 相机），
结果直接显示在页面上，跑起来后截图即可判读。

### hionic 完整命令链（官方 README）

```bash
hionic init <应用名> <包名>              # 已有 web 项目就地初始化
hionic platform add openharmony
hionic plugin add @capacitor/camera      # ← 用**官方包名**，hionic 自动找鸿蒙实现
hionic sync openharmony                   # 同步网页资源
hionic buildui                            # 构建前端（如有前端框架）
hionic buildapp openharmony               # 编 HAP（**会强制检查签名配置**）
hionic run openharmony                    # 装到设备并启动
```

**环境变量**（`source spike/env-harmony.sh` 一键设置）：
`DEVECO_SDK_HOME` + `DEVECO_IDE_PATH`，另需 `hdc`/`ohpm`/`hvigor` 在 PATH。

**目录结构**：
```
harmony-test/
├── openharmony/               # 鸿蒙原生工程
│   ├── capacitor/             # capacitor 框架（自动从 @capacitor-ohos/ohos 复制）
│   └── entry/.../rawfile/www/ # 网页资源
├── www/index.html             # 我的测试页（源）
├── capacitor.config.json
└── package.json
```

> ⚠️ 模板默认带 `HotCodePushPlugin`（热更新）——**华为应用市场对此有严格限制，不要启用**。

---

## ✅ 好消息：**模拟器可用，不需要真机也能开工**

原先的判断「spike 必须真机」**过于绝对**。实测结论：

- 模拟器是**完整的 HarmonyOS 7.0.0 系统**（不是简化渲染），`hdc` 正常、shell 正常
- **而且能用命令行操作它** —— 这对自动化验证极有价值：
  ```bash
  Emulator.exe -instance "Mate 90 Pro" -screenshot -screenshotPath <目录>
  Emulator.exe -instance "Mate 90 Pro" -click <x> <y>
  Emulator.exe -instance "Mate 90 Pro" -slide "<x1> <y1> <x2> <y2>"
  ```
  → **意味着 4 个插件的验证可以在模拟器上自动跑，我能自己截图看结果。**

**模拟器的能力边界**（华为官方文档）：

| 能力 | 模拟器（x86） | 对我们的影响 |
|---|---|---|
| **系统通知** | ✅ 支持 | `local-notifications` 可验 |
| **Web / ArkUI / 窗口** | ✅ 支持 | `www/` 界面可验 |
| **文件管理** | ✅ 支持 | `filesystem` 可验 |
| **相机** | ⚠️ 官方文档两处说法冲突（一处说 x86 支持、一处说 ×） | **必须实测** —— 见下方验证清单 |
| 推送 / 华为账号 / 分布式 | ❌ 不支持 | 我们没用 |
| 蓝牙 / NFC / 传感器 | ❌ 不支持 | 我们没用 |

> ⚠️ **相机是模拟器最关键的不确定项**。好在启动日志里有
> `Enable Camera. / Enable front and back cameras.`，而且模拟器支持「虚拟相机」
> （官方 FAQ：无摄像头时可用虚拟相机）—— 但**能不能真出图**必须实测。

**真机仍然值得连** —— 用来确认「秦老师的具体机型上确实能跑」，
这是调研文档唯一没验证的假设（适配层只声明测过 ROM 5.1.0.150）。但**不再是阻塞项**。

---

## ▶️ 开工：启动模拟器

```bash
cd C:/WorkBuddy/med-reminder/med-reminder
bash spike/start-emulator.sh          # 默认设备 Mate 90 Pro
```

脚本会自动：**先接受协议 → 启动 → 等 hdc 就绪**，成功后打印 `127.0.0.1:5555`。

> 💡 **若要长期使用，建议在 DevEco Studio 的 Device Manager 里启动** ——
> 更稳（不依赖命令行会话），而且有图形化错误提示。

---

## ⚠️⚠️ 三个坑（2026-09-29 现场踩出来的，都花了时间）

### 坑 1 —— 启动参数：**不要传 `-instancePath` / `-imageRoot`**

传了会 **794ms 就退出**，报 `Unable to start the emulator`，**而且连日志都不写** ——
看着像环境问题，其实是参数问题。模拟器自己从 `deployed/lists.json` 读得到路径。

```bash
# ✅ 正确
Emulator.exe -start "Mate 90 Pro"
# ❌ 错误（会静默失败）
Emulator.exe -start "Mate 90 Pro" -instancePath "..." -imageRoot "..."
```

### 坑 2 —— ★ **必须先 `-license accept`**（这是最误导人的一个）

**现象**：模拟器能启动、能显示开机动画、日志看起来一切正常，
但 **12 秒后自己 `quit`**（日志末尾 `StopMultiScreen before quit` / `quit emulator`），
`hdc` 永远等不到设备。

**极易误判成**：Hyper-V 没开 / 镜像坏了 / 内存不够。

**真因**：`%LOCALAPPDATA%\Huawei\Emulator26.0\.emu_config` 里
`HarmonyOS_SDK_Agreement: disagree` **没接受**。

```bash
Emulator.exe -license accept     # 输出 "All licenses have been automatically accepted." 后一次就起来
```

### 坑 3 —— 模拟器进程要活在我们的 shell 里

沙箱会清理「脱离进程树」的子进程：用 Python `DETACHED_PROCESS` 启动的会被杀掉，
**连日志都来不及写**。所以 `start-emulator.sh` 用「后台运行 + shell 保持存活」的方式。

---

## ✅ 已验证的环境事实（2026-09-29 实测）

| 项 | 值 | 怎么验的 |
|---|---|---|
| DevEco Studio | `D:\Program Files\Huawei\DevEco Studio` | 目录 + `product-info.json` |
| **HarmonyOS SDK** | **API 26 · HarmonyOS 26.0.0 Release** | 读 `sdk/default/sdk-pkg.json` |
| SDK 位置 | **内嵌在安装目录** `sdk/`（**不是** `%LOCALAPPDATA%\Huawei\Sdk`） | 目录实查 |
| `ohpm` | **26.0.0.630** | `ohpm.bat -v` **实跑** |
| `hvigorw` | **6.26.8** | `hvigorw.bat --version` **实跑** |
| `hdc` | **Ver 3.2.0f** | `hdc -v` **实跑** |
| 模拟器镜像 | `HarmonyOS-7.0.0/phone_all_x86`，4.6 GB | 目录实查 |
| **模拟器设备** | **`127.0.0.1:5555`**，API 26，x86_64，Model `emulator` | `hdc list targets` + `hdc shell param get` |
| **模拟器界面** | **完整 HarmonyOS 7.0.0 桌面（已截图）** | `Emulator.exe -screenshot` |
| Hyper-V | ✅ 已启用（`hvix64.exe` 存在 + systeminfo「已检测到虚拟机监控程序」） | 二进制 + systeminfo |

> ⭐ **三个好消息**：
> ① **SDK 是 API 26**，比调研文档记的 API 17 新得多，能力只会更全；
> ② **SDK 已随 IDE 装好**，省掉了「首次启动下载 SDK」这一大步；
> ③ **Hyper-V 本来就启用了** —— 我一开始误判成"没启用"（读了 18:20 的旧日志，
>    且自检脚本用「vmms 服务是否注册」判断是**错的方法**，会把没启用误报成已启用）。
>    **教训：报 False/True 都要先怀疑自己的判据。**

> ⚠️ **一个提醒**：以上是「工具能跑、模拟器能起」，**不等于「插件能跑」** ——
> 后者正是 spike 要验证的东西。别把绿色当成 spike 已成功。

---

## 随时可跑：环境自检

```bash
cd C:/WorkBuddy/med-reminder/med-reminder
bash spike/env-check.sh
```

它只读、不改任何东西，逐项告诉你还缺什么。

| 退出码 | 含义 |
|---|---|
| `0` | 全就绪，可以开始 spike |
| `1` | 缺非关键项 |
| `2` | **关键项缺失**（DevEco / SDK 没装好） |

**它会检查 8 类**：DevEco Studio · HarmonyOS SDK · `ohpm`/`hvigorw`/`hdc` ·
**设备连接（真机/模拟器）** · Node/JDK/内存 · 磁盘 · npm 侧鸿蒙包 ·
**虚拟化 + 模拟器协议 + 镜像**。

---

## 环境就绪后的执行顺序（预计半天）

```bash
# 0. 分支隔离 —— main 与 Android 线完全不受影响
git checkout -b harmony

# 1. ★ 依赖升级（本轮新发现的关键步骤）
#    鸿蒙适配层要求 @capacitor/core ^8，本项目现为 6.2.2
#    必须先读 4 个插件的 changelog，不是无脑 install
npm install @capacitor/core@^8 @capacitor/cli@^8 @capacitor/android@^8

# 2. 鸿蒙平台层 + 4 插件（已 npm 核实全部存在）
npm install @capacitor-ohos/ohos @capacitor-ohos/app \
  @capacitor-ohos/camera @capacitor-ohos/filesystem \
  @capacitor-ohos/local-notifications
npm install hionic@2.1.16

# 3. 生成鸿蒙工程（对应 android/）
hionic platform add openharmony
hionic sync openharmony
hionic build openharmony        # 产出 HAP
```

### 验证顺序（从易到难）

| 序 | 插件 | 验证什么 | 风险 |
|---|---|---|---|
| 1 | `app` | 生命周期、返回键 | 低 |
| 2 | `filesystem` | 改 `DIRS` 枚举（`EXTERNAL`→`DOCUMENTS`）后备份能否落盘 | 低 |
| 3 | `camera` | `getPhoto` + `source:'CAMERA'` + `convertFileSrc` 路径 | 中 |
| 4 | **`local-notifications`** | **最难** —— 权限、`getPending` 清场、`schedule` | **高** |

### 🆕 追加验证项（2026-09-29 商业竞品调研得出）

盘了华为应用市场上的商业竞品后，发现**三个竞品已做、我们全缺**的能力
（详见 `MedReminder-开源竞品调研与排期.md` 第五部分 · 排期表 S-9/S-10/S-11）。
**三项都不是纯 web 层能做到的** → 必须在这里一并验证，否则会出现「移植完才发现做不了」：

| 验证项 | 对应竞品 | 要回答的问题 |
|---|---|---|
| **桌面卡片（widget）** | 药管家 / 爸妈的药盒 / 系统三叶草 | 鸿蒙侧能否给 Web 应用加**服务卡片**？Capacitor 有没有对应能力？ |
| **语音播报药名（TTS）** | 爸妈的药盒 | 鸿蒙侧有无 TTS 能力？`@capacitor-ohos` 是否封装了？ |
| **手表端通知** | 药管家 / 叮当快药 / 系统 | **通知能否同步到华为手表并振动**？这是"最不容易被忽略"的提醒方式 |

> ⚠️ 优先级：**手表通知 > 桌面卡片 > TTS**。
> 手表通知影响的是"提醒能不能真的送到"（核心功能），另两项是体验加成。

---

## 已知风险（如实记）

1. **版本差距**：适配层要求 Capacitor **^8**，我们在 **6.2.2** → 要先跨一次大版本升级。
   文档原来估「web 层 ≈ 0」，这个结论**只对 `www/js/` 成立**，工程配置层要算工作量。
2. **机型未验证**：`@capacitor-ohos/*` 只声明在 **SDK 5.0.5 / ROM 5.1.0.150** 测过。
   能否用在秦老师的具体机型上，**只有真机装一次才知道** —— 这是 spike 存在的全部理由。
3. **`local-notifications` 不支持 `localNotificationActionPerformed`** →
   Android 上「锁屏直接点已服用」这个体验在鸿蒙上会退化。
4. **`HotCodePushPlugin`（热更新）**：平台包自带，**不要启用** —— 华为应用市场对此有严格限制。

---

## ⚠️ 一个被纠正的判断

调研文档 §6.3 把「双端准备」（改 `backup.js` 目录枚举、通知不依赖 action listener）
写成「**纯赚**」—— **实际不是**（详见 §24.5）：

- `DIRS` 可配置 → 需要平台判据 → **破了「web 层不写平台判断」纪律**
- 通知不依赖 action listener → **牺牲 Android 现有体验**

→ **不提前做**，等 spike 结果出来再定。
