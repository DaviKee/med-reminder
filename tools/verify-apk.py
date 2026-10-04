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

    def read_opt(name):
        # 模块缺失时返回空串，让相关检查逐条报错 —— 而不是整个脚本崩在 KeyError 上
        try:
            return z.read(name).decode('utf-8')
        except KeyError:
            return ''

    # 2026-09-28 架构重构：app 层已是多个 ES module。
    # **所有源码级检查都在模块图的拼接文本上做**（与 tests/sources.js 同一套思路）——
    # 否则函数一搬家，检查就会因为「不在 app.js 里」而误报，而那些检查本来是对的。
    APP_MODULES = ['js/platform/lifecycle.js', 'js/core/util.js', 'js/core/store.js',
                   'js/core/schedule.js', 'js/ui/tabs.js', 'js/ui/render.js',
                   'js/ui/cards.js', 'js/ui/actions.js', 'js/ui/toast.js',
                   'js/ui/overlay.js', 'js/ui/photo.js', 'js/ui/permission.js',
                   'js/ui/fontsize.js',
                   'js/app.js']
    app = read_opt('assets/public/js/app.js')
    app_modules = [read_opt('assets/public/' + n) for n in APP_MODULES]
    notify = read('assets/public/js/platform/notifications.js')
    photo = read('assets/public/js/platform/camera.js')
    backup = read('assets/public/js/platform/storage.js')
    css = read('assets/public/css/app.css')
    html = read('assets/public/index.html')
    sw = read('assets/public/sw.js')

    APP = '\n'.join(strip_js(x) for x in app_modules)
    NOTIFY, PHOTO, CSS = strip_js(notify), strip_js(photo), strip_css(css)
    BACKUP = strip_js(backup)
    SW = strip_js(sw)

    print('产物: %s' % os.path.basename(apk))
    print('      %.2f MB   MD5=%s'
          % (os.path.getsize(apk) / 1048576, hashlib.md5(open(apk, 'rb').read()).hexdigest()))
    m = re.search(r"var APP_VERSION = '([\d.]+)';", APP)
    print('      版本 v%s' % (m.group(1) if m else '?'))
    print()

    fails = []
    total = [0]

    def check(group, name, cond, extra=''):
        total[0] += 1          # 自动计数：手工维护的项数迟早会和实际对不上
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
    for name in ['js/app.js', 'js/core/util.js', 'js/core/store.js', 'js/core/schedule.js',
                 'js/ui/overlay.js',
                 'js/platform/notifications.js', 'js/platform/camera.js', 'js/platform/storage.js',
                 'css/app.css', 'index.html', 'sw.js']:
        src = os.path.join(WEB, name)
        zp = 'assets/public/' + name
        if not os.path.exists(src):
            check('资源', os.path.basename(name), False, '源文件不存在: %s' % src)
            continue
        try:
            packed = z.read(zp)
        except KeyError:
            check('资源', os.path.basename(name), False, 'APK 内缺少 %s' % zp)
            continue
        ok = hashlib.md5(open(src, 'rb').read()).hexdigest() == hashlib.md5(packed).hexdigest()
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
        ('通知-渠道不传 sound', "sound: 'default'" not in NOTIFY),
        ('通知-渠道重要性 HIGH', 'importance: 5' in NOTIFY),
        ('通知-doSync 串行化', 'if (syncBusy)' in NOTIFY),
        ('通知-doSync 等清场', 'bootPurge || Promise.resolve()' in NOTIFY),
        ('通知-启动清场已接线', 'window.MedNotify.purge()' in APP),
        ('通知-回前台清通知栏', 'clearDelivered()' in APP),
        # ⚠️ 2026-10-04：原来查**单文件 app**。那个按钮的 HTML 随 diagHtml/plugLine
        #    搬进了 ui/permission.js（app.js 只剩绑定），于是误报。
        #    源码级检查一律查 **APP（模块图拼接文本）**，不查单个文件。
        ('通知-DEBUG 有一键清理', 'id="btnPurgeNotif"' in APP),
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
        # F-4 固定时刻排程
        ('F-4-数据模型 mode/times', 'function medMode(m)' in APP and 'function normTimes(arr)' in APP),
        ('F-4-null 不当 0 点', "if (v == null || v === '') return;" in APP),
        ('F-4-预生成（不等打卡）', 'function ensureFixedDoses()' in APP),
        ('F-4-固定模式不顺延', "medMode(m) === 'fixed') return { shifted: 0, dropped: 0 }" in APP),
        ('F-4-改设置后重排今天', 'function rebuildTodayDoses(m)' in APP),
        ('F-4-启动与跨天都接线', APP.count('ensureFixedDoses();') >= 3),
        ('F-4-界面（模式切换+时刻编辑）', 'id="modeRow"' in html and 'id="timeList"' in html and 'id="addTime"' in html),
        ('F-4-样式 44px 触摸目标', '.time-input' in CSS and 'height:44px' in CSS),
        # F-7 自动本地备份
        ('F-7-备份目录优先 DOCUMENTS（鸿蒙=用户可见；Android 无权限自动退 EXTERNAL）',
         "var DIRS = ['DOCUMENTS', 'EXTERNAL', 'DATA'];" in BACKUP),
        ('F-7-先建目录再写文件', 'FS.mkdir(' in BACKUP and 'recursive: true' in BACKUP),
        ('F-7-防抖', 'DEBOUNCE_MS' in BACKUP and 'clearTimeout(timer)' in BACKUP),
        ('F-7-保留策略', 'KEEP' in BACKUP and 'deleteFile' in BACKUP),
        ('F-7-保存时自动备份', 'MedAutoBackup.schedule' in APP),
        ('F-7-用同一份备份格式', 'JSON.stringify(buildBackup())' in APP),
        ('F-7-记录页有卡片', 'function autoBackupCardHtml(' in APP and 'AUTO · 自动备份' in APP),
        ('F-7-明示卸载会删掉', '卸载 App 会连这个目录一起删掉' in APP),
        ('F-7-切后台落盘', 'MedAutoBackup.flush()' in APP),
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
        # 布局：长 token 不许顶出卡片（2026-09-28 真机反馈：AUTO 卡的备份目录路径）
        ('布局-正文允许长 token 断行', 'overflow-wrap:anywhere' in CSS),
        ('布局-备份目录路径插了断行机会', "'/<wbr>'" in APP),
        # 2026-09-28 两个新功能：左右滑动切页 / 历史收进二级菜单
        ('滑动-判定是纯函数且已接线',
         'function swipeTarget(cur, dx, dy)' in APP and 'bindSwipe();' in APP),
        ('滑动-passive 且不设 touch-action',
         '{ passive: true }' in APP and 'touch-action' not in APP and 'touch-action' not in CSS),
        ('滑动-浮层开着时不切页', 'isOverlayOpen()) return;' in APP),
        ('历史-页面只放摘要与入口', 'data-hall' in APP and 'historySummaryHtml()' in APP),
        ('历史-二级菜单骨架齐全',
         'id="dlgHistory"' in html and 'id="histBody"' in html and 'id="histClose"' in html),
        ('历史-返回键能关二级菜单', "'dlgHistory'" in APP),
        ('历史-筛选走事件委托',
         "document.addEventListener('click'" in APP and "closest('[data-hist]')" in APP),
        ('历史-旧直接绑定已移除', "$$('[data-hist]').forEach" not in APP),
        ('D-1 存储可见', 'storageError = {' in APP),
        ('权限卡状态驱动', 'isBlocked(after)' in APP),
        ('插件 JS 随包', len([x for x in z.namelist() if 'assets/public/vendor/' in x]) >= 5),
        # 架构重构（2026-09-28 起）：core 模块必须随包，且 index.html 要显式声明
        # —— SW 的预缓存清单靠解析 index.html 得到，而 import 是隐式的、解析不到。
        ('架构-app 模块随包（core + ui）',
         all(('assets/public/' + m) in z.namelist()
             for m in APP_MODULES)),
        ('架构-index.html 声明了全部 app 模块',
         all(('href="' + m + '"') in html for m in APP_MODULES if m.endswith('.js')
             and '/js/' in m)),
        # H-1 / H-2（v1.4.2）
        ('H-1-判定走 needRebuildDoses',
         'function needRebuildDoses(prev, next)' in APP
         and 'if (needRebuildDoses(prev, next)) rebuildTodayDoses(m);' in APP),
        ('H-1-含"固定模式时刻变了"这一支',
         "next.mode === 'fixed') return !sameTimes(prev.times, next.times);" in APP),
        ('H-1-旧判定已消失', 'if (modeChanged || intervalChanged) rebuildTodayDoses' not in APP),
        ('H-2-sw 走 network-first',
         'function isShellRequest(req, url)' in SW
         and '.addAll(' not in SW
         and re.search(r'isShellRequest\(req, url\)[\s\S]{0,600}?fetch\(req\)\.then', SW) is not None),
        ('H-2-预缓存含照片/备份/插件',
         all(x in SW for x in ["'./js/platform/camera.js'", "'./js/platform/storage.js'", "'./vendor/plugin-camera.js'"])),
        ('H-2-注册失败不静默',
         "serviceWorker.register('sw.js').then" in APP and "console.warn('[sw]" in APP),
        # 2026-09-24 真机反馈批次
        ('返回键已接线', "addListener('backButton'" in APP),
        ('返回键优先级判定存在', 'function backAction(s)' in APP),
        ('Esc 与返回键共用关浮层逻辑', 'if (closeTopDialog()) return;' in APP),
        ('药品页改事件委托', "closest('[data-med]')" in APP),
        ('药品页旧绑定已移除', "$$('.meditem').forEach" not in APP),
        ('openSheet 防重复打开',
         "if ($('#sheetMed').classList.contains('show')) return;" in APP),
        ('轮询不打断浮层', 'if (isOverlayOpen()) return;' in APP),
        ('sw 超时回退', 'NET_TIMEOUT_MS' in SW and 'networkFirst(req)' in SW),
        ('保存有防重入守卫',
         'function sheetSaveBegin()' in APP and 'if (!sheetSaveBegin()) return;' in APP),
        ('打开浮层会重置防重入标记', 'sheetSaveReset();' in APP and APP.count('sheetSaveReset()') >= 2),
        ('假状态栏已删', 'statusbar' not in html),
        # 2026-09-29 真机反馈：拍完照又让再拍一次（相机期间 App 被系统杀掉）
        # —— 恢复期间必须挡住新的拍照会话，且 kind:'one' 的 medId/doseId 错配要修掉
        ('拍照恢复-有 restoringShot 状态位', 'var restoringShot = false;' in APP),
        ('拍照恢复-恢复中拒绝新拍照',
         'if (restoringShot) { toast(' in APP),
        ('拍照恢复-已有会话时拒绝重入',
         'if (pendingShot) { toast(' in APP),
        # ⚠️ 2026-10-04：原来锁死 `restoringShot = true` 这个**字面写法** ——
        #    拍照组搬进 ui/photo.js 后改走语义入口 `enterRestore()`，于是误报。
        #    （记忆里「搬家会把 verify-apk.py 打哑」那个坑的第 N 次。）
        #    改成两种写法都认：只要"检测留痕 → 进恢复态"这件事还在做。
        ('拍照恢复-boot 检测未消费留痕',
         ('if (loadPendingShot()) enterRestore();' in APP
          or 'if (loadPendingShot()) restoringShot = true;' in APP)),
        ('拍照恢复-有 30 秒兜底解锁',
         'clearPendingShot();' in APP and re.search(r'\}, 30000\);', APP) is not None),
        ('拍照恢复-「正在恢复」诚实态',
         '正在恢复上次拍照' in APP),
        ('拍照恢复-kinded one 走 checkIn(medId)',
         'var arr = checkIn(p.doseId);' in APP),
        ('拍照恢复-旧 medId/doseId 错配已移除',
         'l[i].id === p.doseId' not in APP),
    ]
    for name, cond in regress:
        check('回归', name, cond)

    print()
    if fails:
        print('验收未通过 %d 项：' % len(fails))
        for f in fails:
            print('  - ' + f)
        sys.exit(1)
    print('全部通过（%d 项）' % total[0])


if __name__ == '__main__':
    main()
