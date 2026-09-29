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
| ③ **下载 + 安装 DevEco Studio** | ⏸ **等秦老师**（需华为账号登录） |
| ④ 建 harmony 分支 + 跑通最小工程 | ⏳ 待环境就绪 |
| ⑤ 逐个验证 4 个插件 | ⏳ |

---

## 🔴 卡在哪里：DevEco Studio 下载需登录华为开发者账号

这一步**只能由秦老师做** —— 我无法代登录（也不应接触账号密码）。

**要做的事**：

1. 打开 <https://developer.huawei.com/consumer/cn/download/>
2. 登录华为账号（**未注册需先注册 + 实名认证**）
3. 下载 **DevEco Studio 6.0.0 Release**（Build 6.0.0.858）Windows 64 位版
   - ⚠️ 认准 **Release**，不要 Beta
4. 存到任意目录（**路径不能有中文或空格**），例如 `C:\Users\Qinn\Downloads\`
5. 告诉小八路径 → 从这里开始我接管

> 系统要求：Win 10/11 64 位 · 内存 16GB（本机 15.8GB，**刚好够，但建议只用真机不用模拟器**）· 硬盘 100GB（本机 C 盘可用 163GB ✅）

---

## 环境就绪后：先跑自检

```bash
cd C:/WorkBuddy/med-reminder/med-reminder
bash spike/env-check.sh
```

它只读、不改任何东西，会逐项告诉你还缺什么。

| 退出码 | 含义 |
|---|---|
| `0` | 全就绪，可以开始 spike |
| `1` | 缺非关键项 |
| `2` | **关键项缺失**（DevEco / SDK 没装好） |

**它会检查 7 类**：DevEco Studio · HarmonyOS SDK · `ohpm`/`hvigorw`/`hdc` 命令行工具 ·
真机连接 · Node/JDK/内存 · 磁盘 · npm 侧鸿蒙包可达性。

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
