#!/usr/bin/env bash
# 一键出包：同步 Web 资源 → 编译 APK → 复制到项目根目录
# 用法：bash build-apk.sh
set -e

cd "$(dirname "$0")"

# ---------- 找工具链 ----------
# 优先用环境里已有的 JDK / Android SDK（自己装的 Android Studio 也算），
# 其次退回 WorkBuddy 的隔离目录（可能已被清理）。
find_java() {
  if [ -n "$JAVA_HOME" ] && [ -x "$JAVA_HOME/bin/java" ]; then echo "$JAVA_HOME"; return; fi
  if command -v java >/dev/null 2>&1; then
    local p; p="$(dirname "$(dirname "$(readlink -f "$(command -v java)")")")"
    [ -x "$p/bin/java" ] && { echo "$p"; return; }
  fi
  # 用通配匹配带小版本号的目录（如 jdk-17.0.20.101-hotspot）；
  # 未匹配的 glob 会原样保留，[ -x ] 判定失败即可安全跳过。
  for p in "C:/Program Files/Microsoft"/jdk-17* \
           "C:/Program Files/Java"/jdk-17* \
           "C:/Program Files/Eclipse Adoptium"/jdk-17* \
           "C:/Program Files/Android/Android Studio/jbr" \
           "C:/Users/davik/.workbuddy/binaries/android/jdk17"; do
    [ -x "$p/bin/java" ] && { echo "$p"; return; }
  done
}

find_sdk() {
  if [ -n "$ANDROID_HOME" ] && [ -d "$ANDROID_HOME/platforms" ]; then echo "$ANDROID_HOME"; return; fi
  if [ -n "$ANDROID_SDK_ROOT" ] && [ -d "$ANDROID_SDK_ROOT/platforms" ]; then echo "$ANDROID_SDK_ROOT"; return; fi
  for p in "$LOCALAPPDATA/Android/Sdk" "C:/Users/davik/AppData/Local/Android/Sdk" \
           "C:/Android" "C:/Users/davik/.workbuddy/binaries/android/sdk"; do
    [ -d "$p/platforms" ] && { echo "$p"; return; }
  done
}

# 注意：查找函数找不到时会以非 0 返回，必须 || true，否则 set -e 会让脚本静默退出
JH="$(find_java || true)"
AH="$(find_sdk || true)"

if [ -z "$JH" ] || [ -z "$AH" ]; then
  echo "✗ 缺少构建工具链，无法在本机编译 APK。"
  echo ""
  [ -z "$JH" ] && echo "  未找到 JDK 17（JAVA_HOME 未设置，常见路径里也没有）"
  [ -z "$AH" ] && echo "  未找到 Android SDK（ANDROID_HOME 未设置，常见路径里也没有）"
  echo ""
  echo "  两个办法："
  echo "   A. 装 Android Studio（自带 JDK + SDK），然后重跑本脚本 —— 它会自动识别"
  echo "   B. 用 Android Studio 打开 med-reminder/android 目录，"
  echo "      Build → Generate Signed Bundle / APK → APK"
  echo ""
  echo "  只想看改完的效果、不装到手机：浏览器直接打开 www/index.html 即可试交互。"
  exit 1
fi

export JAVA_HOME="$JH"
export ANDROID_HOME="$AH"
export ANDROID_SDK_ROOT="$AH"
export PATH="$JAVA_HOME/bin:$PATH"

echo "  JDK   : $JAVA_HOME"
echo "  SDK   : $ANDROID_HOME"

# ---------- 依赖 ----------
if [ ! -d node_modules ]; then
  echo "→ 安装依赖…"
  npm install --no-audit --no-fund
fi

# ---------- 版本号（单一来源：www/js/app.js 顶部） ----------
# 必须放在编译之前：versionName 要写进 APK，编译后再改就没用了。
VER="$(sed -n "s/^  var APP_VERSION = '\([^']*\)';.*/\1/p" www/js/app.js | head -1)"

if [ -z "$VER" ]; then
  echo "✗ 没能从 www/js/app.js 读到 APP_VERSION —— 出包中止。"
  echo "  版本号是产物命名、Android versionName 以及「手机上装的是哪一版」的唯一依据，不能缺。"
  exit 1
fi
# 构建日期**一律取系统当天**，并回写进 app.js。
# 以前是读 app.js 里一个手写常量 —— 改版本号时太容易忘记改日期，
# 结果 APK 文件名和 App 内 DEBUG 卡显示的都是上一次的日期（2026-09-21 真的发生过）。
# 回写之后：文件名、App 内显示、原生 versionName 三处必然一致。
BUILDDATE="$(date +%Y-%m-%d)"
sed -i.bak "s/^\(  var APP_BUILD = \).*/\1'$BUILDDATE';/" www/js/app.js
rm -f www/js/app.js.bak

# versionCode 由语义版本确定性推导，不用单独维护：
#   v1.0.8 -> 1*10000 + 0*100 + 8 = 10008
MAJ="${VER%%.*}"; REST="${VER#*.}"; MIN="${REST%%.*}"; PAT="${REST##*.}"
VCODE=$(( MAJ * 10000 + MIN * 100 + PAT ))

# ---------- 同步版本到原生工程 ----------
# 这样「设置 → 应用 → 定时服药提醒」里也能看到版本号，便于和 APK 文件对上。
GRADLE="android/app/build.gradle"
if [ -f "$GRADLE" ]; then
  sed -i.bak "s/^\( *versionCode \).*/\1$VCODE/" "$GRADLE"
  sed -i.bak "s/^\( *versionName \).*/\1\"$VER\"/" "$GRADLE"
  rm -f "$GRADLE.bak"
fi

echo "  版本  : v$VER ($BUILDDATE)  versionCode=$VCODE"

# ---------- 同步 Capacitor 插件 JS 到 www/vendor ----------
# 本项目没有打包器，插件必须靠 <script> 引入才会注册进 Capacitor.Plugins。
# 每次都从 node_modules 复制，保证与安装的插件版本一致 ——
# 漏了这段，所有插件调用会静默降级成 web 实现（功能没反应、还不报错）。
mkdir -p www/vendor
cp node_modules/@capacitor/core/dist/capacitor.js                    www/vendor/capacitor.js
cp node_modules/@capacitor/app/dist/plugin.js                        www/vendor/plugin-app.js
cp node_modules/@capacitor/local-notifications/dist/plugin.js         www/vendor/plugin-local-notifications.js
cp node_modules/@capacitor/camera/dist/plugin.js                      www/vendor/plugin-camera.js
cp node_modules/@capacitor/filesystem/dist/plugin.js                  www/vendor/plugin-filesystem.js
echo "→ 已同步 Capacitor 插件 JS 到 www/vendor/"

# ---------- 同步 + 编译 ----------
echo "→ 同步 www/ 到原生工程…"
./node_modules/.bin/cap sync android

echo "→ 编译 APK…"
cd android
./gradlew assembleDebug --no-daemon --console=plain | tail -5
cd ..

# 产物带版本号，避免「手里这个包是哪一版」再次搞混。
# 同时清掉上一版产物，保证目录里永远只有一个待安装包（serve-apk.py 取最新那个）。
OUT="MedReminder-v${VER}-${BUILDDATE}.apk"
rm -f MedReminder-v*.apk med-reminder-debug.apk
cp android/app/build/outputs/apk/debug/app-debug.apk "$OUT"

echo ""
echo "✓ 打包完成：$(pwd)/$OUT"
echo "  版本 v$VER  ·  $BUILDDATE  ·  versionCode $VCODE"
echo "  装到手机：python serve-apk.py  然后手机扫码"
