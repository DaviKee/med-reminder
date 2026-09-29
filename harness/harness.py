#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MedReminder 验证 harness —— 把散在各处的检查串成一道可复现的闸门。

为什么要有它
------------
这个项目的检查本来是**散落**的：
  · `tests/*.spec.js`（13 套，478 条）—— 源码级行为回归
  · `tools/verify-apk.py`（106 项）—— 交付前的内容级验收
  · `.workbuddy/tools/check-module-imports.py` —— 架构铁律（漏 import）
  · 各种一次性命令（出包四项核对、git 身份、记忆体检）—— **只写在文档里，靠人记**

后果很具体：**«改了不跑» 或 «只跑最近动过的那一个»**，回归防护形同虚设。
这个 harness 把它们收进**一个入口、一份报告、一个退出码**，让「跑没跑」不再靠自觉。

用法
----
    python harness/harness.py                 # 跑全部关口
    python harness/harness.py --list          # 只列关口清单
    python harness/harness.py --only spec     # 只跑 id 含 "spec" 的关口
    python harness/harness.py --selftest      # 自检 harness 自身（防谎报全绿）
    python harness/harness.py --json          # 额外输出机器可读结果

退出码
------
    0  全部通过（跳过的不算失败）
    1  有断言失败 / 有 gate 报红
    2  环境问题（缺 python/node、跑不起来、子进程被禁）—— **不代表代码有问题**

★ 两条不可动摇的规矩（本项目用血换来的）
  1. **跳过 ≠ 通过。** 没跑的关口在报告里显式标注，且绝不计入"通过"。
  2. **假绿必须挡住。** spec 报「通过 N / 共 M」时，M>0 且 N==M 才算过 ——
     空集通过（M==0）是失败，不是通过。
"""
import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)                       # med-reminder/
WORKSPACE = os.path.dirname(REPO)                  # C:\WorkBuddy\med-reminder

sys.path.insert(0, HERE)
import gates as gates_mod                          # noqa: E402

# ---------------------------------------------------------------- 环境探测

def find_python():
    """优先用装了库的 venv，退回到裸的解释器（本机 python 会落到商店 stub）。"""
    cands = [
        os.path.join(os.environ.get('USERPROFILE', ''), '.workbuddy', 'binaries',
                     'python', 'envs', 'default', 'Scripts', 'python.exe'),
        os.path.join(os.environ.get('USERPROFILE', ''), '.workbuddy', 'binaries',
                     'python', 'versions', '3.13.12', 'python.exe'),
    ]
    for c in cands:
        if c and os.path.isfile(c):
            return c
    return sys.executable


def find_node():
    """优先用托管 node，退回到系统 node。"""
    cands = [
        os.path.join(os.environ.get('USERPROFILE', ''), '.workbuddy', 'binaries',
                     'node', 'versions', '22.22.2-3', 'node.exe'),
    ]
    for c in cands:
        if c and os.path.isfile(c):
            return c
    for p in os.environ.get('PATH', '').split(os.pathsep):
        exe = os.path.join(p, 'node.exe' if os.name == 'nt' else 'node')
        if os.path.isfile(exe):
            return exe
    return 'node'


def find_apk():
    """取目录里最新的 MedReminder-v*.apk（与 serve-apk.py 同一口径）。"""
    try:
        cands = [f for f in os.listdir(REPO)
                 if f.startswith('MedReminder-v') and f.endswith('.apk')]
    except OSError:
        return None
    if not cands:
        return None
    cands.sort(key=lambda f: os.path.getmtime(os.path.join(REPO, f)), reverse=True)
    return os.path.join(REPO, cands[0])


def build_ctx():
    return {
        'repo': REPO,
        'workspace': WORKSPACE,
        'python': find_python(),
        'node': find_node(),
        'apk': find_apk(),
    }

# ---------------------------------------------------------------- 结果解析

SUMMARY = re.compile(r'通过\s*(\d+)\s*/\s*共\s*(\d+)')


def parse_spec(out):
    """从 spec 输出里抠「通过 N / 共 M」。抠不到返回 None（= 套件自身出错）。"""
    m = SUMMARY.search(out or '')
    return {'pass': int(m.group(1)), 'total': int(m.group(2))} if m else None


def judge(gate, rc, out, err):
    """统一判定 —— 这是全脚本最需要小心的部分（判定错了会谎报全绿）。"""
    if gate['kind'] == 'advisory':
        # 咨询性关口：**永远不报红**，只把输出留给 --verbose 看。
        # 理由见 gates.py 里 imports 关口的注释（工具有已知假阳性）。
        n = len(re.findall(r'⚠️', out or ''))
        return 'ADVISORY', ('%d 处待人工确认' % n) if n else '无提示'

    if gate['kind'] == 'spec':
        s = parse_spec(out)
        if s is None:
            return 'CRASH', '未打印汇总行（rc=%s）—— 视为套件自身出错' % rc
        if s['total'] == 0:
            return 'FAIL', '共 0 条断言 —— 假绿，视为失败'
        if s['pass'] == s['total']:
            return 'PASS', '%d / %d' % (s['pass'], s['total'])
        return 'FAIL', '%d / %d（失败 %d）' % (s['pass'], s['total'], s['total'] - s['pass'])

    # script / selftest：以退出码为准
    if rc == 0:
        return 'PASS', '退出码 0'
    return 'FAIL', '退出码 %s' % rc

# ---------------------------------------------------------------- 执行

def run_gate(gate, ctx):
    cmd = gate['cmd'](ctx) if callable(gate.get('cmd')) else gate.get('cmd')
    cwd = gate['cwd'](ctx) if callable(gate.get('cwd')) else gate.get('cwd', REPO)

    if not cmd:
        return {'state': 'SKIP', 'note': 'gate 未定义命令'}

    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=300)
    except FileNotFoundError as e:
        return {'state': 'ENVERR',
                'note': '命令不存在：%s' % (e.filename or ' '.join(cmd[:2]))}
    except subprocess.TimeoutExpired:
        return {'state': 'FAIL', 'note': '超时 300s'}
    except OSError as e:
        return {'state': 'ENVERR', 'note': '无法启动子进程：%s' % e}

    out = (p.stdout or '') + (p.stderr or '')
    state, note = judge(gate, p.returncode, p.stdout or '', p.stderr or '')
    return {'state': state, 'note': note, 'out': out, 'rc': p.returncode}

# ---------------------------------------------------------------- 自检

def selftest():
    """验证 harness 自己的判定逻辑 —— 汇总算错比用例失败更危险。"""
    cases = [
        # (说明, gate, rc, out, 期望 state)
        ('spec 全绿', {'kind': 'spec'}, 0, '通过 10 / 共 10', 'PASS'),
        ('spec 有失败', {'kind': 'spec'}, 1, '通过 9 / 共 10', 'FAIL'),
        ('spec 假绿：0 条', {'kind': 'spec'}, 0, '通过 0 / 共 0', 'FAIL'),
        ('spec 抠不到汇总', {'kind': 'spec'}, 1, '完全无关的输出', 'CRASH'),
        ('spec 空输出', {'kind': 'spec'}, 1, '', 'CRASH'),
        ('spec 汇总乱序无空格', {'kind': 'spec'}, 0, '通过1/共2', 'FAIL'),
        ('spec 中文空格混杂', {'kind': 'spec'}, 0, '  通过  7  /  共  8  \n', 'FAIL'),
        ('script 退出 0', {'kind': 'script'}, 0, 'ok', 'PASS'),
        ('script 退出 1', {'kind': 'script'}, 1, 'boom', 'FAIL'),
        ('script 退出 2', {'kind': 'script'}, 2, 'boom', 'FAIL'),
        ('advisory 永不报红', {'kind': 'advisory'}, 1, '⚠️ 用到但未声明：quot', 'ADVISORY'),
        ('advisory 无提示也非失败', {'kind': 'advisory'}, 0, '全部干净', 'ADVISORY'),
    ]
    bad = 0
    print('harness 自检：判定逻辑（%d 个用例）' % len(cases))
    for name, g, rc, out, want in cases:
        got, note = judge(g, rc, out, '')
        ok = got == want
        if not ok:
            bad += 1
        print('  %s %-20s → %-6s（期望 %-6s）%s'
              % ('\u2713' if ok else '\u2717', name, got, want, '' if ok else '  ' + note))
    print('\n自检失败 %d 个 \u274c' % bad if bad else '\n自检全部通过 \u2705')
    return 1 if bad else 0

# ---------------------------------------------------------------- 主流程

MARK = {'PASS': '\u2713', 'FAIL': '\u2717', 'SKIP': '\u25cb', 'CRASH': '\u203c',
        'ENVERR': '\u26a0', 'ADVISORY': '\u2139'}


def main():
    ap = argparse.ArgumentParser(description='MedReminder 验证 harness')
    ap.add_argument('--list', action='store_true', help='只列关口清单')
    ap.add_argument('--only', default=None, help='只跑 id 含该子串的关口')
    ap.add_argument('--selftest', action='store_true', help='自检 harness 自身的判定逻辑')
    ap.add_argument('--selftest-e2e', action='store_true',
                    help='端到端自检：注入假失败，确认 harness 真的会报红')
    ap.add_argument('--json', action='store_true', help='额外输出 JSON 结果')
    ap.add_argument('--verbose', action='store_true', help='打印每个关口的完整输出')
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    if args.selftest_e2e:
        import selftest_e2e
        return selftest_e2e.main()

    ctx = build_ctx()
    G = gates_mod.gates(ctx)

    if args.only:
        G = [g for g in G if args.only in g['id']]

    if args.list:
        print('共 %d 个关口：' % len(G))
        for g in G:
            print('  %-22s %s' % (g['id'], g['name']))
            print('      %s' % g['why'])
        return 0

    print('=' * 68)
    print('MedReminder 验证 harness')
    print('=' * 68)
    print('  仓库 : %s' % REPO)
    print('  python: %s' % ctx['python'])
    print('  node  : %s' % ctx['node'])
    print('  APK   : %s' % (os.path.basename(ctx['apk']) if ctx['apk'] else '（无）'))
    print('  关口  : %d 个' % len(G))
    print()

    rows = []
    for g in G:
        # 软跳过（例如没有 APK）—— 显式记 SKIP，绝不计入通过
        if g.get('skip_if') and g['skip_if']():
            note = g.get('skip_why', lambda: '跳过')()
            print('  %s  %-22s SKIP  %s' % (MARK['SKIP'], g['id'], note))
            rows.append({'id': g['id'], 'name': g['name'], 'state': 'SKIP', 'note': note})
            continue

        print('  ▶ %-22s %s' % (g['id'], g['name']))
        r = run_gate(g, ctx)

        # 环境错误：立刻中止（后面只会连锁失败，且不是代码的问题）
        if r['state'] == 'ENVERR':
            print('  %s  %-22s ENVERR  %s' % (MARK['ENVERR'], g['id'], r['note']))
            rows.append({'id': g['id'], 'name': g['name'], 'state': 'ENVERR', 'note': r['note']})
            print('\n环境问题，中止（退出码 2）。这不是代码的问题。')
            if args.json:
                print(json.dumps(rows, ensure_ascii=False, indent=2))
            return 2

        print('  %s  %-22s %-6s %s' % (MARK[r['state']], g['id'], r['state'], r['note']))
        if args.verbose and r.get('out'):
            for line in r['out'].splitlines():
                print('      | ' + line)
        rows.append({'id': g['id'], 'name': g['name'], 'state': r['state'], 'note': r['note']})

    # ---- 汇总 ----
    npass = sum(1 for r in rows if r['state'] == 'PASS')
    nfail = sum(1 for r in rows if r['state'] == 'FAIL')
    ncrash = sum(1 for r in rows if r['state'] == 'CRASH')
    nskip = sum(1 for r in rows if r['state'] == 'SKIP')
    nadv = sum(1 for r in rows if r['state'] == 'ADVISORY')

    print()
    print('=' * 68)
    print('汇总')
    print('=' * 68)
    for r in rows:
        print('  %s  %-22s %-8s %s' % (MARK[r['state']], r['id'], r['state'], r['note']))
    print('  ' + '-' * 60)
    print('  通过 %d ｜ 失败 %d ｜ 出错 %d ｜ 跳过 %d ｜ 咨询 %d ｜ 共 %d 个关口'
          % (npass, nfail, ncrash, nskip, nadv, len(rows)))

    if nadv:
        print()
        print('  \u2139 有 %d 个咨询性关口（不影响退出码，但值得看一眼）：' % nadv)
        for r in rows:
            if r['state'] == 'ADVISORY':
                print('      - %s：%s' % (r['id'], r['note']))
                print('        （用 --only %s --verbose 看细节）' % r['id'])

    if nskip:
        print()
        print('  \u26a0 有 %d 个关口被跳过 —— 跳过不等于通过：' % nskip)
        for r in rows:
            if r['state'] == 'SKIP':
                print('      - %s：%s' % (r['id'], r['note']))

    code = 1 if (nfail or ncrash) else 0
    print()
    print('全部通过 \u2705' if code == 0 else '有失败 \u274c（退出码 %d）' % code)

    if args.json:
        print(json.dumps({'rows': rows, 'pass': npass, 'fail': nfail,
                          'crash': ncrash, 'skip': nskip, 'advisory': nadv,
                          'code': code},
                         ensure_ascii=False, indent=2))
    return code


if __name__ == '__main__':
    sys.exit(main())
