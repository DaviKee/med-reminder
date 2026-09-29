#!/bin/bash
# 自动签名后复测：publishReminder 是否还报 1700002
#
# 前提：模拟器**已在运行**
#   → 请用 DevEco Studio 的 Tools → Device Manager 启动（不要用命令行启动，
#     命令行的模拟器进程会随本 shell 一起结束）
#
# 用法：bash spike/harmony-test/verify-reminder-after-sign.sh
set -u

D="/d/Program Files/Huawei/DevEco Studio"
HDC="$D/sdk/default/openharmony/toolchains/hdc.exe"
S="/c/WorkBuddy/med-reminder/med-reminder/spike/harmony-test"
T=127.0.0.1:5555
PKG=com.medreminder.spike

# ---------- 1. 检查设备 ----------
echo "=== [1/6] 检查模拟器 ==="
if ! "$HDC" list targets 2>/dev/null | grep -q "$T"; then
  echo "✗ 没检测到模拟器（$T）"
  echo ""
  echo "  请先在 DevEco Studio 里启动：Tools → Device Manager → Mate 90 Pro → ▶"
  echo "  （不要用命令行启动 —— 那样模拟器会随 shell 一起被清理）"
  exit 2
fi
echo "✓ 设备在线: $T"

# ---------- 2. 编译 ----------
echo ""
echo "=== [2/6] 编译 ==="
cd "$S" || exit 1
source ../env-harmony.sh >/dev/null 2>&1
cd "$S/openharmony" || exit 1
hvigorw assembleHap --mode module -p product=default -p buildMode=debug --no-daemon 2>&1 \
  | grep -E "BUILD SUCCESSFUL|BUILD FAILED|ERROR|Will skip sign|SignHap" | head -8

# ---------- 3. 找产物 ----------
OUT="$S/openharmony/entry/build/default/outputs/default"
echo ""
echo "=== [3/6] 产物 ==="
ls -la "$OUT"/*.hap 2>/dev/null | awk '{print "  "$NF, $5" bytes"}' || { echo "  ✗ 没有 .hap"; exit 1; }

HAP=""
for cand in entry-default-signed.hap entry-default-unsigned.hap; do
  [ -f "$OUT/$cand" ] && { HAP="$cand"; break; }
done
[ -z "$HAP" ] && { echo "  ✗ 找不到可安装的 hap"; exit 1; }
echo "  将安装: $HAP"

# ---------- 4. 卸载 + 安装 ----------
echo ""
echo "=== [4/6] 卸载旧包并安装 ==="
echo "（签名变了，覆盖安装会失败，所以先卸载）"
"$HDC" -t "$T" uninstall "$PKG" 2>&1 | tail -2

cd "$OUT" || exit 1
"$HDC" -t "$T" install "$HAP" 2>&1 | tail -3

# ---------- 5. 启动 ----------
echo ""
echo "=== [5/6] 清日志 → 启动应用 ==="
"$HDC" -t "$T" shell hilog -r >/dev/null 2>&1
"$HDC" -t "$T" shell aa start -b "$PKG" -a EntryAbility 2>&1 | head -2
echo "  等探针执行（原生侧延迟 8 秒 + 测试耗时）…"
sleep 28

# ---------- 6. 结果 ----------
echo ""
echo "=================== 探针结果 ==================="
"$HDC" -t "$T" shell hilog -x -T SPIKE-REMINDER 2>/dev/null | tail -20
echo "==============================================="
echo ""
echo "看第 1 行："
echo "  ✅  1) publishReminder OK  id=...        → 自动签名生效，开发调试期可用！"
echo "  ❌  1) publishReminder FAIL code=1700002 → 仍需走 AGC 正式申请（预期结果）"
