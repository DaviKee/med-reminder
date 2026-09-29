#!/usr/bin/env bash
# 启动鸿蒙模拟器，并等 hdc 就绪
#
# 用法：bash spike/start-emulator.sh [设备名] [等待秒数]
#   设备名默认 "Mate 90 Pro"；用 Emulator.exe -list 看有哪些
#
# ⚠️⚠️ 两个必踩的坑（2026-09-29 现场踩出来的，都写进注释了）：
#
#   坑 1 —— **不要传 -instancePath / -imageRoot！**
#     传了会 794ms 就退出，报 "Unable to start the emulator"，
#     **而且连日志都不写** —— 看着像环境问题，其实是参数问题。
#     模拟器自己从 deployed/lists.json 读得到路径。**只传名字即可**。
#
#   坑 2 —— **必须先 `-license accept`！**
#     否则模拟器能启动、能显示开机动画，但 **12 秒后自己 quit**
#     （日志末尾是 `StopMultiScreen before quit` / `quit emulator`），
#     而且 hdc 永远等不到设备。这是最误导人的一个：
#     `.emu_config` 里 `HarmonyOS_SDK_Agreement: disagree` 就是元凶。
#     接受后一次就起来。
#
#   坑 3（环境相关）—— **模拟器进程要活在我们的 shell 里**。
#     沙箱会清理"脱离进程树"的子进程（DETACHED_PROCESS 启动的会被杀掉，
#     连日志都来不及写）。所以本脚本用后台运行 + 保持 shell 存活。
#     若要长期使用，建议在 **DevEco Studio 的 Device Manager** 里启动。
#
# 成功标志：hdc list targets 出现 127.0.0.1:5555
set -u

D="/d/Program Files/Huawei/DevEco Studio"
EMU="$D/tools/emulator/Emulator.exe"
HDC="$D/sdk/default/openharmony/toolchains/hdc.exe"
NAME="${1:-Mate 90 Pro}"
WAIT="${2:-240}"

if [ ! -x "$EMU" ]; then
  echo "✗ 找不到模拟器: $EMU"
  echo "  先跑 bash spike/env-check.sh 看环境"
  exit 2
fi

# 已经在跑就不重复启动
running=$("$HDC" list targets 2>/dev/null | grep -v "^\[Empty\]" | head -1)
if [ -n "$running" ]; then
  echo "✓ 设备已在运行: $running（不重复启动）"
  exit 0
fi

echo "步骤 0/2：接受模拟器许可协议（不做这一步会启动后又自己退出）"
"$EMU" -license accept 2>&1 | head -3
echo ""

echo "步骤 1/2：启动模拟器 $NAME"
"$EMU" -start "$NAME" > "/tmp/emu-start.log" 2>&1 &
EMU_PID=$!
echo "  PID $EMU_PID，输出写往 /tmp/emu-start.log"

echo "步骤 2/2：等待 hdc 就绪（最多 ${WAIT}s，冷启动通常 1-3 分钟）..."
i=0
while [ "$i" -lt "$WAIT" ]; do
  out=$("$HDC" list targets 2>/dev/null | grep -v "^\[Empty\]" | head -1)
  if [ -n "$out" ]; then
    echo ""
    echo "✓ 设备已就绪: $out"
    echo ""
    echo "--- 后续可直接用的命令 ---"
    echo "  截图: $EMU -instance \"$NAME\" -screenshot -screenshotPath \"<目录>\""
    echo "  点击: $EMU -instance \"$NAME\" -click <x> <y>"
    echo "  滑动: $EMU -instance \"$NAME\" -slide \"<x1> <y1> <x2> <y2>\""
    echo "  装包: $HDC -t $out install <xxx.hap>"
    exit 0
  fi
  sleep 6; i=$((i+6))
  if [ $((i % 30)) -eq 0 ]; then echo "  已等 ${i}s..."; fi
done

echo ""
echo "✗ ${WAIT}s 内没等到设备。"
echo ""
echo "--- 启动输出 ---"
cat "/tmp/emu-start.log" 2>/dev/null
echo ""
echo "--- 实例日志（最后 20 行）---"
tail -20 "$LOCALAPPDATA/Huawei/Emulator/deployed/$NAME/Log/Emulator.log" 2>/dev/null
echo ""
echo "--- 排查 ---"
echo "  1. 若日志末尾是 'quit emulator' → 协议没接受，重跑本脚本（它会先 accept）"
echo "  2. 若报 HYPER_V_ERROR / 00801001 → 以管理员启用 Hyper-V + 虚拟机平台，然后重启"
echo "  3. 最省事的替代：直接在 DevEco Studio 的 Device Manager 里点启动"
exit 1
