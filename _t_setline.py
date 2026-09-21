# -*- coding: utf-8 -*-
"""验证 build-apk.sh 里的 set_line：必须**保留原文件行尾**，只替换目标行。"""
import io, os, subprocess, re, sys

BASE = r'D:\WorkBuddy\MedReminder\med-reminder'
SRC = os.path.join(BASE, 'build-apk.sh')

# ---------- 1) 从 build-apk.sh 抽出 set_line 函数（不执行整个脚本）----------
t = io.open(SRC, encoding='utf-8', newline='').read()
i = t.index('set_line() {')
b = t.index('{', i); d, j = 0, b
while j < len(t):
    if t[j] == '{': d += 1
    elif t[j] == '}':
        d -= 1
        if d == 0: break
    j += 1
FN = t[i:j + 1]
fn_path = os.path.join(BASE, '_setline_fn.sh')
io.open(fn_path, 'w', encoding='utf-8', newline='').write(FN + '\n')
print('已抽出 set_line（%d 行）' % len(FN.splitlines()))

PASS = 0
FAIL = 0


def mk(path, eol):
    body = ["  var APP_VERSION = '1.4.1';",
            "  var APP_BUILD = '2026-09-17';",
            "  var X = 1;"]
    io.open(path, 'w', encoding='utf-8', newline='').write(eol.join(body) + eol)


def kind(path):
    b = open(path, 'rb').read()
    c = b.count(b'\r\n'); lf = b.count(b'\n') - c
    return ('CRLF' if (c and lf == 0) else ('LF' if c == 0 else 'MIXED')), c, lf


def line_of(path, key):
    for l in io.open(path, encoding='utf-8', newline=''):
        if key in l:
            return l.rstrip('\r\n')
    return None


def run_setline(path, prefix, newline_text):
    args = ' '.join('"%s"' % a.replace('"', '\\"') for a in [path, prefix, newline_text])
    cmd = 'source "%s" && set_line %s' % (fn_path.replace('\\', '/'), args)
    r = subprocess.run(['bash', '-c', cmd], capture_output=True, text=True)
    return r


def case(label, eol, prefix, new, want_kind, want_build, note):
    global PASS, FAIL
    p = os.path.join(BASE, '_sl_test.txt')
    mk(p, eol)
    r = run_setline(p, prefix, new)
    k, c, lf = kind(p)
    got_build = line_of(p, 'APP_BUILD')
    got_x = line_of(p, 'var X')
    ok = (k == want_kind and want_build in (got_build or '') and got_x == '  var X = 1;'
          and r.returncode == 0)
    print('  [%s] %s' % ('PASS' if ok else 'FAIL', label))
    print('        行尾 %s (CRLF %d / LF-only %d)  期望 %s' % (k, c, lf, want_kind))
    print('        APP_BUILD = %r' % got_build)
    print('        var X     = %r' % got_x)
    if r.returncode != 0:
        print('        bash 返回码 %d；stderr=%s' % (r.returncode, r.stderr.strip()[:200]))
    if not ok:
        print('        原始内容 repr: %r' % open(p, 'rb').read()[:200])
        print('        说明:', note)
        FAIL += 1
    else:
        PASS += 1


print()
print('=== A. CRLF 文件 → 必须仍是 CRLF ===')
case('CRLF 保留', '\r\n', '  var APP_BUILD = ', "  var APP_BUILD = '2026-09-21';",
     'CRLF', "'2026-09-21'", 'CRLF 被转成了 LF（就是 sed 的老毛病）')

print()
print('=== B. LF 文件 → 必须仍是 LF ===')
case('LF 保留', '\n', '  var APP_BUILD = ', "  var APP_BUILD = '2026-09-21';",
     'LF', "'2026-09-21'", 'LF 被转成了 CRLF')

print()
print('=== C. 前缀不匹配 → 文件必须原样，且行尾不变 ===')
case('不匹配则不动', '\r\n', '  var NOTHERE = ', '  改了',
     'CRLF', "'2026-09-17'", '误改了不该改的行')

# 清理
for f in ['_sl_test.txt', '_setline_fn.sh']:
    fp = os.path.join(BASE, f)
    if os.path.exists(fp):
        os.remove(fp)
print()
print('通过 %d / 失败 %d' % (PASS, FAIL))
sys.exit(1 if FAIL else 0)
