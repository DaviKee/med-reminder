#!/usr/bin/env bash
# git pre-commit 闸门 —— 提交前自动跑 harness，红了就拦住这次提交。
#
# 为什么要有它
# ------------
# 规矩写在文档里 = 靠人记得跑 = 迟早不跑。把它挂到 git 的 pre-commit 上，
# 「改完不跑」这件事就从"要靠自觉"变成"物理上做不到"。
#
# 用法
# ----
#   安装：bash harness/install-hooks.sh
#   卸载：bash harness/install-hooks.sh --uninstall
#   手动跑一次（不提交）：bash harness/pre-commit.sh
#
# 控制
# ----
#   SKIP_HARNESS=1 git commit ...     临时跳过（有意识地用，不是图快）
#   git commit --no-verify            绕过所有 pre-commit hook（最粗暴）
#
# ⚠️ 为什么不禁用 --no-verify：留一条"逃生门"是有意的。一个**没有逃生门**的闸门
#    会逼人在急的时候去删 hook 文件，那比绕过更糟（改回来很难记得）。
#    有逃生门但默认开着的闸门，才是能长期活下去的闸门。
set -u

# 找到仓库根（hook 是从 .git/hooks 里被调用的，cwd 未必是仓库）
REPO="$(git rev-parse --show-toplevel 2>/dev/null)"
if [ -z "$REPO" ]; then
  echo "✗ 不在 git 仓库里，pre-commit 无法定位仓库根" >&2
  exit 0
fi
cd "$REPO"

HARNESS="$REPO/harness/harness.py"
if [ ! -f "$HARNESS" ]; then
  echo "⚠️  找不到 harness/harness.py —— 跳过自动验证（不阻断提交）"
  echo "   如果这是新克隆的仓库，先跑一次 harness/install-hooks.sh"
  exit 0
fi

# ---- 有意跳过 ----
if [ "${SKIP_HARNESS:-}" = "1" ]; then
  echo ""
  echo "  ⏭  SKIP_HARNESS=1 —— 本次提交跳过了自动验证。"
  echo "     请在 push 之前自己补跑：python harness/harness.py"
  echo ""
  exit 0
fi

# ---- 找 python（绝对路径优先：本机裸调 python 有时会落到商店占位程序）----
PY=""
for c in "$USERPROFILE/.workbuddy/binaries/python/envs/default/Scripts/python.exe" \
         "$USERPROFILE/.workbuddy/binaries/python/versions/3.13.12/python.exe" \
         "$HOME/.workbuddy/binaries/python/versions/3.13.12/python.exe"; do
  if [ -n "${c:-}" ] && [ -x "$c" ]; then PY="$c"; break; fi
done
if [ -z "$PY" ]; then
  if command -v python3 >/dev/null 2>&1; then PY=python3
  elif command -v python  >/dev/null 2>&1; then PY=python
  else
    echo "⚠️  找不到 python —— 跳过自动验证（不阻断提交）"
    echo "   本机 python 绝对路径见 CLAUDE.md / .workbuddy/memory/MEMORY.md §1b"
    exit 0
  fi
fi

echo ""
echo "════════════════════════════════════════════════════════════"
echo "  提交前自动验证（harness）"
echo "════════════════════════════════════════════════════════════"

START=$(date +%s)
"$PY" "$HARNESS" "$@"
RC=$?
ELAPSED=$(( $(date +%s) - START ))

if [ "$RC" -eq 0 ]; then
  echo ""
  echo "  ✅ 验证通过（${ELAPSED}s）—— 继续提交"
  echo ""
  exit 0
fi

echo ""
echo "════════════════════════════════════════════════════════════"
if [ "$RC" -eq 2 ]; then
  echo "  ⚠️  环境问题（退出码 2）—— 未跑完，不是代码的错"
  echo ""
  echo "  这通常意味着 python/node 找不到或跑不起来。"
  echo "  本次提交**已被拦下**，但请先修环境："
  echo "    python harness/harness.py --list     # 看能否列出关口"
else
  echo "  ❌ 验证未通过（退出码 $RC）—— 本次提交已被拦下"
  echo ""
  echo "  怎么查："
  echo "    1) 往上看失败的关口（✗ 或 ‼ 那一行）"
  echo "    2) 看细节：python harness/harness.py --only <关口名> --verbose"
  echo ""
  echo "  确实要先提一版中间状态（少见，请想清楚）："
  echo "    SKIP_HARNESS=1 git commit ...      # 跳过一次，push 前记得补跑"
fi
echo "════════════════════════════════════════════════════════════"
exit 1
