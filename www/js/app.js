/* 定时服药提醒 — app logic
 * 打卡制：每天第一次服药打卡后，按药品间隔自动排程当天剩余提醒。
 */

import { $, $$, esc, nowMin, minToStr, todayKey } from './core/util.js';
/* ⬆ 2026-09-24 架构重构：这些工具函数已抽到 core/util.js。
 * import 必须在模块顶层 —— 所以它在 IIFE 外面，IIFE 内部靠闭包可见。
 * 其余代码一字未改。 */


import { load, S, storageStats, gcNotified, cleanTargets, save, setSaveHooks } from './core/store.js';
import { writeCardFile } from './core/cardSummary.js';


import { todayDoses, medById, dueAt, nextPending, silenceOverdue, markTaken, ensureFixedDoses, snoozeDose, snoozeToast } from './core/schedule.js';


/* ⬆ 2026-09-28 架构重构：浮层原语与返回键判定已抽到 ui/overlay.js。
 * openCount / lastFocus / confirmCb 由它自己管 —— 本文件只读、或走它给的小 API 写。 */
import { openDlg, closeDlg, closableDialogs, trapTab, backAction, isOverlayOpen, clearConfirmCb, takeConfirmCb } from './ui/overlay.js';

/* ⬆ 2026-10-01 架构重构（多平台准备）：**平台生命周期**已抽到 platform/lifecycle.js。
 * 这里原本散着 3 处 `window.Capacitor` 直取（backButton / appRestoredResult / appStateChange），
 * 现在统一走本模块的回调注入 —— 业务层不再认识 Capacitor。
 * `exitApp` 也一并移过去了（它原本在 ui/overlay.js，但那是平台操作、不是浮层原语）。 */
import { exitApp, onBackButton, onCameraRestored, onAppStateChange } from './platform/lifecycle.js';


import { medScheduleLabel } from './ui/cards.js';
/* ⬆ 2026-10-01（A-2 第 14 步）：**重绘调度器**。它是"请重绘"的底层入口，
 * 让视图模块（将来的 ui/today.js 等）不必 import 上层的 `render` —— 那是反向依赖。 */
import { setRenderers, markDirty, render } from './ui/render.js';


import { toast } from './ui/toast.js';


import { syncNotifications, takenToast, rollDayIfNeeded, markToday, queueStorageRefresh } from './ui/actions.js';


/* （搬运器分两批各插了一行 import，已合并到上面那条；
 *   `lastDay` 不再 import —— 它只由 actions.js 内部读写，外部走 markToday()） */


import { setTab, currentTab } from './ui/tabs.js';


/* （`queueStorageRefresh` 已并入上面那条 actions import） */


/* ⚠️ 拍照组的状态分两类，**别混**：
 *    · 外部要**写**（清会话 / 进恢复态）→ 一律走语义入口（`discardShotSession` /
 *      `enterRestore` / `leaveRestore`），不要直接赋值。
 *    · 外部要**读** → 正常 import 那个 `export var`（活绑定会同步到最新值）。
 *  📌 2026-10-04 修正：`pendingShot` 上一轮被误当成"内部状态"摘掉了 import，
 *     但 #photoTake 的重试逻辑要**读**它（拿 doseId 与上次的报错文案）→
 *     后果是「拍照失败后点重试」直接 ReferenceError（spec 查源码文本，抓不到；
 *     是 check-module-imports 抓出来的）。读的一律留着 import。 */
import { loadPendingShot, photoGate, finishShot, stampPhoto, resumeShot, refreshPhotoStats, allPhotoRefs, pendingShot, restoringShot, discardShotSession, enterRestore, leaveRestore } from './ui/photo.js';


import { refreshPerm, invalidatePermProbe, initBrowserPerm } from './ui/permission.js';


import { loadFontScale, applyFontScale } from './ui/fontsize.js';
import { initTheme } from './ui/theme.js';


import { openSheet, closeSheet, bindSheet } from './ui/sheet.js';


import { renderToday, countdownText, bindSkipDlg } from './ui/today.js';


import { buildBackup, openDataDlg, cleanKeep, bindDataDlg, disarmRestore } from './ui/data.js';


import { renderRecords, setVersionInfo, setTestReminder, bindHistoryFilter } from './ui/records.js';
import { bindViz } from './ui/viz.js';


import { dropLegacySamples, renderMeds } from './ui/meds.js';

(function () {
  'use strict';

  /* ---------------- 重绘调度器接线（A-2 第 14 步） ----------------
   * 把三个视图的渲染函数注入底层调度器。函数声明会提升，所以这里可以前向引用。
   * ⚠️ **必须早于任何一次 markDirty/render** —— 注册之前发生的标脏不会丢（dirty 会留着），
   *    但注册本身要尽快，否则首屏会白等一轮。 */
  setRenderers({ today: renderToday, meds: renderMeds, records: renderRecords });

  /* ---------------- 上层钩子注入（core 层不许反向 import） ----------------
   * save() 住在 core/store.js，但它写完盘要做三件属于「上层」的事：
   *   ① 刷新存储告警卡片   ② 同步系统通知   ③ 触发自动备份
   * 把这三件事注册进去 —— **必须在任何一次 save() 之前**，所以放在最前面。
   * 三个被引用的函数都是本 IIFE 内的函数声明（hoisted），此处只是取引用、不调用。 */
  setSaveHooks({
    refresh: queueStorageRefresh,
    notify: syncNotifications,
    /* S-9：每次落盘顺手刷新桌面卡片的摘要文件（实现见 core/cardSummary.js）。 */
    card: writeCardFile,
    backup: function () {
      if (window.MedAutoBackup) {
        window.MedAutoBackup.schedule(function () { return JSON.stringify(buildBackup()); });
      }
    }
  });


  /* ---------------- 版本号（单一来源） ----------------
   * 只在这里改一处：build-apk.sh 会自动读出这两个值，用于
   * ① 命名 APK 文件  ② 同步 android/app/build.gradle 的 versionName / versionCode。
   *
   * 为什么 App 内也要显示：装上手机之后没法确认装的是哪个版本。前几轮验收都是靠
   * 「某个功能有没有出现」反推版本，很容易搞混 —— 秦老师也提了这个问题。
   * 现在记录页 DEBUG 卡会直接写出「版本 vX.Y.Z · 日期 · 提交号」。
   *
   * 编号规则（v主.次.修订）：
   *   修订 +1  修 bug
   *   次   +1  加功能
   *   主   +1  不兼容变更（数据格式之类）
   * 历史对照表见 MedReminder-后续任务计划.md 的「版本历史」。 */
  var APP_VERSION = '1.5.21';
  var APP_BUILD = '2026-10-07';
















  /* save() 已搬到 core/store.js（见顶部 import）。
   * 它写完盘后通过 setSaveHooks() 注入的三个钩子回到本文件：
   * 刷新存储告警 / 同步系统通知 / 触发自动备份 —— 注册见 IIFE 顶部。 */




  function cleanOldRecords(keepDays) {
    var keys = cleanTargets(keepDays);
    if (!keys.length) return { days: 0, doses: 0, freed: 0 };
    var before = storageStats().bytes, n = 0;
    keys.forEach(function (k) {
      n += (S.doses[k] || []).length;
      delete S.doses[k];
    });
    gcNotified();     // 告知标记要跟着回收，否则 doses 删了体积也降不下来
    save();
    /* 记录删了，对应照片必须跟着回收 —— 否则照片一直占着空间，D-1 的体积治理等于没做 */
    if (window.MedPhoto && window.MedPhoto.ready()) {
      window.MedPhoto.gc(allPhotoRefs()).then(function (k) {
        if (k) { refreshPhotoStats(); }
      });
    }
    return { days: keys.length, doses: n, freed: Math.max(0, before - storageStats().bytes) };
  }































































  /* ---------------- 历史：摘要 / 全部列表 / 二级菜单 ---------------- */

















  function closeTopDialog() {
    var closable = closableDialogs();
    if (!closable.length) return false;
    var top = closable[closable.length - 1];
    /* 关掉拍照对话框 = 放弃这次打卡。必须清掉「待恢复」标记，
     * 否则下次启动会被误当成「拍照途中被杀」而自动补一次打卡。 */
    if (top.id === 'dlgPhoto') discardShotSession();
    disarmRestore();     // 关掉数据浮层 = 解除「恢复」的二次确认武装（写走入口）
    closeDlg(top);
    return true;
  }

  function handleBackButton() {
    var a = backAction({
      closableDlg: closableDialogs().length > 0,
      sheetOpen: $('#sheetMed').classList.contains('show'),
      anyDlg: $$('.dlg-wrap.show').length > 0,
      tab: currentTab
    });
    if (a === 'close-dlg') { closeTopDialog(); return; }
    if (a === 'close-sheet') { closeSheet(); return; }
    if (a === 'consume') return;                       // 提醒弹窗在，不退出
    if (a === 'go-today') { setTab('today'); return; }
    exitApp();
  }






  /* ---------------- reminder ---------------- */
  function tick() {
    var cd = $('#countdown');
    if (cd) cd.textContent = countdownText(nextPending());

    var list = todayDoses();
    var now = nowMin();
    for (var i = 0; i < list.length; i++) {
      var ds = list[i];
      if (ds.status !== 'pending') continue;
      if (S.notified[ds.id]) continue;
      /* ★ 防重复弹窗（2026-10-07 真机反馈）：同一种药刚打过卡（2 分钟内），
       * 就不再为它的下一条剂量自动弹提醒 —— 真机上出现过「点完已服药，
       * 同一种药的打卡框又弹出来」。无论设备上的真实路径是哪条
       * （桌面 Chromium 复现不了），这道闸都把症状堵死。
       * 剂量本身仍是 pending：该手动补打卡的照样能补，只是不再自动弹。 */
      var lastTk = 0;
      for (var j = 0; j < list.length; j++) {
        var o = list[j];
        if (o.medId === ds.medId && o.status === 'taken' && o.takenAt && o.takenAt > lastTk) lastTk = o.takenAt;
      }
      if (lastTk && Date.now() - lastTk < 120000) {
        S.notified[ds.id] = 1; save();
        if (window.MedNotify && window.MedNotify.native) window.MedNotify.cancelOne(ds.id);
        continue;
      }
      var due = dueAt(ds);                 // 延后过就按延后时刻；原计划时刻不动
      if (due > now) continue;
      if (now - due > 30) { S.notified[ds.id] = 1; save(); continue; } // 过期太久，静默
      S.notified[ds.id] = 1; save();
      // 前台（App 可见）弹页面内提醒，确保开屏也看得见；
      // 后台 / 锁屏 / 息屏由系统通知负责，无需此处处理
      if (document.visibilityState === 'visible') {
        if (window.MedNotify && window.MedNotify.native) window.MedNotify.cancelOne(ds.id); // 撤销系统通知，避免与页面内弹窗重复
        showReminder(ds);
      }
      break;
    }
  }

  var remindDose = null;
  function showReminder(ds) {
    /* ★ 幂等护栏：只有「待服」才值得弹。已服/已跳过的剂量再弹就是骚扰
     * （真机 2026-10-07 反馈的重复弹窗，防线之一）。 */
    if (!ds || ds.status !== 'pending') return;
    var med = medById(ds.medId);
    remindDose = ds;
    $('#remindName').textContent = med ? med.name : '服药时间';
    $('#remindMeta').textContent = minToStr(ds.time)
      + (ds.snoozeUntil != null ? ('（已延后至 ' + minToStr(ds.snoozeUntil) + '）') : '')
      + ' · 第 ' + (ds.idx + 1) + ' / ' + ds.total + ' 次 · ' + (med ? esc(medScheduleLabel(med)) : '-');
    openDlg($('#dlgRemind'));
    fireNotification(med, ds);
  }
  function fireNotification(med, ds) {
    try {
      if ('Notification' in window && Notification.permission === 'granted') {
        new Notification('该服药了', { body: (med ? med.name : '') + ' · ' + minToStr(ds.time), icon: 'icon.svg' });
      }
    } catch (e) { /* ignore */ }
  }

  /* 锁屏通知上的按钮 / 点通知体 / 回到前台 */
  function onNotifyAction(action, doseId) {
    if (action === 'resume') {
      render();
      syncNotifications();
      /* 回到前台顺手清掉通知栏里**已经显示出来**的提醒：
       * 用户已经在看 App 了，那些提醒已经没有意义；而且它们很可能是刚才被系统
       * 一次性投递出来的（息屏/休眠期间到期的会被攒着，唤醒时一起弹）。 */
      if (window.MedNotify && window.MedNotify.clearDelivered) window.MedNotify.clearDelivered();
      return;
    }
    var l = todayDoses(), ds = null;
    for (var i = 0; i < l.length; i++) if (l[i].id === doseId) { ds = l[i]; break; }
    if (!ds) return;
    if (action === 'taken') {
      if (ds.status !== 'pending') return;
      var med = medById(ds.medId);
      /* 通知上的「已服用」也走拍照 —— 否则它就是一条绕过拍照的捷径，
       * 「强制拍照」直接形同虚设。点通知会唤起 App，所以能交给同一套流程；
       * 拍照期间 App 若被系统杀掉，resumeShot() 会从 appRestoredResult 把这次接上。 */
      photoGate({ desc: '这次是：' + (med ? med.name : '服药') + '。', kind: 'one', doseId: ds.id }, function (rel, skipped) {
        stampPhoto([ds], rel, skipped);
        var at = Date.now();
        var r = markTaken(ds, at);
        if (window.MedNotify) window.MedNotify.cancelOne(ds.id);
        save(); render(); toast(takenToast(med, at, r));
      });
    } else if (action === 'snooze') {
      var sr = snoozeDose(ds);
      save(); render(); toast(snoozeToast(sr));
    } else if (action === 'open') {
      if (ds.status === 'pending') showReminder(ds); else render();
    }
  }

  /* 测试提醒：验证锁屏/息屏能否正常响，不写任何服药数据 */
  function testReminder() {
    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.test(10000).then(function (ok) {
        if (!ok) { toast('测试提醒登记失败，请确认通知权限已开'); return; }
        /* 报一下系统里现在排了几条：测试提醒是**固定 id、先取消再登记**，
         * 所以无论点多少次，这个数字里只应该算它 1 条。 */
        var st = window.MedNotify.stat ? window.MedNotify.stat() : Promise.resolve(null);
        st.then(function (s) {
          invalidatePermProbe();
          refreshPerm();
          toast('已登记测试提醒，约 10 秒后响一条'
            + (s && s.pending != null ? '（系统当前排程共 ' + s.pending + ' 条）' : ''));
        });
      });
    } else {
      toast('10 秒后弹出测试提醒（浏览器需保持本页面打开）');
      var td = { id: 'test', medId: null, time: nowMin(), idx: 0, total: 1, status: 'pending' };
      setTimeout(function () { showReminder(td); }, 10000);
    }
  }










  /* ---------------- S-7：服药报告（近 7 天，给医生 / 家属看） ----------------
   * 与 CSV 的分工：CSV 是**原始记录**（给会看表的人、可导进 Excel）；
   * 报告是**结论**（回答"近 7 天按时没按时"这一个问题）。
   * 主场景是**当场把屏幕拿给医生看** → 正文用排版好的 HTML，不塞进文本框；
   * 「复制文字」只作次要出口（发给家人时纯文本最通用）。 */


















  /* ---------------- 存储 UI（D-1） ---------------- */





  /* ⚠️ `render()` 已**移入 `ui/render.js`**（2026-10-01）——
   * 它只要还住在 app.js，视图模块调它就等于"引用上层的函数"，拆视图永远卡住这一格。
   * 本文件里 55 处 `render()` 调用照旧，只是现在从 import 来。
   * 语义：`render()` = 全量重绘；`markDirty('today')` = 只重绘今天页（新代码优先用后者）。 */

  /* ---------------- 左右滑动切换选项卡 ----------------
   * 判定抽成**纯函数**（与 backAction 同一套路）：DOM 事件在调用方拍好再传进来，
   * 所以判定矩阵可以直接单测，不必造触摸事件。
   * 返回 null 表示「这次手势不该切页」。 */
  var TAB_ORDER = ['today', 'meds', 'records'];
  var SWIPE_MIN_PX = 60;          // 太短的多半是误触（也不是滚动，就是手抖）
  function swipeTarget(cur, dx, dy) {
    if (Math.abs(dx) < SWIPE_MIN_PX) return null;
    /* 竖向位移不小于横向 → 用户是在滚页面，不是在切页。不判这一条，
     * 一边上下滚一边手一歪就会莫名其妙换页。 */
    if (Math.abs(dx) <= Math.abs(dy)) return null;
    var i = TAB_ORDER.indexOf(cur);
    if (i < 0) return null;
    var j = dx < 0 ? i + 1 : i - 1;             // 左滑 → 下一个；右滑 → 上一个
    if (j < 0 || j >= TAB_ORDER.length) return null;   // 到头就停住，不绕回
    return TAB_ORDER[j];
  }

  /* 监听器挂在 #main 上（它不会被重建，所以绑一次就够）。
   * ⚠️ 用 passive 监听、且**不设 touch-action** —— 设了会把双指缩放一起废掉，
   *    而 capacitor.config 里的 zoomEnabled:true 是刻意开的。 */
  function bindSwipe() {
    var main = $('#main');
    if (!main) return;
    var x0 = 0, y0 = 0, tracking = false;

    main.addEventListener('touchstart', function (ev) {
      tracking = false;
      if (ev.touches.length !== 1) return;                  // 双指 = 缩放，不参与
      if (isOverlayOpen()) return;                          // 浮层开着时不切页
      var t = ev.target;
      if (t && t.closest && t.closest('.dlg-wrap, .sheet, input, textarea, .dlg-scroll')) return;
      tracking = true;
      x0 = ev.touches[0].clientX;
      y0 = ev.touches[0].clientY;
    }, { passive: true });

    main.addEventListener('touchend', function (ev) {
      if (!tracking) return;
      tracking = false;
      var t = ev.changedTouches && ev.changedTouches[0];
      if (!t) return;
      var next = swipeTarget(currentTab, t.clientX - x0, t.clientY - y0);
      if (next) setTab(next);
    }, { passive: true });
  }


  /* ---------------- boot ---------------- */
  function boot() {
    // 字号必须最先应用：晚于首次 render 会先按默认字号画一遍再跳变，肉眼可见闪一下
    applyFontScale(loadFontScale(), false);
    // 主题同理（首屏已由 index.html 的内联脚本设好，这里同步 JS 状态并挂上系统监听）
    initTheme();

    // 不再预置任何示例药品：服药场景里「看起来像真药」的假数据会造成误导，
    // 用户可能以为自己在吃阿莫西林。空态改为引导（见 renderToday 的 HOW IT WORKS）。
    // 过期提醒静默
    silenceOverdue();
    gcNotified();     // 回收孤儿告知标记（纯垃圾回收，不碰用户可见数据），再 save 一并写回
    save();

    $$('.tab').forEach(function (t) {
      t.onclick = function () { setTab(t.getAttribute('data-tab')); };
    });
    bindSwipe();

    /* 记录页要靠 app.js 喂两样东西：版本号（住在下面的 IIFE 里）、
     * 测试提醒（依赖提醒流程）。见 ui/records.js 里 setVersionInfo 的注释。 */
    setVersionInfo(APP_VERSION, APP_BUILD);
    setTestReminder(testReminder);

    bindHistoryFilter();
    /* S-6 月历翻页：委托在 document 上，只注册一次 */
    bindViz();

    bindSheet();
    $('#confirmCancel').onclick = function () { clearConfirmCb(); closeDlg($('#dlgConfirm')); };
    $('#confirmOk').onclick = function () {
      var cb = takeConfirmCb();
      closeDlg($('#dlgConfirm'));
      if (cb) cb();
    };

    bindSkipDlg();

    /* ---- 存储（D-1） ---- */
    /* 告警卡会随两个视图的 innerHTML 重建而消失，静态 onclick 绑不住 —— 用事件委托，
     * 只绑一次、永久有效。 */
    document.addEventListener('click', function (ev) {
      var el = ev.target && ev.target.closest ? ev.target.closest('[data-storage-fix]') : null;
      if (!el) return;
      /* 数据正在丢，第一优先是把已有记录抢救出来 —— 直接开导出备份，
       * 而不是先让用户自己去找入口。备份后回到记录页就能看到清理入口。 */
      setTab('records');
      openDataDlg('backup');
    });

    /* ---- 拍照打卡（G-1） ---- */
    $('#photoTake').onclick = function () {
      var btn = this;
      if (!pendingShot) { closeDlg($('#dlgPhoto')); return; }
      btn.disabled = true;
      btn.textContent = '正在调用相机…';
      var reqBody = $('#photoBody');
      if (reqBody && pendingShot.body) reqBody.textContent = pendingShot.body;   // 复位上一次的报错文案
      window.MedPhoto.take(pendingShot.doseId).then(function (r) {
        btn.disabled = false;
        btn.textContent = '拍照打卡';
        if (r.ok) { finishShot(r.rel, false); return; }
        if (r.reason === 'cancelled') return;      // 用户自己退出相机，留在对话框让他重选
        /* 三类推失败，文案必须分开 —— 否则用户不知道是自己没拍好，还是出了故障。
         * 每一类都保留「点下面跳过」的逃生通道（方案 B：可跳过但留痕）。 */
        var body = $('#photoBody');
        if (!body) return;
        if (r.reason === 'low-quality' || r.reason === 'duplicate') {
          body.textContent = '这张没能通过检查：' + (r.msg || '画面不合格')
            + '。请重拍一张，或者点下面跳过（记录里会标「未拍照」）。';
        } else {
          body.textContent = '没能拍照（' + (r.msg || r.reason) + '）。可以再试一次，或点下面跳过。';
        }
      });
    };
    $('#photoSkip').onclick = function () { finishShot(null, true); };
    $('#viewClose').onclick = function () { closeDlg($('#dlgView')); };

    /* 拍照会启动一个独立的相机 Activity，系统可能在过程中杀掉本 App。
     * Capacitor 官方明确要求监听这个事件 —— 不处理会**同时丢照片和打卡**。
     * 事件接在 platform/lifecycle.js；这里只提供「拿到路径之后干什么」。 */
    if (window.MedPhoto && window.MedPhoto.ready()) {
      onCameraRestored(function (path) { resumeShot(path); });
      refreshPhotoStats();
      /* 安全回收：只删「没有任何记录引用」的照片文件（纯垃圾回收，不碰被引用的）。
       * 与 D-1 的 gcNotified 同一思路。 */
      window.MedPhoto.gc(allPhotoRefs()).then(function (k) {
        if (k) refreshPhotoStats();
      });
    }

    $('#cleanCancel').onclick = function () { closeDlg($('#dlgClean')); };
    $('#cleanConfirm').onclick = function () {
      if (!cleanKeep) return;                       // 按钮本应是禁用态，这里是兜底
      var r = cleanOldRecords(cleanKeep);
      closeDlg($('#dlgClean'));
      cleanKeep = 0;
      render();
      if (!r.days) { toast('没有可清理的记录'); return; }
      /* 不承诺释放多少 KB：localStorage 的实际占用由实现决定，估算值报出来容易对不上 */
      toast('已清理 ' + r.days + ' 天 / ' + r.doses + ' 条记录');
    };

    $('#remindDone').onclick = function () {
      var dose = remindDose;
      /* 先关提醒弹窗再开拍照弹窗 —— 否则两个浮层会叠在一起 */
      closeDlg($('#dlgRemind'));
      remindDose = null;
      if (!dose) { render(); return; }
      var med = medById(dose.medId);
      photoGate({ desc: '这次是：' + (med ? med.name : '服药') + '。', kind: 'one', doseId: dose.id }, function (rel, skipped) {
        stampPhoto([dose], rel, skipped);
        var at = Date.now();
        var r = markTaken(dose, at);
        /* ★ 撤掉这条剂量的系统通知 —— onNotifyAction 的「已服用」路径有这一步，
         * 这里原来漏了，靠 save→syncNotifications 兜底（那是 250ms 防抖的，
         * 极端时序下通知会多活一会儿再响一次）。 */
        if (window.MedNotify && window.MedNotify.native) window.MedNotify.cancelOne(dose.id);
        save(); render();
        toast(takenToast(med, at, r));
      });
    };
    $('#remindSnooze').onclick = function () {
      var msg = '这条已经处理过了';
      if (remindDose) {
        msg = snoozeToast(snoozeDose(remindDose));
        save();
      }
      closeDlg($('#dlgRemind')); remindDose = null; render(); toast(msg);
    };

    bindDataDlg();

    /* ---- 药品页：事件委托（只绑一次、永久有效） ----
     * 为什么不用「每次 render 后逐个 onclick」：`render()` 是
     * `renderToday(); renderMeds(); renderRecords();` —— 一次把三个视图的 innerHTML
     * 整个重建。手指落下（pointerdown/touchstart）到 click 之间只要重绘一次，
     * **正在点的那个元素就被换掉了**，click 落在已脱离文档的节点上，什么都不会发生。
     * 用户看到的就是「点了没反应」，于是连点（2026-09-24 反馈）。
     * 而 refreshPerm 每 5 秒轮询一次、签名一变就 render() —— 这个重绘**不受用户控制**。
     * 委托绑在 document 上，天然免疫重绘；项目里存储卡早就用了同一招。 */
    document.addEventListener('click', function (ev) {
      var t = ev.target;
      if (!t || !t.closest) return;
      var item = t.closest('[data-med]');
      if (item) { openSheet(item.getAttribute('data-med')); return; }
      if (t.closest('#btnAdd')) { openSheet(null); return; }
      if (t.closest('#btnDropLegacy')) { dropLegacySamples(); return; }
    });

    /* ---- Android 返回键 / 侧滑返回 ----
     * 不注册这个监听，侧滑返回就没反应；浮层开着时更是出不来。
     * 事件注册在 platform/lifecycle.js；「关浮层 → 回今天 → 退出」的判定仍在
     * ui/overlay.js 的 backAction()（纯函数、可单测），这里只把两者接上。 */
    onBackButton(handleBackButton);

    /* Esc 关闭浮层。只对「非打断式」浮层生效：
     * 提醒弹窗仍要求用户明确选择「已服用 / 稍后」，不能被一键抹掉。 */
    document.addEventListener('keydown', function (e) {
      if (e.key === 'Tab') { trapTab(e); return; }
      if (e.key !== 'Escape' && e.key !== 'Esc') return;
      /* 与返回键共用同一套「关最上层」逻辑 */
      if (closeTopDialog()) return;
      if ($('#sheetMed').classList.contains('show')) closeSheet();
    });

    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.init(onNotifyAction);
      /* 启动先彻底清场：把插件持久化存储里遗留的通知（含手机重启后会被插件
       * 「复活」的僵尸）全部取消，再排今天的。doSync 内部会等这个 Promise 完成，
       * 所以这里不必关心先后顺序。 */
      window.MedNotify.purge();
      refreshPerm();
    } else {
      // 浏览器模式过去从不检查权限，导致被拒后没有任何提示
      initBrowserPerm();
    }
    /* 固定时刻模式的药**不等打卡**，启动就要把今天的时刻排出来 ——
     * 否则第一个时刻的提醒永远不会响（那时用户还没打过卡）。 */
    ensureFixedDoses();
    /* ★ 2026-09-29：启动时若 localStorage 里还留着「拍照留痕」，说明上次拍照期间
     * 本 App 被系统杀掉、`appRestoredResult` 还没回放。先把界面切到「正在恢复」，
     * 并挡住新的拍照打卡 —— 否则用户会看到"今天还没打卡"而重复点（真机反馈）。
     * 留痕会在 resumeShot 里被清掉；万一事件迟迟不来（极少数），下一次正常
     * photoGate 也会通过 clearPendingShot 覆盖它，不会永久卡住。 */
    if (loadPendingShot()) enterRestore();
    render();
    /* 兜底：留痕可能是"上次拍照被取消/失败后没清干净"的残留（正常路径都会清，
     * 但异常路径不敢保证）。给它一个上限 —— 超过 30 秒没有任何恢复事件到达，
     * 就清掉留痕、解除锁定，别让用户永远点不了打卡。 */
    if (restoringShot) {
      setTimeout(function () {
        if (!restoringShot) return;              // 已被 resumeShot 正常处理
        leaveRestore();                          // 内含「复位恢复态 + 清留痕」两件事
        render();
      }, 30000);
    }
    syncNotifications();
    setTab('today');
    tick();
    setInterval(tick, 1000);
    setInterval(refreshPerm, 5000);   // 用户可能刚去系统设置里改了权限，回到前台要能自动反映
    /* 从系统设置页回来时立刻重查一次。只靠 5 秒轮询最坏要等 5 秒，用户会以为「改了没用」；
     * 而且 App 在后台时 JS 定时器本就被系统暂停，所以「回到前台」这个时机比轮询更准。 */
    if (window.MedNotify && window.MedNotify.native) {
      onAppStateChange(function (isActive) {
        if (isActive) {
          refreshPerm();
          /* ⚠️ 后台时 JS 定时器被系统暂停 —— 过夜后回到前台是第一现场，
           * 必须在这里补一次跨天处理（否则今天没有剂量，见 rollDayIfNeeded 的注释）。 */
          rollDayIfNeeded();
        } else if (window.MedAutoBackup) {
          /* 切到后台时把还没写的备份立刻落盘 —— 防抖窗口内被杀掉就白改了 */
          window.MedAutoBackup.flush();
        }
      });
    }

    // 跨天自动刷新（与「回到前台」共用同一套处理，见 rollDayIfNeeded）
    /* ⚠️ 原来是 `lastDay = todayKey();`。lastDay 搬进 ui/actions.js 之后
     * import 绑定只读，直接赋值会 TypeError —— 改走语义化的 markToday()。 */
    markToday();
    setInterval(rollDayIfNeeded, 30000);

    /* PWA。注册失败**不能静默** —— 静默的后果是"以为有离线缓存、其实没有"，
     * 而 H-2 那个"装了新版仍是旧代码"的问题，正是从"没人知道 SW 在干什么"开始的。
     * 每次 load 都 reg.update() 一次，配合 sw.js 的 network-first，装了新版就能真拿到新版。 */
    if ('serviceWorker' in navigator) {
      window.addEventListener('load', function () {
        navigator.serviceWorker.register('sw.js').then(function (reg) {
          if (reg && reg.update) reg.update();
        }).catch(function (e) {
          console.warn('[sw] Service Worker 注册失败：' + ((e && (e.message || e.name)) || e));
        });
      });
    }
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
  else boot();
})();
