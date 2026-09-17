#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""APK 内容级验收

为什么要脚本化：这个检查以前是每次手写在命令行里的，结果**同一个坑踩了三次** ——
拿「源码里还有没有某个旧写法」去查 apk 里的资源时，匹配到了我自己写在注释里的旧代码示例，
于是明明已经修好的东西报 False。

所以这里定死两条规矩：
  1. 查「代码里有没有 X」之前，**必须先剥离注释**（strip_js）。
  2. 每条检查都写成 (名称, 文件, 表达式, 期望)，跑完打印结果，不靠人眼。

用法：python tools/verify-apk.py [apk 路径]
      不给路径就取 med-reminder/ 下最新的 MedReminder-v*.apk
"""
import hashlib
import glob
import os
import re
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                      # med-reminder/
WEB = os.path.join(ROOT, 'www')


def strip_js(src):
    """剥离 /* */ 与 // 注释。

    ★ 这一步不能省。我们习惯在注释里引用旧写法做对比说明，
      不剥离就会把「说明文字」当成「代码」而误报。
    """
    src = re.sub(r'/\*[\s\S]*?\*/', '', src)
    src = re.sub(r'^\s*//.*$', '', src, flags=re.M)
    return src


def strip_css(src):
    return re.sub(r'/\*[\s\S]*?\*/', '', src)


def pick_apk(argv):
    if len(argv) > 1:
        return argv[1]
    cands = sorted(glob.glob(os.path.join(ROOT, 'MedReminder-v*.apk')), key=os.path.getmtime)
    if not cands:
        sys.exit('找不到 APK，请先出包')
    return cands[-1]


def main():
    apk = pick_apk(sys.argv)
    z = zipfile.ZipFile(apk)

    def read(name):
        return z.read(name).decode('utf-8')

    app = read('assets/public/js/app.js')
    notify = read('assets/public/js/notify.js')
    photo = read('assets/public/js/photo.js')
    css = read('assets/public/css/app.css')
    html = read('assets/public/index.html')

    APP, NOTIFY, PHOTO, CSS = strip_js(app), strip_js(notify), strip_js(photo), strip_css(css)

    print('产物: %s' % os.path.basename(apk))
    print('      %.2f MB   MD5=%s'
          % (os.path.getsize(apk) / 1048576, hashlib.md5(open(apk, 'rb').read()).hexdigest()))
    m = re.search(r"var APP_VERSION = '([\d.]+)';", APP)
    print('      版本 v%s' % (m.group(1) if m else '?'))
    print()

    fails = []

    def check(group, name, cond, extra=''):
        ok = bool(cond)
        print('  %-26s %s%s' % (name, 'OK ' if ok else '!! ', extra if not ok else ''))
        if not ok:
            fails.append('%s / %s' % (group, name))

    print('=== 版本与构建号 ===')
    bc = open(os.path.join(ROOT, 'android/app/build.gradle'), encoding='utf-8').read()
    vcode = re.search(r'versionCode\s+(\d+)', bc)
    # versionCode 规则：1 | minor(2 位) | patch(2 位)
    #   v1.1.2 -> 10102   v1.2.0 -> 10200   v1.2.1 -> 10201
    expect = None
    if m:
        parts = (m.group(1).split('.') + ['0', '0'])[:3]
        expect = '%d%s%s' % (int(parts[0]), parts[1].zfill(2), parts[2].zfill(2))
    check('版本', 'APP_VERSION 与构建号一致',
          expect and vcode and expect == vcode.group(1),
          'APP_VERSION=%s 推得=%s versionCode=%s' % (m and m.group(1), expect, vcode and vcode.group(1)))

    print()
    print('=== 资源与源一致（构建没吃到旧文件）===')
    for name in ['js/app.js', 'js/notify.js', 'js/photo.js', 'css/app.css', 'index.html']:
        src = os.path.join(WEB, name)
        zp = 'assets/public/' + name
        ok = hashlib.md5(open(src, 'rb').read()).hexdigest() == hashlib.md5(z.read(zp)).hexdigest()
        check('资源', os.path.basename(name), ok, '与 www/ 下的源不一致')

    print()
    print('=== 回归：历次修复的关键点（注释已剥离）===')
    # (名称, 条件)
    regress = [
        # 通知清场（v1.2.1）
        ('通知-不再依赖内存早退', '!scheduled.length' not in NOTIFY),
        ('通知-用 getPending 拿清单', 'LN.getPending()' in NOTIFY),
        ('通知-测试用固定 id', 'var TEST_ID = 990000001' in NOTIFY and 'Math.random() * 2000000000' not in NOTIFY),
        ('通知-测试先取消再排', 'LN.cancel({ notifications: [{ id: TEST_ID }] })' in NOTIFY),
        ('通知-purge 已导出', 'function purge()' in NOTIFY and 'purge: purge' in NOTIFY),
        ('通知-clearDelivered 已导出', 'clearDelivered: clearDelivered' in NOTIFY),
        ('通知-stat 已导出', 'stat: stat' in NOTIFY),
        ('通知-台账落盘', 'medreminder.notifPending.v1' in NOTIFY),
        ('通知-doSync 串行化', 'if (syncBusy)' in NOTIFY),
        ('通知-doSync 等清场', 'bootPurge || Promise.resolve()' in NOTIFY),
        ('通知-启动清场已接线', 'window.MedNotify.purge()' in APP),
        ('通知-回前台清通知栏', 'clearDelivered()' in APP),
        ('通知-DEBUG 有一键清理', 'id="btnPurgeNotif"' in app),
        # 拍照（v1.1.3 / v1.2.0）
        ('拍照-落盘前建目录', 'function ensureDir()' in PHOTO and 'recursive: true' in PHOTO),
        ('拍照-两条路都带原因', "'copy: ' + (firstErr || '?')" in PHOTO),
        ('拍照-L1 质量校验', 'function qualityIssue(a)' in PHOTO),
        ('拍照-L2 防重复', 'function isDuplicate(h)' in PHOTO and 'function hamming(a, b)' in PHOTO),
        ('拍照-放行优先原则', '宁可放行，不可误伤' in photo),   # ★ 这句只在注释里，故意查未剥离文本
        ('拍照-只允许现场拍（禁相册）', "source: 'CAMERA'" in PHOTO and "source: 'camera'" not in PHOTO),
        ('拍照-不传 promptLabel', 'promptLabel' not in PHOTO),
        ('拍照-5 个打卡入口', APP.count('photoGate({') >= 5),
        ('药品-删除走二次确认', 'function deleteMed(id)' in APP and 'askConfirm(' in APP),
        ('药品-删除撤掉系统通知', 'window.MedNotify.cancelOne(d.id)' in APP),
        ('药品-删除保留历史记录', 'deleteMed' in APP and 'd.medId !== id' in APP),
        ('药品-有通用确认框', 'id="dlgConfirm"' in html and 'id="confirmOk"' in html),
        # 前几轮成果
        ('B-1 snooze 独立字段', 'function snoozeDose(d)' in APP and 'snoozeUntil' in APP),
        ('B-1 不再篡改计划时刻', 'ds.time = Math.min(1439' not in APP),
        ('F-2 依从率分开统计', 'function adherenceStats(medId)' in APP),
        ('F-2 历史列表与筛选', 'HISTORY · 近 ' in APP and 'data-hist=' in APP),
        ('F-3 首次加药引导', 'function guideNotifyOnce()' in APP),
        ('B-2 跨天可见化', 'function droppedCardHtml()' in APP),
        ('F-1 30 分钟粒度', 'function intervalLabel(v)' in APP and 'stepVal -= 0.5' in APP),
        ('A-2 44x44 命中区', '.icon-btn::after' in CSS and 'width:44px' in CSS),
        ('A-4 周点形状区分', '.dot.today::after' in CSS),
        ('A-5 reduced-motion', 'prefers-reduced-motion' in CSS),
        ('D-1 存储可见', 'storageError = {' in APP),
        ('权限卡状态驱动', 'isBlocked(after)' in APP),
        ('插件 JS 随包', len([x for x in z.namelist() if 'assets/public/vendor/' in x]) >= 5),
        ('假状态栏已删', 'statusbar' not in html),
    ]
    for name, cond in regress:
        check('回归', name, cond)

    print()
    if fails:
        print('验收未通过 %d 项：' % len(fails))
        for f in fails:
            print('  - ' + f)
        sys.exit(1)
    print('全部通过（%d 项）' % (len(regress) + 6))


if __name__ == '__main__':
    main()
