# 定时服药提醒

从设计稿 1:1 落地的可运行应用。打卡制：每天第一次服药打卡后，按药品自定义间隔自动排程当天剩余提醒，**息屏 / 锁屏 / App 没打开都会响**。

## 目录结构

```
www/                     Web 应用（也是原生壳里的界面）
  index.html             三个页签 + 药品面板 + 两个弹窗
  js/app.js              排程、打卡、跳过、提醒、统计
  js/notify.js           通知层：原生走系统闹钟，浏览器降级为页面内提醒
  css/app.css            设计令牌（#0A0A0A / #191919 / #212327 / #FF7A17）
android/                 已生成的 Android 原生工程（Android Studio 直接打开）
capacitor.config.json    webDir = www
```

## 后台提醒是怎么做到的

浏览器（PWA）没有系统闹钟能力，页面一关就收不到。所以走 Capacitor 壳：

- **系统级闹钟**：`LocalNotifications` 用 `AlarmManager.setExactAndAllowWhileIdle` 登记每条待服剂次，
  息屏、Doze 省电模式、App 被划掉都能按时响
- **锁屏直接操作**：通知上带「已服用」「10 分钟后」两个按钮，不用解锁进 App
- **重启不丢**：插件自带 `LocalNotificationRestoreReceiver`，监听 `BOOT_COMPLETED` 自动恢复闹钟
- **精确闹钟权限**：`AndroidManifest.xml` 同时声明 `USE_EXACT_ALARM`（安装即授予）与
  `SCHEDULE_EXACT_ALARM`。Android 14 起后者默认不授予，只声明它会导致提醒被系统降级为
  非精确闹钟、延迟好几分钟，所以以 `USE_EXACT_ALARM` 兜底（自用分发）。
  若将来上架 Google Play，需移除 `USE_EXACT_ALARM` 并改为引导用户开启系统「闹钟和提醒」权限。
- **通知小图标**：`res/drawable/ic_stat_notify.xml`（纯白胶囊剪影，符合 Android 规范）
- **应用图标**：橙色胶囊（`#FF7A17`）配近黑底（`#0A0A0A`），自适应 / 方形 / 圆形三种规格齐全，
  启动画面同为黑底胶囊，避免暗色界面启动时闪白

任何状态变化（打卡 / 提前服药 / 跳过 / 顺延 / 改间隔 / 跨天）都会先撤销旧通知再重新登记，不会重复响。

## 打包 APK

> **本机目前没有构建工具链**（原先装在 `~/.workbuddy/binaries/android` 的 JDK 17 + Android SDK 34
> 已按你的要求清理，释放约 724M）。需要重新出包时，先装 **Android Studio**（自带 JDK 和 SDK），
> 或者自己装 JDK 17 + 命令行 SDK 并设好 `JAVA_HOME` / `ANDROID_HOME`。

一条命令出包（脚本会自动找 JDK 和 SDK，找不到会给出明确指引而不是报错）：

```bash
bash build-apk.sh        # → med-reminder/med-reminder-debug.apk
```

它会自动识别这些位置，装好 Android Studio 一般不用额外配置：

- `JAVA_HOME`，否则常见路径（`C:/Program Files/Microsoft/jdk-17`、
  `C:/Program Files/Eclipse Adoptium/jdk-17`、Android Studio 自带的 `jbr`）
- `ANDROID_HOME` / `ANDROID_SDK_ROOT`，否则 `%LOCALAPPDATA%/Android/Sdk`（Android Studio 默认位置）

手动打包也一样：

```bash
npx cap sync android
cd android && ./gradlew assembleDebug
```

产物在 `android/app/build/outputs/apk/debug/app-debug.apk`。

只改了 `www/` 里的界面、想先看效果不想打包：浏览器直接打开 `www/index.html` 即可试交互。

> 产物有两份：`android/app/build/outputs/apk/debug/app-debug.apk`（Gradle 原始输出）
> 和 `med-reminder/med-reminder-debug.apk`（脚本复制出来的，方便直接发给手机）。

## 装到手机

**方式一：扫码直装（最快，不用数据线）**

手机和电脑连**同一个 WiFi**，电脑上起一个临时下载服务：

```bash
python serve-apk.py          # 默认 8080 端口，可加参数改端口
```

终端会打印本机局域网地址，同时项目里的 `install.html` 有二维码——手机扫一下打开下载页，
点「下载安装包」即可。装完 Ctrl+C 关掉服务。

> 微信内置浏览器会拦截 APK，页面会提示改用系统浏览器打开。
> 服务只绑定本机局域网网卡，用完就关，别长期开着。

**方式二：数据线**

1. 手机 → 设置 → 关于手机 → 连点「版本号」7 次，开启开发者模式
2. 设置 → 系统 → 开发者选项 → 打开「USB 调试」
3. 插上数据线，手机弹窗选「允许 USB 调试」
4. 电脑执行：

```bash
# adb 来自 Android 平台工具，Android Studio 装好后一般在 %LOCALAPPDATA%/Android/Sdk/platform-tools/
adb devices                                    # 确认已识别
adb install -r "med-reminder-debug.apk"
```

**方式三：直接传文件**

把 APK 发到微信 / QQ / 网盘，手机上点开安装。系统会提示「未知来源应用」，允许即可
（Debug 包由调试证书签名，这个提示是正常的）。

**装好后必做**：第一次打开 App 会请求通知权限，**必须允许**，否则锁屏和息屏时收不到提醒。
若误点了拒绝，去 设置 → 应用 → 定时服药提醒 → 通知，手动打开。

## 真机上要注意（重要）

国产 ROM 的省电策略会杀后台闹钟。第一次装完建议手动加白名单，否则可能延迟几分钟才响：

- 设置 → 应用 → 定时服药提醒 → **电池**：选「无限制 / 不优化」
- 设置 → 应用 → **自启动 / 后台运行**：打开
- 小米/华为/OPPO/vivo 各自还有「锁屏后清理内存」「神隐模式」之类的开关，一并关掉

## 也能当网页用

```bash
cd www && python -m http.server 5173
```

浏览器端没有系统闹钟，降级为「页面开着时的弹窗 + Web Notification」，适合先在电脑上试交互。

## 产品逻辑

- 药品 = 名称 + 服药间隔（1–24 小时）
- 某药当天首次打卡时刻 `T` → 排程 `T, T+间隔, T+2×间隔 …`，**不跨天**（超过 24:00 的不再排）
- 「提前服药」= 把当前待服剂次直接记为已服用
- 「跳过本次」= 记为未服用，进入依从率分母，**后续提醒时间不变**
- 「10 分钟后提醒」= 该剂次顺延 10 分钟，其余不变
- 改间隔时，已打的卡保留，当天未服的剂次按新间隔重排
- 支持逐药打卡：点药品卡片只打卡某一种

数据存 `localStorage`（原生壳里是 WebView 的 localStorage），换设备不同步。
