#!/usr/bin/env bash
# S-0 鸿蒙 spike —— 环境就绪自检
#
# 目的：把「装完 DevEco 之后到底还缺什么」变成一条命令，而不是靠猜。
# 只读，不改任何东西。退出码：0 = 全就绪 ｜ 1 = 有缺失 ｜ 2 = 关键项缺失
#
# 用法：
#   bash spike/env-check.sh
#
# 依据：MedReminder-后续任务计划.md §24（S-0 执行清单）
set -u

RED=$'\033[31m'; GRN=$'\033[32m'; YEL=$'\033[33m'; DIM=$'\033[2m'; RST=$'\033[0m'

ok=0; miss=0; crit=0
say_ok()   { printf '  %s✓%s %s\n'      "$GRN" "$RST" "$1"; ok=$((ok+1)); }
say_miss() { printf '  %s✗%s %s\n'      "$RED" "$RST" "$1"; miss=$((miss+1)); }
say_crit() { printf '  %s✗✗%s %s\n'     "$RED" "$RST" "$1"; crit=$((crit+1)); }
say_note() { printf '     %s%s%s\n'    "$DIM" "$1" "$RST"; }
say_warn() { printf '  %s⚠%s %s\n'      "$YEL" "$RST" "$1"; }

echo "=============================================="
echo " S-0 鸿蒙 spike · 环境就绪自检"
echo "=============================================="

# ---------- 1. 关键项：DevEco Studio 安装 ----------
echo ""
echo "[1] DevEco Studio（唯一硬前置）"

DEVECO_DIRS=(
  "/d/Program Files/Huawei/DevEco Studio"
  "/c/Program Files/Huawei/DevEco Studio"
  "/c/DevEnv/DevEco Studio"
  "$LOCALAPPDATA/Programs/DevEco Studio"
)
found_deveco=""
for d in "${DEVECO_DIRS[@]}"; do
  if [ -d "$d" ]; then found_deveco="$d"; break; fi
done
# 兜底：找 DevEco 的典型标志文件（devecostudio64.exe / product-info.json）
if [ -z "$found_deveco" ]; then
  for root in /c /d; do
    hit=$(ls -d "$root"/Program\ Files/Huawei/DevEco\ Studio "$root"/DevEco* "$root"/Huawei/DevEco* 2>/dev/null | head -1)
    [ -n "$hit" ] && { found_deveco="$hit"; break; }
  done
fi

if [ -n "$found_deveco" ]; then
  say_ok "DevEco Studio: $found_deveco"
else
  say_crit "未找到 DevEco Studio"
  say_note "需登录华为开发者账号，从 developer.huawei.com/consumer/cn/download 下载"
  say_note "建议版本 6.0.0 Release（Build 6.0.0.858）"
  say_note "⚠️ 安装路径必须全英文、无空格"
fi

# ---------- 2. 关键项：HarmonyOS SDK ----------
echo ""
echo "[2] HarmonyOS SDK"

SDK_ROOTS=(
  "$LOCALAPPDATA/Huawei/Sdk"
  "$HOME/AppData/Local/Huawei/Sdk"
  "/c/DevEnv/HarmonyOS/Sdk"
  "/d/Huawei/Sdk"
)
found_sdk=""
for d in "${SDK_ROOTS[@]}"; do
  if [ -d "$d" ]; then found_sdk="$d"; break; fi
done

# ★ 新版 DevEco 把 SDK **内嵌**在安装目录的 sdk/default/ 下（本项目实测如此），
#   没有独立的 %LOCALAPPDATA%\Huawei\Sdk。必须认这种情况。
embedded_sdk=""
if [ -n "$found_deveco" ] && [ -d "$found_deveco/sdk" ]; then
  embedded_sdk="$found_deveco/sdk"
fi

if [ -n "$embedded_sdk" ]; then
  say_ok "SDK（内嵌于 DevEco）: $embedded_sdk"
  # 读 sdk-pkg.json 拿 API 版本
  pkg=$(find "$embedded_sdk" -maxdepth 2 -name "sdk-pkg.json" 2>/dev/null | head -1)
  if [ -n "$pkg" ]; then
    api=$(grep -o '"apiVersion"[^,]*' "$pkg" 2>/dev/null | head -1)
    name=$(grep -o '"displayName"[^,]*' "$pkg" 2>/dev/null | head -1)
    rel=$(grep -o '"releaseType"[^,]*' "$pkg" 2>/dev/null | head -1)
    say_note "$api ｜ $name ｜ $rel"
  fi
  found_sdk="$embedded_sdk"
  hdc=$(find "$found_sdk" -name "hdc.exe" -o -name "hdc" 2>/dev/null | head -1)
  if [ -n "$hdc" ]; then say_ok "hdc: $hdc"; else say_warn "SDK 里没找到 hdc"; fi
elif [ -n "$found_sdk" ]; then
  say_ok "SDK 根目录: $found_sdk"
  for sub in "$found_sdk"/*/; do
    [ -d "$sub" ] && say_note "发现: $(basename "$sub")"
  done
  hdc=$(find "$found_sdk" -name "hdc.exe" -o -name "hdc" 2>/dev/null | head -1)
  if [ -n "$hdc" ]; then say_ok "hdc: $hdc"
  else say_warn "SDK 里没找到 hdc（可能在 toolchains/ 未下载）"; fi
else
  say_crit "未找到 HarmonyOS SDK"
  say_note "新版 DevEco 内置 SDK（<安装目录>/sdk/）；旧版首次启动会引导下载"
  say_note "SDK 路径同样不能有中文"
fi

# ---------- 3. 命令行工具：ohpm / hvigorw ----------
echo ""
echo "[3] 命令行构建工具"

find_tool() {  # $1 = 可执行名, $2 = 备用名（可选，如 .bat）
  # 先看 PATH
  if command -v "$1" >/dev/null 2>&1; then command -v "$1"; return; fi
  # 再从 DevEco / SDK 目录里找
  # ⚠️ 用 \( ... \) 分组：否则 find 的 -o 优先级会让 -maxdepth 只作用于第一个 -name
  for base in "$found_deveco" "$found_sdk" /c/DevEnv; do
    [ -z "$base" ] && continue
    if [ -n "${2:-}" ]; then
      hit=$(find "$base" -maxdepth 6 \( -name "$1" -o -name "$2" \) 2>/dev/null | head -1)
    else
      hit=$(find "$base" -maxdepth 6 \( -name "$1" -o -name "$1.exe" \) 2>/dev/null | head -1)
    fi
    [ -n "$hit" ] && { echo "$hit"; return; }
  done
  echo ""
}

OHPM=$(find_tool ohpm)
if [ -n "$OHPM" ]; then say_ok "ohpm: $OHPM"
else say_miss "ohpm 未找到（鸿蒙的 npm，随 DevEco 提供）"; fi

HVIGOR=$(find_tool hvigorw)
if [ -n "$HVIGOR" ]; then say_ok "hvigorw: $HVIGOR"
else say_miss "hvigorw 未找到（鸿蒙的 gradle，随 DevEco 提供）"; fi

HDC=$(find_tool hdc)
if [ -n "$HDC" ]; then say_ok "hdc: $HDC"
else say_miss "hdc 未找到（鸿蒙的 adb，在 SDK toolchains/）"; fi

# ---------- 4. 关键项：真机连接 ----------
echo ""
echo "[4] 真机连接（spike 必须真机，模拟器不算）"
if [ -n "$HDC" ]; then
  targets=$("$HDC" list targets 2>/dev/null | grep -v "^\[Empty\]" | head -5)
  if [ -n "$targets" ]; then
    say_ok "已连接设备:"; echo "$targets" | sed 's/^/       /'
  else
    say_warn "未发现设备"
    say_note "鸿蒙手机：设置 → 系统 → 开发者选项 → 打开「USB 调试」"
  fi
else
  say_note "（hdc 未就绪，跳过）"
fi

# ---------- 5. 已有工具链（本项目其他前置，好消息是都有了） ----------
echo ""
echo "[5] 本项目其他前置（Android 线已在用，应当都有）"

if [ -x "C:/DevEnv/node/node.exe" ] || command -v node >/dev/null 2>&1; then
  v=$(node --version 2>/dev/null || C:/DevEnv/node/node.exe --version 2>/dev/null)
  say_ok "Node.js: $v"
else
  say_miss "Node.js 未找到"
fi

if [ -d "/c/DevEnv/jdk-17" ]; then
  say_ok "JDK 17: C:\\DevEnv\\jdk-17"
  say_note "（hvigor / hap-sign-tool 要求 JDK 11+，满足）"
else
  say_miss "JDK 17 未找到（C:\\DevEnv\\jdk-17）"
fi

# 内存：DevEco 要求 16GB
# ⚠️ 用 awk 做四舍五入。bash 整数除法会把 16568616 kB（≈15.80 GiB）截成 15，
#    从而误报「低于 16GB」—— 华为的 16GB 指的是 16 GiB 标称值，这台机器刚好够。
mem_kb=$(grep -i MemTotal /proc/meminfo 2>/dev/null | awk '{print $2}')
if [ -n "$mem_kb" ]; then
  mem_gb=$(awk -v k="$mem_kb" 'BEGIN{printf "%d", k/1024/1024+0.5}')
  mem_gb1=$(awk -v k="$mem_kb" 'BEGIN{printf "%.1f", k/1024/1024}')
  if [ "$mem_gb" -ge 16 ]; then
    say_ok "内存: ${mem_gb1} GB（DevEco 要求 16GB）"
    say_warn "刚好卡在最低线，IDE + 模拟器可能吃紧（建议关闭模拟器、只用真机）"
  else
    say_miss "内存: ${mem_gb1} GB —— 低于 DevEco 要求的 16GB"
  fi
fi

# ---------- 6. 磁盘 ----------
echo ""
echo "[6] 磁盘空间（DevEco + SDK + 模拟器镜像，官方要求 100GB）"
avail_c=$(df -h / 2>/dev/null | awk 'NR==2{print $4}')
say_note "C 盘可用: ${avail_c:-未知}"

# ---------- 7. npm 侧：鸿蒙包是否可达（本会话已核实存在） ----------
echo ""
echo "[7] 鸿蒙 Capacitor 包（npm 侧，本轮已核实存在）"
for p in ohos app camera filesystem local-notifications; do
  printf '     %s\n' "@capacitor-ohos/$p"
done
say_note "实测版本: ohos/app/camera/filesystem = 8.0.2, local-notifications = 8.0.1"
say_note "⚠️ 它们 peerDependencies 要求 @capacitor/core ^8，本项目现为 6.2.2 → 需先升级"

# ---------- 汇总 ----------
echo ""
echo "=============================================="
if [ "$crit" -gt 0 ]; then
  printf ' %s关键项缺失 %d 项，其余缺失 %d 项 —— 环境未就绪%s\n' "$RED" "$crit" "$miss" "$RST"
  echo "=============================================="
  exit 2
elif [ "$miss" -gt 0 ]; then
  printf ' %s缺失 %d 项（非关键）—— 部分就绪%s\n' "$YEL" "$miss" "$RST"
  echo "=============================================="
  exit 1
else
  printf ' %s环境就绪（%d 项通过）—— 可以开始 spike%s\n' "$GRN" "$ok" "$RST"
  echo "=============================================="
  exit 0
fi
