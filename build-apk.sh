#!/usr/bin/env bash
# 一键出包：同步 Web 资源 → 编译 APK → 复制到项目根目录
# 用法：bash build-apk.sh
set -e

cd "$(dirname "$0")"

# ---------- 找工具链 ----------
# 优先用环境里已有的 JDK / Android SDK（自己装的 Android Studio 也算），
# 其次退回 WorkBuddy 的隔离目录（可能已被清理）。
find_java() {
  # Capacitor 8 的 android 库按 Java 21 编译（compileOptions VERSION_21），
  # JDK 17 会在 javac 阶段报 "invalid target release: 21"。
  # 所以**先找 21**，找不到再退回 17 / JAVA_HOME。
  # （AGP 8.13 + Gradle 8.14.3 在 Java 21 上官方支持，2026-10-05 实测可用。）
  for p in "C:/DevEnv/jdk-21" \
           "C:/Program Files/Microsoft"/jdk-21* \
           "C:/Program Files/Java"/jdk-21* \
           "C:/Program Files/Eclipse Adoptium"/jdk-21*; do
    [ -x "$p/bin/java" ] && { echo "$p"; return; }
  done
  if [ -n "$JAVA_HOME" ] && [ -x "$JAVA_HOME/bin/java" ]; then echo "$JAVA_HOME"; return; fi
  if command -v java >/dev/null 2>&1; then
    local p; p="$(dirname "$(dirname "$(readlink -f "$(command -v java)")")")"
    [ -x "$p/bin/java" ] && { echo "$p"; return; }
  fi
  for p in "C:/Program Files/Microsoft"/jdk-17* \
           "C:/Program Files/Java"/jdk-17* \
           "C:/Program Files/Eclipse Adoptium"/jdk-17* \
           "C:/Program Files/Android/Android Studio/jbr" \
           "$HOME/.workbuddy/binaries/android/jdk17" \
           "C:/DevEnv/jdk-17"; do
    [ -x "$p/bin/java" ] && { echo "$p"; return; }
  done
}

find_sdk() {
  if [ -n "$ANDROID_HOME" ] && [ -d "$ANDROID_HOME/platforms" ]; then echo "$ANDROID_HOME"; return; fi
  if [ -n "$ANDROID_SDK_ROOT" ] && [ -d "$ANDROID_SDK_ROOT/platforms" ]; then echo "$ANDROID_SDK_ROOT"; return; fi
  # 用 $LOCALAPPDATA / $HOME 而不是写死用户名 —— 两台机器都各自解析正确
  for p in "$LOCALAPPDATA/Android/Sdk" "$HOME/AppData/Local/Android/Sdk" \
           "C:/Android" "$HOME/.workbuddy/binaries/android/sdk"; do
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
# ---------- 保留行尾的原地替换 ----------
# ⚠️ 千万不要用 `sed -i` 改这些文件：Git Bash（MSYS）的 sed 在文本模式下
# 会把**整个文件**的 CRLF 转成 LF。2026-09-21 实测确认，后果是每次构建都
# 静默重写 app.js / build.gradle 的行尾 —— 而本项目已经多次被「行尾漂移」
# 坑到（补丁锚点莫名失配，排查很久）。所以这里用纯 bash 实现，逐行重建，
# 先探测原文件行尾再原样写回。
#   用法：set_line <文件> <行首前缀> <替换后的整行>
set_line() {
  _f="$1"; _pat="$2"; _new="$3"
  IFS= read -r _first < "$_f" || true
  case "$_first" in
    *$'\r') _nl=$'\r\n' ;;
    *)       _nl=$'\n'   ;;
  esac
  : > "$_f.tmp"
  while IFS= read -r _line || [ -n "$_line" ]; do
    _line="${_line%$'\r'}"
    if [ "$_line" != "${_line#"$_pat"}" ]; then _line="$_new"; fi
    printf '%s%s' "$_line" "$_nl" >> "$_f.tmp"
  done < "$_f"
  mv "$_f.tmp" "$_f"
}

# 构建日期**一律取系统当天**，并回写进 app.js。
# 以前是读 app.js 里一个手写常量 —— 改版本号时太容易忘记改日期，
# 结果 APK 文件名和 App 内 DEBUG 卡显示的都是上一次的日期（2026-09-21 真的发生过）。
# 回写之后：文件名、App 内显示、原生 versionName 三处必然一致。
BUILDDATE="$(date +%Y-%m-%d)"
set_line www/js/app.js "  var APP_BUILD = " "  var APP_BUILD = '$BUILDDATE';"

# versionCode 由语义版本确定性推导，不用单独维护：
#   v1.0.8 -> 1*10000 + 0*100 + 8 = 10008
MAJ="${VER%%.*}"; REST="${VER#*.}"; MIN="${REST%%.*}"; PAT="${REST##*.}"
VCODE=$(( MAJ * 10000 + MIN * 100 + PAT ))

# ---------- 同步版本到原生工程 ----------
# 这样「设置 → 应用 → 定时服药提醒」里也能看到版本号，便于和 APK 文件对上。
GRADLE="android/app/build.gradle"
if [ -f "$GRADLE" ]; then
  set_line "$GRADLE" "        versionCode " "        versionCode $VCODE"
  set_line "$GRADLE" "        versionName " "        versionName \"$VER\""
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
# ⚠️ 这里**故意用 `cap copy` 而不是 `cap sync`**（踩了 4 次才定下，2026-09-29）。
#
# `cap sync` = `copy` + `update`。它的 `update` 阶段会**删除 ≥50 个构建中间产物**
# 去重建 Cordova 插件的 gradle 配置，于是撞上本机 WorkBuddy 的删除守卫：
#     [safe-delete][SAFE_DELETE_BULK_CONFIRM_REQUIRED] {"count":50,...}
# 而且它是**半途而废**：`copy` 已成功、`update` 只删没建 →
#     android/capacitor-cordova-android-plugins/cordova.variables.gradle 丢失
# → 紧接着 gradlew 报「Could not read script ... cordova.variables.gradle」，
#   **看着像原生工程坏了，根因却在上一命令**（2026-09-24 两次 / 09-26 一次 / 09-29 一次）。
#
# `cap copy` **不走 update 阶段**，因此根本不会碰删除守卫 —— 已连续多次实测 rc=0。
# 代价：不更新 Cordova 插件的 gradle 配置。本项目用的是 **Capacitor 插件**（非 Cordova），
# 那部分配置与本项目无关，影响可忽略。
#
# 若哪天确实需要 `sync`（例如刚加/删了 Capacitor 插件，要重算插件清单）：
#   先把 `android/app/src/main/assets/public` **改名让开**（不删）再重建 ——
#   ⚠️ 但**不要动 `android/capacitor-cordova-android-plugins`**：它是 `cap sync` 的
#   `update` 阶段产物，`cap copy` **不会重建它**，改名让开只会让 gradle 直接报
#   「Could not read script ... cordova.variables.gradle」（2026-09-29 实测踩到）。
echo "→ 同步 www/ 到原生工程…（cap copy，绕开删除守卫）"
./node_modules/.bin/cap copy android

# ---------- 编译 ----------
# ⚠️ 这里必须让 gradlew 的**退出码真实传出来**。
# 曾经写成 `./gradlew ... | tail -5`：管道只反映 tail 的退出码，
# 于是**编译失败也会继续往下走**，最后照样打印「✓ 打包完成」——
# 2026-09-29 就差点交出一个 assets 被删空的残缺包（靠手动核对才发现）。
# 现在：`set -o pipefail` + 显式判 rc，失败即中止（set -e 也会兜底）。
echo "→ 编译 APK…"
cd android
set -o pipefail
./gradlew assembleDebug --no-daemon --console=plain 2>&1 | tail -12
RC=${PIPESTATUS[0]}
set +o pipefail
if [ "$RC" -ne 0 ]; then
  echo ""
  echo "✗ 编译失败（gradlew 退出码 $RC）—— 出包中止，不要交付上面的任何产物。"
  exit 1
fi
cd ..

# 产物带版本号，避免「手里这个包是哪一版」再次搞混。
# 同时清掉上一版产物，保证目录里永远只有一个待安装包（serve-apk.py 取最新那个）。
OUT="MedReminder-v${VER}-${BUILDDATE}.apk"
APKSRC="android/app/build/outputs/apk/debug/app-debug.apk"

# ⚠️ 交付前先确认**产物真的存在**：编译"成功"不等于包在。
# 2026-09-29 曾出现「BUILD SUCCESSFUL 但 assets 被删空」的残缺包 —— 验产物不是验命令。
if [ ! -f "$APKSRC" ]; then
  echo "✗ 没找到编译产物 $APKSRC —— 出包中止。"
  exit 1
fi

rm -f MedReminder-v*.apk med-reminder-debug.apk
cp "$APKSRC" "$OUT"

# 产物级自检：包内 APP_VERSION 必须与本次版本号一致（防止复制到旧包/残缺包）。
PKGVER="$(unzip -p "$OUT" assets/public/js/app.js 2>/dev/null \
          | sed -n "s/^  var APP_VERSION = '\([^']*\)';.*/\1/p" | head -1)"
if [ "$PKGVER" != "$VER" ]; then
  echo "✗ 包内 APP_VERSION='$PKGVER' 与本次 v$VER 不一致 —— 产物可疑，不要交付。"
  exit 1
fi

echo ""
echo "✓ 打包完成：$(pwd)/$OUT"
echo "  版本 v$VER  ·  $BUILDDATE  ·  versionCode $VCODE"
echo "  大小 $(stat -c '%s' "$OUT" 2>/dev/null || echo '?') B · 包内版本已核对 = v$PKGVER"
echo "  装到手机：python serve-apk.py  然后手机扫码"
