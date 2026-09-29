#!/usr/bin/env bash
# 把 harness 挂到 git 的 pre-commit 上（幂等，可反复跑）。
#
#   bash harness/install-hooks.sh              # 安装（默认：全量 19s，硬闸门）
#   bash harness/install-hooks.sh --fast       # 安装（快速版：只跑语法+验收，约 3s）
#   bash harness/install-hooks.sh --uninstall  # 卸载
#   bash harness/install-hooks.sh --status     # 看当前装的是什么
#
# 为什么用「包装脚本」而不是直接把逻辑写进 .git/hooks/pre-commit：
#   · .git/ 不进版本库，**换台机器/重新克隆就没了** —— 所以要有安装脚本；
#   · 真正的逻辑放在 harness/pre-commit.sh（进版本库、可 review、可改）；
#   · .git/hooks/pre-commit 只是个三行的转发器，随时可由本脚本重建。
set -u

REPO="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [ -z "$REPO" ]; then
  echo "✗ 不在 git 仓库里" >&2
  exit 1
fi
cd "$REPO"

HOOK="$REPO/.git/hooks/pre-commit"
MODE="${1:-}"

case "$MODE" in
  --uninstall)
    if [ -f "$HOOK" ] && grep -q "harness/pre-commit.sh" "$HOOK" 2>/dev/null; then
      rm -f "$HOOK"
      echo "✓ 已卸载 pre-commit hook"
    else
      echo "• 没有装过（或那个 hook 不是 harness 装的）—— 什么都没做"
    fi
    exit 0
    ;;
  --status)
    if [ -f "$HOOK" ] && grep -q "harness/pre-commit.sh" "$HOOK" 2>/dev/null; then
      echo "✓ 已安装。内容："
      echo "---"
      cat "$HOOK"
      echo "---"
    else
      echo "• 未安装"
    fi
    exit 0
    ;;
  --fast) FAST=1 ;;
  "")     FAST=0 ;;
  *)      echo "✗ 未知参数：$MODE（可用 --fast / --uninstall / --status）" >&2; exit 1 ;;
esac

mkdir -p "$REPO/.git/hooks"

if [ "$FAST" = "1" ]; then
  ARGS_LINE='exec bash "$REPO/harness/pre-commit.sh" --only syntax --only apk'
  MODE_DESC="快速版（syntax + apk，约 3 秒）"
else
  ARGS_LINE='exec bash "$REPO/harness/pre-commit.sh"'
  MODE_DESC="全量（16 个关口，约 19 秒）"
fi
cat > "$HOOK" <<HOOKEOF
#!/usr/bin/env bash
# ⚠️ 本文件由 harness/install-hooks.sh 生成 —— 不要手改（改了会被下次安装覆盖）。
#    真正的逻辑在：harness/pre-commit.sh
#    卸载：bash harness/install-hooks.sh --uninstall
REPO="\$(git rev-parse --show-toplevel 2>/dev/null || true)"
[ -z "\$REPO" ] && exit 0
# 保留一个逃生门（有意为之，见 pre-commit.sh 顶部注释）
[ "\${SKIP_HARNESS:-}" = "1" ] && exit 0
$ARGS_LINE
HOOKEOF

chmod +x "$HOOK"
chmod +x "$REPO/harness/pre-commit.sh" 2>/dev/null || true

echo "✓ 已安装 pre-commit hook"
echo "  模式：$MODE_DESC"
echo "  位置：$HOOK"
echo ""
echo "  试试看：随便改一个 www/js 下的文件，然后 git commit —— 红了会被拦下。"
echo "  临时跳过：SKIP_HARNESS=1 git commit ..."
echo "  卸载    ：bash harness/install-hooks.sh --uninstall"
