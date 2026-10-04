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
    # ★ 2026-10-04 两处改动 —— 原来这个关口**形同虚设**：
    #
    #  ① 文件清单改成**自动扫描**。原来是手写的 9 个路径（写于还没拆模块的年代），
    #     于是 ui/ 下十几个模块**根本没被检查** —— `pendingShot` 漏 import 这种真问题
    #     就藏在里面，从 v1.5.8 一直藏到 v1.5.12，还是靠手工跑工具才抓到。
    #     **手写清单必然过时；过时的清单 = 假装在检查。**
    #
    #  ② 从「咨询性」升为**硬闸门**。原来不敢当闸门，是因为工具有已知假阳性
    #     （正则字面量被剥成标识符），怕"每天假报红、红久了没人看"。
    #     现在假阳性逐条核实后登记进工具的 KNOWN_FP 白名单，用 --strict 跑 ——
    #     **白名单外的才算真漏 import → 退出码 1**。
    #     代价是白名单要维护；但比"永远红的检查"强得多。
    jsroot = os.path.join(repo, 'www', 'js')
    app_modules = sorted(
        os.path.join(dp, f)
        for dp, _, fs in os.walk(jsroot)
        for f in fs if f.endswith('.js')
    )
    G.append({
        'id': 'imports',
        'name': 'ES module 漏 import 诊断（硬闸门：白名单外的未声明名字会报红）',
        'kind': 'script',
        'why': '搬运/新增模块时漏一个 import 不会报语法错，只在执行到那一行才 ReferenceError；'
               'spec 跑的是聚合源码文本，发现不了。假阳性由工具的 KNOWN_FP 白名单兜住。',
        'cmd': [py, os.path.join(ws, '.workbuddy', 'tools', 'check-module-imports.py'), '--strict']
               + app_modules,
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
