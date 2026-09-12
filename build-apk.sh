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
  for p in "C:/Program Files/Microsoft/jdk-17" "C:/Program Files/Java/jdk-17" \
           "C:/Program Files/Eclipse Adoptium/jdk-17" \
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

# ---------- 同步 + 编译 ----------
echo "→ 同步 www/ 到原生工程…"
./node_modules/.bin/cap sync android

echo "→ 编译 APK…"
cd android
./gradlew assembleDebug --no-daemon --console=plain | tail -5
cd ..

cp android/app/build/outputs/apk/debug/app-debug.apk med-reminder-debug.apk
echo ""
echo "✓ 打包完成：$(pwd)/med-reminder-debug.apk"
echo "  装到手机：python serve-apk.py  然后手机扫码"
