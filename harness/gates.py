#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""harness 的各个「关口」（gate）定义。

设计原则
--------
1. **关口只描述「跑什么、怎么判」，不自己实现检查逻辑。** 真正的检查住在
   `tests/*.spec.js`、`tools/verify-apk.py`、`.workbuddy/tools/*.py` 里 ——
   那些地方才是唯一真相。harness 的职责是**把它们串起来、统一判定、挡住谎报**。
2. **每个关口都必须给出「可复现的退出码判据」**，不能靠人眼读输出。
   - spec 类：解析「通过 N / 共 M」，M>0 且 N==M 才算过（防「假绿：空集通过」）。
   - 脚本类：以退出码为准（0 过）。
3. **跳过（skip）与通过（pass）必须区分**，且要在报告里显式说明为什么跳过。
   「没跑」绝不能显示成「通过」—— 这正是本项目反复踩的坑。

每个 gate 是一个 dict：
  id        短标识（报告里用）
  name      中文名
  kind      'spec' | 'script' | 'selftest'
  why       为什么需要它（一句话，写给人看）
  cmd       函数(ctx) -> list[str]，要跑的命令
  cwd       函数(ctx) -> str
  ...（kind 特有的字段见下）
"""
import os

# ---------- 路径常量（由 ctx 注入，这里只做缺省） ----------

SPEC_GLOB_DIR = 'tests'


def _specs(ctx):
    """按文件名排序返回全部 tests/*.spec.js 的绝对路径。"""
    d = os.path.join(ctx['repo'], 'tests')
    names = sorted(f for f in os.listdir(d) if f.endswith('.spec.js'))
    return [os.path.join(d, n) for n in names]


def gates(ctx):
    """返回本次要跑的关口列表。ctx 见 harness.py。"""
    py = ctx['python']
    node = ctx['node']
    repo = ctx['repo']
    ws = ctx['workspace']

    G = []

    # ── 关口 0：语法自检 —— 最便宜、最先跑 ───────────────────────────
    # 文件语法坏了，后面全都会以奇怪的方式失败。先花 0.2 秒挡掉。
    G.append({
        'id': 'syntax',
        'name': 'JS 语法自检（全部 www/js 模块 + spec）',
        'kind': 'script',
        'why': '语法错误会让后续关口以误导性的方式失败，先挡掉最省时间',
        'cmd': [node, os.path.join(repo, 'harness', 'check_syntax.js')],
        'cwd': repo,
        'soft': False,
    })

    # ── 关口 1：模块 import 检查（架构铁律） ─────────────────────────
    # 「用到了却没声明、也没 import」→ 运行到那行才 ReferenceError，
    # 而抽函数块的测试很可能碰不到那行 → 静默漏过。
    #
    # ⚠️ 这是一个 **advisory（咨询性）** 关口，不是硬闸门 —— 它会把真问题
    #    报出来，但它自己有**已知的假阳性**（正则字面量与注释里的词）：
    #      · schedule.js 的 `$`  ← 来自 /^(\d{1,2}):(\d{2})$/
    #      · app.js 的 `Android` ← 来自注释里的「Android 13」
    #      · backup.js 的 `backup`、photo.js 的 vendor 压缩码（A/Za/z0…）
    #    把它当硬闸门 = 每天假报红，红久了就没人看了（"狼来了"）。
    #    所以：**永远执行、输出保留给人看、但不影响退出码。**
    app_modules = [
        'www/js/core/util.js', 'www/js/core/store.js', 'www/js/core/schedule.js',
        'www/js/ui/overlay.js', 'www/js/app.js',
        'www/js/platform/notifications.js', 'www/js/platform/camera.js', 'www/js/platform/storage.js',
    ]
    G.append({
        'id': 'imports',
        'name': 'ES module 漏 import 诊断（咨询性：有已知假阳性，不阻断）',
        'kind': 'advisory',
        'why': '搬运/新增模块时容易漏 import；但该工具对正则字面量会假报，故只看不当闸',
        'cmd': [py, os.path.join(ws, '.workbuddy', 'tools', 'check-module-imports.py')]
               + [os.path.join(repo, m) for m in app_modules],
        'cwd': repo,
        'soft': False,
    })

    # ── 关口 2：全部 spec（真正的回归防护） ───────────────────────────
    # 每个 spec 单独起进程 —— 沙箱可能禁子进程，逐个跑最稳，也便于定位。
    for p in _specs(ctx):
        rel = os.path.relpath(p, repo).replace('\\', '/')
        G.append({
            'id': 'spec:' + os.path.basename(p).replace('.spec.js', ''),
            'name': '用例 ' + os.path.basename(p),
            'kind': 'spec',
            'why': '源码级行为回归（在 vm 里跑真实源码文本）',
            'cmd': [node, p],
            'cwd': repo,
            'soft': False,
        })

    # ── 关口 3：APK 内容级验收（有包才跑） ───────────────────────────
    G.append({
        'id': 'apk',
        'name': 'APK 内容级验收（源码级断言 + 模块随包 + 版本一致）',
        'kind': 'script',
        'why': '交付前的最后一道闸：验产物，不是验命令',
        'cmd': [py, os.path.join(repo, 'tools', 'verify-apk.py')],
        'cwd': repo,
        'soft': True,              # 没有 APK 时跳过，不算失败
        'skip_if': lambda: ctx['apk'] is None,
        'skip_why': lambda: '没有找到 MedReminder-v*.apk（未出包）',
    })

    return G
