# platform/ —— 平台适配层

> **本目录是唯一允许出现「平台差异」的地方。**
> 业务层（`core/`、`ui/`）里出现 `window.Capacitor` / 平台判断，即为违规。

## 为什么有这一层

目标：**多平台**（Android / 鸿蒙 HAP / 将来）。
项目纪律 §4.1：**平台差异不得带出插件层** —— 双端同步的成本只取决于这一条。
把平台代码**物理收口**到一个目录，加平台时就只需动这里。

## 本层契约（5 条）

1. **唯一入口**：`window.Capacitor` / `Capacitor.Plugins.*` / 平台判定，**只允许在本目录出现**。
2. **自带降级**：插件不存在时返回安全默认值，**不抛错**（浏览器里也要能跑）。
   ⚠️ **不要用 `Capacitor.isNativePlatform()` 判可用性** —— 它恒为 `true`（已踩过）。
   正确写法：`!!(window.Capacitor && window.Capacitor.Plugins && window.Capacitor.Plugins.Xxx)`。
3. **不 import `ui/`**：需要与界面交互时走**回调参数**（现有做法，保持）。
4. **导出形态**：过渡期同时挂 `window.MedXxx`（兼容旧调用点）—— 见下方「过渡期」。
5. **只能被上层调用**，不可反向 import `app.js`（会成环）。

## 依赖方向（铁律）

```
platform ──┐
           ├──→ core ──→ util
ui ────────┘
main ─────────→ 装配以上所有（唯一允许「知道全部」的地方）
```

- `core/` **永不** import `platform/`；`platform/` **永不** import `ui/`
- `core/` 需要上层副作用时**走钩子注入**（范例：`store.save()` 的 `saveHooks`）

## 模块清单

| 文件 | 职责 | 来源 |
|---|---|---|
| `notifications.js` | 通知：权限、排程、清场（`MedNotify`） | 原 `js/notify.js` |
| `camera.js` | 拍照：相机调用、照片落盘与孤儿回收（`MedPhoto`） | 原 `js/photo.js` |
| `storage.js` | 文件落盘与自动本地备份（`MedAutoBackup`） | 原 `js/backup.js` |
| `capabilities.js` | 已注册插件清单 / 能力探测 | ⬜ 待抽（现散在 `app.js:1886`） |
| `lifecycle.js` | `appRestoredResult` / 返回键 / 前后台 | ⬜ 待抽（现散在 `app.js`、`ui/overlay.js`） |

## 过渡期：为什么保留 `window.MedXxx`

搬迁分两步走，**一次只改一件事**（§18.5）：

1. **先搬位置**（保持 IIFE 形态）→ 行为零变化，只改路径
2. **再 module 化**（加 `export` + `app.js` 改 `import`）

第 1 步期间，`app.js` 里 20+ 处 `window.MedNotify.xxx()` **一行都不用改** ——
把「搬迁」与「改调用点」分开，出问题才分得清是哪一步的错。

## 加新平台时（将来）

**现在不做抽象** —— 现状只有 Android 一个真实平台（鸿蒙还在等 AGC 审批），
此时设计「平台接口」是凭空猜，必然猜错。当第二个真实平台落地时，
**照着两个真实实现的差异**再抽公共接口。

新增一个平台的改动范围，应当**被限制在本目录内**。

## ⚠️ 无打包器的代价：加/移文件必须同步改四处

少一处就出问题：

1. `www/index.html` —— `<script>` 或 `<link rel="modulepreload">`
   （SW 的预缓存清单靠**解析 html** 得到，而 **import 是隐式的、解析不到**）
2. `www/sw.js` —— `ASSETS` 加文件 + `CACHE` 版本号 **+1**
3. `tests/sources.js` 的 `MODULES`（ES module 才需要）/ 相关 spec 的 **读取路径**
4. `tools/verify-apk.py` 的 `APP_MODULES` 与**路径断言**

⚠️ **改名 / 搬家最容易漏的是第 3、4 处** —— 它们按**路径**引用文件，
文件一搬就静默失效；`harness` 的 `apk` 关口会把这类问题暴露出来。
