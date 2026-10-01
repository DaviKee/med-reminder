#!/bin/bash
# 为 AGC「代理提醒」**二次申请**重采截图（带演示数据）
#
# 与 capture-for-agc.sh 的区别：
#  ① rawfile/www/index.html 注入了演示数据（阿司匹林=固定时刻 08:00/20:00，二甲双胍=每 8 小时）
#     → 截图不再是"空状态"，能证明"用户已主动设置提醒"
#  ② 会点进药品编辑页，展示"按固定时刻 / 按间隔"两种设置界面
#  ③ 截图自动按用途重命名
#
# ⚠️ 必须整体一个脚本跑完：模拟器进程活在本 shell 进程树里，shell 一结束它就被清理。
# 用法：bash spike/harmony-test/capture-for-agc-v2.sh
set -u

D="/d/Program Files/Huawei/DevEco Studio"
EMU="$D/tools/emulator/Emulator.exe"
HDC="$D/sdk/default/openharmony/toolchains/hdc.exe"
S="/c/WorkBuddy/med-reminder/med-reminder/spike/harmony-test"
WPATH="C:/WorkBuddy/med-reminder/med-reminder/spike/harmony-test/screenshots-for-agc"
TMP="$WPATH/_tmp"
OUT="$S/screenshots-for-agc"
ANALYSIS="$LOCALAPPDATA/Huawei/Emulator/deployed/Mate 90 Pro/uiLayout/analysis.md"
DEV="Mate 90 Pro"
T=127.0.0.1:5555
PKG=com.medreminder.spike

mkdir -p "$OUT" "$TMP"
rm -f "$TMP"/*.png 2>/dev/null

shoot() {  # $1 = 输出名（不含 .png）
  ( cd "$D/tools/emulator" && "$EMU" -instance "$DEV" -screenshot -screenshotPath "$WPATH" >/dev/null 2>&1 )
  sleep 2
  latest=$(ls -t "$TMP"/*.png 2>/dev/null | head -1)
  if [ -n "$latest" ]; then
    mv -f "$latest" "$OUT/$1.png"
    echo "    📷 已保存 $1.png"
  else
    echo "    ✗ 截图失败（$1）"
  fi
}

tap() {  # $1 = 界面文字
  ( cd "$D/tools/emulator" && "$EMU" -instance "$DEV" -uiLayout >/dev/null 2>&1 )
  local id
  id=$(grep -aoE "$1 \[id:[0-9]+\]" "$ANALYSIS" 2>/dev/null | grep -oE "[0-9]+" | head -1)
  if [ -n "$id" ]; then
    echo "    点击「$1」id=$id"
    ( cd "$D/tools/emulator" && "$EMU" -instance "$DEV" -click "$id" >/dev/null 2>&1 )
    sleep 3
  else
    echo "    ⚠ 未找到「$1」，跳过"
  fi
}

restart_app() {
  "$HDC" -t "$T" shell aa force-stop "$PKG" >/dev/null 2>&1
  sleep 2
  "$HDC" -t "$T" shell aa start -b "$PKG" -a EntryAbility >/dev/null 2>&1
  sleep 15
}

echo "=========== AGC 二次申请截图采集 ==========="

echo "[1/8] 启动模拟器"
# ⚠️ 不要用 `( cd ... && nohup ... & )` 子 shell 写法 ——
#    子 shell 立即退出会把后台的模拟器进程一起带走（实测：进程消失、hdc 永远 [Empty]）。
#    必须在**当前 shell** 里 nohup 起来，且本脚本保持在同一个 shell 里等到截图做完。
cd "$D/tools/emulator" || exit 1
"$EMU" -license accept >/dev/null 2>&1
nohup "$EMU" -start "$DEV" >/tmp/emu-v2.log 2>&1 &
echo "    启动器 PID $!"

echo "[2/8] 等待 hdc 就绪（最多 900s）"
# ⚠️ 为什么要等这么久（2026-10-01 实测）：模拟器**快照启动超时**会回退冷启动，
#    冷启动要逐个校验镜像签名 —— system.img 一次就 ~59s、sys_prod.img ~14s，
#    加上内核启动，总计 3~6 分钟。之前只等 240/360s，**在它就绪前脚本就退出，
#    shell 一结束模拟器进程就被带走**（日志里能看到"刚校验完 system.img 就被 Kill"）。
i=0; ok=""
while [ "$i" -lt 900 ]; do
  ok=$("$HDC" list targets 2>/dev/null | grep -v '^\[Empty\]' | head -1)
  [ -n "$ok" ] && break
  sleep 6; i=$((i+6))
done
if [ -z "$ok" ]; then echo "    ✗ 900s 未就绪，看 /tmp/emu-v2.log"; exit 1; fi
echo "    ✓ 设备 $ok"

echo "[3/8] 安装 HAP"
cd "$S/openharmony/entry/build/default/outputs/default" || exit 1
"$HDC" -t "$T" install -r entry-default-signed.hap 2>&1 | tail -2

echo "[4/8] 启动应用"
restart_app

echo "[5/8] 今日页（应显示 08:00 / 20:00 待服）"
shoot "今日提醒排程"

echo "[6/8] 药品列表"
tap "药品"
shoot "我的药品-提醒规则"

echo "[7/8] 编辑药品①（按固定时刻）"
tap "阿司匹林"
shoot "设置提醒-每日固定时刻"

echo "[8/8] 编辑药品②（按间隔倒计时）"
restart_app
tap "药品"
tap "二甲双胍"
shoot "设置提醒-按间隔倒计时"

echo "=========== 完成 ==========="
ls -la "$OUT"/*.png 2>/dev/null | tail -8
