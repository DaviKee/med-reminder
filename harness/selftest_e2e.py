#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""harness 的**端到端**自检 —— 证明它真的会报红，而不是只会打印绿色。

为什么不能只做单元级自检
------------------------
`--selftest` 只验证了「judge() 这个纯函数算得对」。但真正危险的失败模式是：
harness **跑不起来子进程**、**读错路径**、**把退出码吞了** —— 这些都不是 judge() 的问题。
必须真跑一次、真注入一个失败、真看到它报红。

做法（三步，全程可回退）：
  1. 备份 tests/ 下任意一个 spec；
  2. 往它里面注入一条**必定失败**的断言（`t('注入的失败', false, 'boom')`）；
  3. 跑 harness，要求：退出码 != 0 且 该 spec 判 FAIL；
  4. 无论成败，**finally 里恢复原文件**（逐字节校验恢复成功）。

退出码：0 = harness 行为符合预期（确实报红了）；1 = harness 漏报（谎报全绿）；2 = 环境问题。

用法：python harness/selftest_e2e.py
"""
import hashlib
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
PY = sys.executable
TARGET = os.path.join(REPO, 'tests', 'layout.spec.js')     # 选最小的一个套件，跑得快
INJECT = "\nt('§注入的失败§', false, '这是 harness 自检注入的假失败');\n"


def sha(p):
    with open(p, 'rb') as f:
        return hashlib.md5(f.read()).hexdigest()


def main():
    if not os.path.isfile(TARGET):
        print('✗ 找不到自检靶子：%s' % TARGET)
        return 2

    orig_hash = sha(TARGET)
    bak = TARGET + '.selftest-bak'

    try:
        shutil.copy2(TARGET, bak)

        # ---- 注入失败断言：塞在汇总行之前 ----
        with open(TARGET, 'r', encoding='utf-8') as f:
            src = f.read()
        marker = "console.log('通过 "
        idx = src.rfind(marker)
        if idx < 0:
            print('✗ 靶子里找不到汇总行，无法注入')
            return 2
        injected = src[:idx] + INJECT + '\n' + src[idx:]
        with open(TARGET, 'w', encoding='utf-8', newline='') as f:
            f.write(injected)

        # ---- 跑 harness（只跑这一个 spec） ----
        print('注入假失败后跑 harness（--only spec:layout）…\n')
        p = subprocess.run([PY, os.path.join(HERE, 'harness.py'),
                            '--only', 'spec:layout'],
                           cwd=REPO, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=120)
        out = (p.stdout or '') + (p.stderr or '')
        print(out)

        # ---- 判定 ----
        rc = p.returncode
        said_fail = 'FAIL' in out and 'spec:layout' in out
        said_green = '全部通过' in out

        ok = (rc != 0) and said_fail and not said_green
        print('=' * 60)
        print('退出码 = %d（期望非 0）' % rc)
        print('被判 FAIL = %s（期望 True）' % said_fail)
        print('误报全绿 = %s（期望 False）' % said_green)
        print('=' * 60)
        if ok:
            print('✓ harness 行为正确：注入的失败被抓住了 ✅')
            return 0
        print('✗ harness 漏报了！它没抓住注入的失败 ❌')
        return 1

    finally:
        # ---- 无条件恢复 ----
        if os.path.isfile(bak):
            shutil.copy2(bak, TARGET)
            os.remove(bak)
            if sha(TARGET) == orig_hash:
                print('\n[恢复] %s 已逐字节还原 ✅' % os.path.basename(TARGET))
            else:
                print('\n[恢复] ⚠️ 还原后哈希不一致！请人工检查 %s' % TARGET)


if __name__ == '__main__':
    sys.exit(main())
