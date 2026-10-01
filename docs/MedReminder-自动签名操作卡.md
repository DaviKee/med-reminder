# DevEco 自动签名 —— 操作卡

> **目的**：试一次「自动签名」，看能不能**免申请**拿到代理提醒的配额（`1700002` 是否消失）。
> 依据：官方 FAQ《HarmonyOS 权限分类、受限权限审批前调试方法》——
> 「**本地可以使用调试 Profile 提前获取相关受限权限**」。
>
> **预计耗时**：10 分钟 ｜ **失败也不损失什么**（照常走 AGC 申请）

---

## 📋 操作前：当前状态（我已确认）

| 检查项 | 状态 |
|---|---|
| 工程 `runtimeOS` | ✅ 已是 **`HarmonyOS`**（自动签名的前提） |
| 工程 `signingConfigs` | `[]` 空 —— 正是待填状态 |
| 签名材料（`.p12`/`.cer`/`.p7b`） | 全无（全新开始） |
| 工程是否被 IDE 打开过 | ❌ 没有 `.idea` 目录 —— **这是你要做的第一步** |
| 本机是否安装 DevEco CLI | ❌ 没有 → **无法命令行自动签名，必须走 GUI** |

---

## 🔧 步骤

### Step 1 · 启动模拟器（**在 DevEco 里启动，别用我的脚本**）

> ⚠️ **为什么强调**：命令行的模拟器进程活在**我的 shell 进程树**里，我的任务一结束它就被清理。
> 用 DevEco 启动的模拟器归 IDE 管，可以**一直开着**，不会中途消失。

1. 打开 DevEco Studio
2. 菜单 **Tools → Device Manager**
3. 找到 **Mate 90 Pro**，点右侧 ▶ 启动
4. 等它进入桌面（冷启动约 1–3 分钟）

**为什么要先连设备**：调试 Profile 里有一份「**允许调试的设备列表**」，
自动签名需要读到设备来注册。没有设备时签名可能做不完整。

### Step 2 · 用 DevEco 打开工程

启动界面上选 **Open**，路径选：

```
C:\WorkBuddy\med-reminder\med-reminder\spike\harmony-test\openharmony
```

> ⚠️ **要打开 `openharmony` 这一层**，不是 `harmony-test` 那一层。
> 判断标准：打开后左侧能看到 `entry` 和 `capacitor` 两个模块。

等 IDE 索引完成（右下角进度条走完，首次打开会慢一些）。

### Step 3 · 配置自动签名

菜单 **File → Project Structure…** → 左侧选 **Project** → 右侧 **Signing Configs** 标签页

在这个页面上：

1. 勾选 ☑ **Automatically generate signature**
2. 勾选 ☑ **Support HarmonyOS**　← ⚠️ **这个别漏**（官方 FAQ 专门提到）
3. 点 **Sign In** 按钮

### Step 4 · 登录授权

1. 会**自动跳转浏览器**到华为账号登录页
2. 输入你已实名认证的账号 → 登录
3. 页面上点 **Allow / 允许**（授权 DevEco Studio 访问你的华为账号）
4. 回到 DevEco，等它自动生成证书与 Profile

### Step 5 · 确认成功

**成功的样子**：

- `Signing Configs` 页面里，`Store File` / `Certpath` / `Profile File` 三个字段**被自动填好**
- 有一行 **`Provisioning Profile: DevEco Managed Profile`**
  （把鼠标悬停上去，还能看到里面包含的权限清单）
- 页面底部可以点 **Apply → OK**

**工程侧的变化**：`openharmony/build-profile.json5` 里的
`"signingConfigs": []` 会被填成一段带证书信息的配置（这是成功最硬的证据）。

---

## 🔍 Step 6 · 验证（关键一步）

签名之后**重新编译 + 安装 + 跑探针**，看 `publishReminder` 还是不是 `1700002`。

我已准备好一键脚本：

```bash
cd C:/WorkBuddy/med-reminder/med-reminder
bash spike/harmony-test/verify-reminder-after-sign.sh
```

它会：**编译 → 卸载旧包 → 装新包 → 启动 → 等 26 秒 → 打印探针结果**。

**你要看的就一行**：

| 结果 | 含义 |
|---|---|
| `1) publishReminder OK  id=xxx` | 🎉 **成了！** 不用等 8 天就能开发和调试 |
| `1) publishReminder FAIL  code=1700002 ...` | 预期结果 → **照常走 AGC 申请**（正式发布也还是要申请） |

> ⚠️ **两种结果都要做 AGC 申请**（正式上架必须），区别只是**开发调试期能不能提前跑通**。

---

## ❓ 可能遇到的问题

| 现象 | 原因 | 怎么办 |
|---|---|---|
| **Sign In 后浏览器没反应** | 默认浏览器/代理问题 | 手动复制 IDE 里给的链接到浏览器打开 |
| **报 `9568320 The signature file does not exist`** | HAP 没签名 | 就是这一步要解决的；确认 Step 3 的勾选都做了 |
| **签名了但装不上，报签名验证失败** | 设备 UUID 不在 Profile 的调试设备列表里 | 确认 Step 1 的设备是在**自动签名之前**就连上的；必要时重新生成一次签名 |
| **`Provisioning Profile` 显示灰色/为空** | 登录没成功，或没勾 Support HarmonyOS | 回 Step 3 重来 |
| **自动签名后编译失败** | `signingConfigs` 与工程不匹配 | 看 hvigor 报错；把 `build-profile.json5` 的 `signingConfigs` 清回 `[]` 重新签 |
| **提示没有权限/需要申请** | 说明确实得走 AGC 申请 | 正常，回申请材料包 §3 |

---

## 🧹 如果想撤销自动签名

把 `openharmony/build-profile.json5` 里的 `signingConfigs` 改回 `[]` 即可
（未签名的 debug 包在**模拟器**上照样能装能跑，只是真机不行）。

> 撤销前先备份该文件 —— 里面有证书路径与口令。

---

## 📌 附：这一步在整个计划里的位置

```
✅ 已做：确认 1700002 = 没有「代理提醒」开放能力
⭐ 现在：试自动签名（本操作卡）—— 期望「开发调试期可用」
        ↓ 无论成败
📋 接着：AGC 申请「代理提醒」开放能力（8 工作日）—— 正式上架必须
        ↓ 通过后
🔑 手动签名（重新生成 .p7b）→ 才真正生效
```
