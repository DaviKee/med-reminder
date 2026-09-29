#!/usr/bin/env bash
# S-0 spike 环境变量 —— **用之前先 source 这个文件**
#
#   source spike/env-harmony.sh
#
# 为什么需要：hionic 的 buildapp / run 要求
#   · DEVECO_SDK_HOME  —— HarmonyOS SDK 路径
#   · DEVECO_IDE_PATH  —— DevEco Studio 路径
#   · hdc / ohpm / hvigor 在 PATH 里
#
# ⚠️ 这些是**当前 shell 生效**的（不写注册表）。
#    理由：改注册表后 WorkBuddy 的 bash 不会立刻看到（要重启应用），
#    而且 spike 是试验性质，不该污染系统环境。
#
# 依据：hionic 官方 README「编译构建hap应用」一节
#       + 本项目实测路径（见 spike/README.md）

DEVECO_ROOT="/d/Program Files/Huawei/DevEco Studio"

if [ ! -d "$DEVECO_ROOT" ]; then
  echo "✗ 找不到 DevEco Studio: $DEVECO_ROOT"
  return 1 2>/dev/null || exit 1
fi

# hionic 读的是 Windows 风格路径
export DEVECO_SDK_HOME="D:/Program Files/Huawei/DevEco Studio/sdk"
export DEVECO_IDE_PATH="D:/Program Files/Huawei/DevEco Studio"

# 命令行工具加进 PATH
export PATH="$PATH:$DEVECO_ROOT/sdk/default/openharmony/toolchains"
export PATH="$PATH:$DEVECO_ROOT/tools/ohpm/bin"
export PATH="$PATH:$DEVECO_ROOT/tools/hvigor/bin"
export PATH="$PATH:$DEVECO_ROOT/tools/node"

# DevEco 自带的 Node（hvigor 需要特定版本；用 IDE 自带的最稳）
export NODE_HOME="$DEVECO_ROOT/tools/node"

echo "── S-0 spike 环境已就绪 ──"
echo "  DEVECO_SDK_HOME = $DEVECO_SDK_HOME"
echo "  DEVECO_IDE_PATH = $DEVECO_IDE_PATH"
printf '  hdc      : '; command -v hdc      || echo '(未找到)'
printf '  ohpm     : '; command -v ohpm     || echo '(未找到)'
printf '  hvigorw  : '; command -v hvigorw  || echo '(未找到)'
printf '  node     : '; node -v 2>/dev/null || echo '(未找到)'
