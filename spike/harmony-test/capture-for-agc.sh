#!/bin/bash
# 为 AGC「代理提醒」开放能力申请采集界面截图
#
# ⚠️ 为什么必须整个流程放在一个脚本里：
#    模拟器进程活在本 shell 的进程树中，**shell 一结束模拟器就被清理**。
#    所以「启动 → 等就绪 → 装包 → 启动应用 → 逐页截图」必须一气呵成。
#    （另一个已知限制：后台任务约 2h20m 会被回收，届时模拟器也会一起走。）
#
# 用法：bash spike/harmony-test/capture-for-agc.sh
set -u

D="/d/Program Files/Huawei/DevEco Studio"
EMU="$D/tools/emulator/Emulator.exe"
HDC="$D/sdk/default/openharmony/toolchains/hdc.exe"
S="/c/WorkBuddy/med-reminder/med-reminder/spike/harmony-test"
WPATH="C:/WorkBuddy/med-reminder/med-reminder/spike/harmony-test/screenshots-for-agc"
T=127.0.0.1:5555
ANALYSIS="$LOCALAPPDATA/Huawei/Emulator/deployed/Mate 90 Pro/uiLayout/analysis.md"
DEV="Mate 90 Pro"

mkdir -p "$S/screenshots-for-agc"

shot() {  # $1 = 可读的用途名（只用于日志）
  cd "$D/tools/emulator" || exit 1
  "$EMU" -instance "$DEV" -screenshot -screenshotPath "$WPATH" >/dev/null 2>&1
  sleep 2
  echo "    📷 已截图（$1）"
}

uid() {  # $1 = 界面文字 → 返回元素 id
  cd "$D/tools/emulator" || exit 1
  "$EMU" -instance "$DEV" -uiLayout >/dev/null 2>&1
  grep -aoE "$1 \[id:[0-9]+\]" "$ANALYSIS" 2>/dev/null | grep -oE "[0-9]+" | head -1
}

echo "=============== AGC 申请截图采集 ==============="

echo "[1/6] 接受协议 + 启动模拟器"
cd "$D/tools/emulator" || exit 1
"$EMU" -license accept >/dev/null 2>&1
nohup "$EMU" -start "$DEV" >/tmp/emu-capture.log 2>&1 &
echo "    启动器 PID $!"

echo "[2/6] 等待 hdc 就绪（最多 240s）"
i=0; ok=""
while [ "$i" -lt 240 ]; do
  ok=$("$HDC" list targets 2>/dev/null | grep -v '^\[Empty\]' | head -1)
  [ -n "$ok" ] && break
  sleep 6; i=$((i+6))
done
if [ -z "$ok" ]; then
  echo "    ✗ 240s 内未就绪 —— 看 /tmp/emu-capture.log"
  exit 1
fi
echo "    ✓ 设备: $ok"

echo "[3/6] 安装 HAP（含主项目真实界面）"
cd "$S/openharmony/entry/build/default/outputs/default" || exit 1
"$HDC" -t "$T" install -r entry-default-unsigned.hap 2>&1 | tail -2

echo "[4/6] 启动应用"
"$HDC" -t "$T" shell aa force-stop com.medreminder.spike >/dev/null 2>&1
sleep 2
"$HDC" -t "$T" shell aa start -b com.medreminder.spike -a EntryAbility 2>&1 | head -1
sleep 14

echo "[5/6] 逐页截图"
shot "今日（首页）"

# ⚠️ 顺序很重要：先点「记录」，最后点「药品」。
#    因为「药品」页在药品列表为空时会**自动弹出「添加药品」表单**，会挡住后续操作。
for tab in 记录 药品; do
  tid=$(uid "$tab")
  if [ -n "$tid" ]; then
    echo "    点击「$tab」id=$tid"
    cd "$D/tools/emulator" || exit 1
    "$EMU" -instance "$DEV" -click "$tid" >/dev/null 2>&1
    sleep 3
    shot "$tab"
  else
    echo "    ⚠ 未找到「$tab」的 id，跳过"
  fi
done

echo "[6/6] 完成"
ls -la "$S/screenshots-for-agc/" 2>/dev/null | tail -8
echo "=============== 结束 ==============="
