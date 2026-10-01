/* 定时服药提醒 — app logic
 * 打卡制：每天第一次服药打卡后，按药品间隔自动排程当天剩余提醒。
 */

import { $, $$, pad, uid, esc, fmtDate, nowMin, minToStr, minOfDay,
         dateAt, todayKey, copyText, legacyCopy } from './core/util.js';
/* ⬆ 2026-09-24 架构重构：这些工具函数已抽到 core/util.js。
 * import 必须在模块顶层 —— 所以它在 IIFE 外面，IIFE 内部靠闭包可见。
 * 其余代码一字未改。 */


import { KEY, DEFAULT, load, S, isQuotaError, STORAGE_BUDGET, storageStats, gcNotified, cleanTargets, cleanPreview, readJSON, writeJSON, photoStats, setPhotoStats, storageError, save, setSaveHooks } from './core/store.js';


import { intervalLabel, medMode, normTimes, needRebuildDoses, timeInputToMin,
         todayDoses, medById, todayDoseCount, findDoseById, sortedDoses, dueAt,
         nextPending, progress, isMissed, missedDoses, silenceOverdue,
         checkIn, checkInAll, markTaken, ensureFixedDoses, rebuildTodayDoses,
         snoozeDose, snoozeToast, droppedToday, droppedAcked, ackDropped,
         MAX_TIMES, MISS_GRACE_MIN } from './core/schedule.js';


/* ⬆ 2026-09-28 架构重构：浮层原语与返回键判定已抽到 ui/overlay.js。
 * openCount / lastFocus / confirmCb 由它自己管 —— 本文件只读、或走它给的小 API 写。 */
import { setScrim, sheetSaveBegin, sheetSaveReset, askConfirm,
         openDlg, closeDlg, closableDialogs, trapTab, backAction,
         isOverlayOpen, enterOverlay, leaveOverlay,
         rememberFocus, restoreFocus, clearConfirmCb, takeConfirmCb } from './ui/overlay.js';

/* ⬆ 2026-10-01 架构重构（多平台准备）：**平台生命周期**已抽到 platform/lifecycle.js。
 * 这里原本散着 3 处 `window.Capacitor` 直取（backButton / appRestoredResult / appStateChange），
 * 现在统一走本模块的回调注入 —— 业务层不再认识 Capacitor。
 * `exitApp` 也一并移过去了（它原本在 ui/overlay.js，但那是平台操作、不是浮层原语）。 */
import { exitApp, onBackButton, onCameraRestored, onAppStateChange } from './platform/lifecycle.js';

(function () {
  'use strict';

  /* ---------------- 上层钩子注入（core 层不许反向 import） ----------------
   * save() 住在 core/store.js，但它写完盘要做三件属于「上层」的事：
   *   ① 刷新存储告警卡片   ② 同步系统通知   ③ 触发自动备份
   * 把这三件事注册进去 —— **必须在任何一次 save() 之前**，所以放在最前面。
   * 三个被引用的函数都是本 IIFE 内的函数声明（hoisted），此处只是取引用、不调用。 */
  setSaveHooks({
    refresh: queueStorageRefresh,
    notify: syncNotifications,
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
  var APP_VERSION = '1.4.13';
  var APP_BUILD = '2026-10-01';








  /* 服药方式的统一文案（药品卡 / 药品列表 / 提醒弹窗共用） */
  function medScheduleLabel(m) {
    if (medMode(m) === 'fixed') {
      var ts = normTimes(m && m.times);
      return ts.length ? ('每天 ' + ts.map(minToStr).join(' / ')) : '未设时刻';
    }
    return '每 ' + intervalLabel(m && m.interval);
  }

  /* 药品卡下面那行说明 */
  function medMetaText(m) {
    var n = todayDoseCount(m.id);
    if (n) return '今日 ' + n + ' 次 · 已排程';
    if (medMode(m) === 'fixed') {
      return normTimes(m.times).length ? '今天还没排程 · 回「今天」页刷新' : '还没设时刻，去编辑里加一个';
    }
    return '每 ' + intervalLabel(m.interval) + ' · 打卡后开始计时';
  }

  /* 展示用：已服用的剂量显示**真实打卡时刻**（takenAt），未服用/跳过显示计划时刻。
   * 旧记录没有 takenAt（此字段 2026-09-14 前只写不读），用 ≈ 标出这是按计划推定、非真实打卡。 */
  function doseClockHtml(ds) {
    if (ds.status === 'taken' && ds.takenAt) return minToStr(minOfDay(ds.takenAt));
    if (ds.status === 'taken') return '<span title="旧记录：按计划时刻推定，非真实打卡时刻">≈' + minToStr(ds.time) + '</span>';
    return minToStr(ds.time);
  }


  var notifyPerm = 'unknown';   // granted / denied / unsupported / unknown

  /* ---------------- 存储安全（D-1） ----------------
   * 原写法是 `try { setItem } catch (e) { /* ignore *\/ }` —— 配额满时一句话都不说。
   * 对服药记录来说这比崩溃更糟：用户以为记录还在，其实最近的改动早已没了。
   *
   * 现在把失败做成**持续可见的状态**：只要写不进去，界面上就一直挂着，直到恢复。
   * save() 与其失败语义见 core/store.js（2026-09-28 搬过去）——
   * 本文件只保留**界面刷新**这一环；storageError 走 import 的活绑定读取，自动最新。 */
  var storageRefreshQueued = false;


  /* save() 会在 render 过程中被调用，不能同步再 render（会递归）—— 排队到下一轮事件循环。
   * first 判断（在 store 内）保证失败状态持续存在时不会反复排队，避免异步死循环。 */
  function queueStorageRefresh() {
    if (storageRefreshQueued) return;
    storageRefreshQueued = true;
    setTimeout(function () { storageRefreshQueued = false; render(); }, 0);
  }

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

  /* ---------------- 字号缩放 ----------------
   * 用独立的 localStorage key，不写进主状态 —— 这样备份格式（BACKUP_VERSION 1）完全不用动，
   * 也不会影响导入校验。
   * 实现：只改 CSS 变量 --fs；CSS 里所有 font-size/line-height 都写成 calc(Npx * var(--fs))，
   * 而 padding/margin 保持 px 不变 —— 所以只有文字变大，布局骨架不动。 */
  var FS_KEY = 'medreminder.fontScale.v1';
  var FS_LEVELS = [
    { label: '小', v: 0.9 },
    { label: '标准', v: 1 },
    { label: '大', v: 1.15 },
    { label: '特大', v: 1.3 }
  ];
  var fontScale = 1;

  function loadFontScale() {
    var raw = null;
    try { raw = localStorage.getItem(FS_KEY); } catch (e) { /* ignore */ }
    var v = parseFloat(raw);
    for (var i = 0; i < FS_LEVELS.length; i++) if (FS_LEVELS[i].v === v) return v;
    return 1;   // 默认「标准」，与改造前逐像素一致
  }
  function applyFontScale(v, persist) {
    fontScale = v;
    document.documentElement.style.setProperty('--fs', String(v));
    if (persist) { try { localStorage.setItem(FS_KEY, String(v)); } catch (e) { /* ignore */ } }
  }
  /* ---------------- 首次添加药品时引导授权（F-3） ----------------
   * 原来只有"打卡时"才引导，于是"设完药、还没到打卡点"这段空窗期里，
   * 用户并不知道自己收不到提醒 —— 对提醒类 App 这是最要命的静默失效。
   * 只引导一次：已 granted 不必、已 denied 交给权限卡（反复弹窗只会招人烦）。 */
  var GUIDE_KEY = 'medreminder.notifyGuide.v1';
  function guideNotified() { return readJSON(GUIDE_KEY) === '1'; }
  function markGuided() { writeJSON(GUIDE_KEY, '1'); }

  function guideNotifyOnce() {
    if (guideNotified()) return;
    if (notifyPerm === 'granted') return;      // 已经能收到，别打扰
    if (notifyPerm === 'denied') return;       // 已明确收不到 → 用权限卡引导（那里还能跳系统设置）
    markGuided();
    askNotify();
    toast('顺手把通知权限开一下，锁屏时才能收到提醒');
  }

  /* 一次性提示：把"字可以调大"告诉需要它的人（A-1） */
  var FS_HINT_KEY = 'medreminder.fsHint.v1';
  function fsHintDismissed() {
    try { return localStorage.getItem(FS_HINT_KEY) === '1'; } catch (e) { return true; }
  }
  function dismissFsHint() {
    try { localStorage.setItem(FS_HINT_KEY, '1'); } catch (e) { /* ignore */ }
  }
  function fsHintHtml() {
    return '<button class="card" id="btnFsHint" style="display:flex;flex-direction:column;gap:6px;width:100%;cursor:pointer;text-align:left;-webkit-tap-highlight-color:transparent">'
      + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs))">字太小看不清？</span>'
      + '<span class="meta">到「记录」页可以把字调大（大 / 特大），布局不会乱。点这里去看看。</span></button>';
  }

  function fsLabel() {
    for (var i = 0; i < FS_LEVELS.length; i++) if (FS_LEVELS[i].v === fontScale) return FS_LEVELS[i].label;
    return '标准';
  }

  function syncNotifications() {
    if (!window.MedNotify) return;
    var list = todayDoses()
      .filter(function (d) { return d.status === 'pending'; })
      .map(function (d) {
        var m = medById(d.medId);
        /* 系统通知的**响铃时刻**用 dueAt（延后过就按延后时刻），
         * 但正文里仍写计划时刻 —— 用户关心的是"这次药本来该几点吃"。
         * 延后过则把延后时刻一并说清，避免"通知怎么晚响了"的困惑。 */
        var t = minToStr(d.time);
        if (d.snoozeUntil != null) t += '（已延后至 ' + minToStr(d.snoozeUntil) + '）';
        return { id: d.id, timeStr: t, medName: m ? m.name : '服药', at: dateAt(dueAt(d)) };
      });
    window.MedNotify.sync(list);
  }




  /* 旧版本（≤ 2026-09-14）首次启动会写入两条示例药品，让新用户误以为那是自己的药。
   * 这里只「识别 + 交给用户一键删除」，绝不静默删除 —— 万一同名的是真实药品，静默删掉就是数据事故。 */
  var LEGACY_SAMPLE_NAMES = ['维生素 D3', '阿莫西林'];
  function legacySampleMeds() {
    var used = {};
    Object.keys(S.doses).forEach(function (k) {
      (S.doses[k] || []).forEach(function (d) { used[d.medId] = 1; });
    });
    return S.meds.filter(function (m) {
      return LEGACY_SAMPLE_NAMES.indexOf(m.name) >= 0 && !used[m.id];
    });
  }

  /* 删除识别出来的示例药品（用户在这张卡上点过才调用，不是自动清） */
  function dropLegacySamples() {
    var ids = legacySampleMeds().map(function (m) { return m.id; });
    if (!ids.length) return;
    S.meds = S.meds.filter(function (m) { return ids.indexOf(m.id) < 0; });
    save(); render();
    toast('已删除 ' + ids.length + ' 个示例药品');
  }






  function droppedCardHtml() {
    var n = droppedToday();
    if (!n || droppedAcked()) return '';
    return '<div class="card card-miss" style="display:flex;flex-direction:column;gap:6px">'
      + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:#FF7A17">有 ' + n
      + ' 次服药排到了次日，今天不再提醒</span>'
      + '<span class="meta">这次打卡比较晚，按间隔本该排到明天凌晨。今天不会再响，建议明早打卡后补记一次。</span>'
      + '<button class="btn btn-ghost" id="btnDropAck" style="align-self:flex-start;min-height:44px;margin-top:2px">知道了</button>'
      + '</div>';
  }


  /* 顺延后重排序号，保证「第 N / 共 M 次」仍然正确 */
  /* 删除药品。两件事缺一不可：
   *   ① **撤掉今天该药已经排进系统的通知**。旧实现只清了 App 内的剂量数组，
   *      系统闹钟照旧会响；点开时那一刻 doseId 已经查不到了（onNotifyAction 里
   *      `if (!ds) return;`）—— 表现就是「点了没反应」。
   *   ② **保留历史记录**。吃过药的事实不该因为药品被删掉而消失，
   *      否则按药统计和依从率会凭空变好看。 */
  function deleteMed(id) {
    var med = medById(id);
    if (!med) return null;
    var todays = todayDoses().filter(function (d) { return d.medId === id; });
    todays.forEach(function (d) { if (window.MedNotify) window.MedNotify.cancelOne(d.id); });
    S.meds = S.meds.filter(function (m) { return m.id !== id; });
    S.doses[todayKey()] = todayDoses().filter(function (d) { return d.medId !== id; });
    return { name: med.name, doses: todays.length };
  }



  /* 跨天处理 —— **三个入口共用**：启动后、30 秒轮询、以及**从后台回到前台**。
   * ⚠️ 最后一处最容易被漏掉：App 在后台时 JS 定时器被系统暂停，后台过夜就收不到轮询；
   * 回到前台若不补这一下，今天一条剂量都不会生成 —— 用户此时点打卡，就会走进
   * checkIn 的「从此刻起按间隔排」，表现为「固定时刻被顺延」（2026-09-28 真机反馈）。 */
  var lastDay = null;
  function rollDayIfNeeded() {
    if (todayKey() === lastDay) return false;
    lastDay = todayKey();
    S.notified = {};
    save();
    ensureFixedDoses();      // 新的一天，固定时刻要重新排一遍
    render();
    syncNotifications();
    return true;
  }


  function takenToast(med, takenMs, r) {
    var s = '已记录 ' + minToStr(minOfDay(takenMs)) + ' · ' + (med ? med.name : '');
    if (r && r.shifted) s += '，后续 ' + r.shifted + ' 次已顺延';
    if (r && r.dropped) s += '，' + r.dropped + ' 次越过零点不再提醒';
    return s;
  }

  /* 已延后的剂量在排程行上标出来。不标的话，用户会以为"刚才明明延后了，
   * 怎么列表还是原来的时间"——其实计划时刻本就该保持原样，是响铃时刻变了。 */
  function snoozeTag(ds) {
    if (!ds || ds.status !== 'pending' || ds.snoozeUntil == null) return '';
    return '<span class="snooze-tag">延后至 ' + esc(minToStr(ds.snoozeUntil)) + '</span>';
  }


  /* 本月拍照打卡统计。跳过率是这个功能最关键的观察指标：
   * 跳过率很高说明要么场景确实拍不了，要么用户不接受这个设计 ——
   * 用它来决定该收紧还是放宽，而不是一开始就锁死。 */
  function photoTally() {
    var d = new Date();
    var prefix = d.getFullYear() + '-' + pad(d.getMonth() + 1);
    var shot = 0, skipped = 0;
    Object.keys(S.doses || {}).forEach(function (k) {
      if (k.indexOf(prefix) !== 0) return;
      (S.doses[k] || []).forEach(function (x) {
        if (x.status !== 'taken') return;
        if (x.photo) shot++;
        else if (x.photoSkipped) skipped++;
      });
    });
    return { shot: shot, skipped: skipped };
  }

  /* 打开一张服药照片 */
  function openPhoto(id) {
    var ds = findDoseById(id);
    if (!ds || !ds.photo) return;
    var med = medById(ds.medId);
    $('#viewMeta').textContent = minToStr(ds.time) + (med ? ' · ' + med.name : '');
    var hint = $('#viewHint');
    hint.textContent = '';
    if (ds.takenAt) {
      var t = new Date(ds.takenAt);
      hint.textContent = '打卡于 ' + t.getFullYear() + '-' + pad(t.getMonth() + 1) + '-' + pad(t.getDate())
        + ' ' + pad(t.getHours()) + ':' + pad(t.getMinutes());
    }
    var img = $('#viewImg');
    img.removeAttribute('src');
    openDlg($('#dlgView'));
    window.MedPhoto.src(ds.photo).then(function (u) {
      if (u) { img.src = u; }
      else { hint.textContent = '照片文件找不到了（可能已被系统清理）。'; }
    });
  }

  /* ---------------- 拍照打卡（G-1） ----------------
   * 目标不是"防遗忘"，而是**防「随手划掉提醒、假装吃过」**。
   * 现在通知上的「已服用」一秒就能点掉 —— 没吃药也能清掉提醒，App 还记一笔已服用。
   * 这叫「假依从」，比漏服更难发现：漏服至少记录是空的，假依从连数据都是假的。
   * 拍照把"清掉提醒"从零成本变成有成本。
   *
   * 逃生通道按已拍板方案 B：**拍不了可以跳过，打卡照常完成，但记录留痕并计入跳过率**。
   * 为什么不能硬性阻断：现实中拍不了的场景很多（在外面 / 开会中 / 药盒没带 / 光线太暗 /
   * 存储满 / 相机权限被误关 / 老人不会用相机）。硬拦会让这些用户只能强停 App，
   * 数据直接断掉 —— 比不强制更糟。
   *
   * 浏览器预览模式没有相机插件：不假装能拍，直接放行，也**不计跳过**
   * （那是环境不支持，不是用户偷懒，混进跳过率会让指标失真）。
   */
  var PENDING_KEY = 'medreminder.pendingPhoto.v1';   // 独立 key，不碰主状态，备份格式不用动
  var pendingShot = null;                            // { kind:'all'|'one', doseId, onDone }

  /* ★ 2026-09-29 真机反馈：「拍完了还让我再拍一次」。
   *
   * 根因：相机是**独立 Activity**，低端机 / 内存紧张时系统会在它运行期间**杀掉本 App**
   * （用户体感就是"卡顿"）。Capacitor 把结果存起来，重启后经 `appRestoredResult` 回放。
   * 于是出现一个**几分钟的空窗**：
   *   ① 重启后 `todayDoses()` 还是空的 → 「今天」页显示「今天还没打卡」+ 打卡按钮；
   *   ② 用户看到"没成功"，又点了一次打卡 → 又弹出拍照框（"又提示要拍照"）；
   *   ③ 与此同时 `appRestoredResult` 到达 → 照片落盘 + 打卡完成（"拍照已经成功了"）。
   *
   * 修法：重启时**只要 localStorage 里还有未消费的拍照留痕**，就把界面切到
   * 「正在恢复上次拍照」的诚实状态，并在恢复结束前**拒绝**任何新的拍照打卡
   * —— 既不让用户白点，也不让两次 photoGate 互相覆盖 pendingShot。 */
  var restoringShot = false;                         // 恢复进行中：界面据此改文案、打卡按钮据此拒绝

  function savePendingShot(kind, doseId) {
    try { localStorage.setItem(PENDING_KEY, JSON.stringify({ kind: kind, doseId: doseId || null, at: Date.now() })); } catch (e) { /* ignore */ }
  }
  function loadPendingShot() {
    try { var r = localStorage.getItem(PENDING_KEY); return r ? JSON.parse(r) : null; } catch (e) { return null; }
  }
  function clearPendingShot() {
    try { localStorage.removeItem(PENDING_KEY); } catch (e) { /* ignore */ }
  }

  /* 打卡前先过一道拍照。opts = { desc, kind:'all'|'one', doseId }
   * onDone(photoRel, skipped)：photoRel 为 null 表示这次没有照片。 */
  function photoGate(opts, onDone) {
    if (!(window.MedPhoto && window.MedPhoto.ready())) { onDone(null, false); return; }
    /* ★ 恢复中不许再开一个新的拍照会话 —— 否则会覆盖 pendingShot，
     * 让真正要恢复的那次打卡彻底对不上号（真机"又让我拍一次"的放大器）。 */
    if (restoringShot) { toast('上次的拍照还在处理，请稍等一下'); return; }
    if (pendingShot) { toast('拍照正在进行中，请稍等一下'); return; }
    pendingShot = { kind: opts.kind, doseId: opts.doseId || null, onDone: onDone };
    savePendingShot(opts.kind, opts.doseId);   // 相机 Activity 可能把 App 挤掉，先留痕
    var body = $('#photoBody');
    if (body) body.textContent = (opts.desc ? opts.desc + ' ' : '')
      + '拍下这次的药，之后能回看确认。实在拍不了可以跳过，但记录里会标出来。';
    if (pendingShot) pendingShot.body = body ? body.textContent : '';   // 重拍时用来复位提示
    var take = $('#photoTake');
    if (take) { take.disabled = false; take.textContent = '拍照打卡'; }
    openDlg($('#dlgPhoto'));
  }

  function finishShot(rel, skipped) {
    var p = pendingShot;
    pendingShot = null;
    clearPendingShot();
    closeDlg($('#dlgPhoto'));
    if (p) p.onDone(rel, skipped);
  }

  /* 把照片路径 / 跳过标记写到一批剂量上 */
  function stampPhoto(doses, rel, skipped) {
    (doses || []).forEach(function (d) {
      if (!d) return;
      if (rel) d.photo = rel;
      if (skipped) d.photoSkipped = 1;
    });
  }

  /* App 在拍照期间被系统杀掉 → 恢复时把这次打卡接上。
   * Capacitor 官方明确要求监听 appRestoredResult，不处理会**同时丢照片和打卡**。 */
  function resumeShot(photoPath) {
    var p = loadPendingShot();
    clearPendingShot();
    if (!p) return;                       // 不是在打卡流程里被杀掉的，不插手
    if (!(window.MedPhoto && window.MedPhoto.ready())) return;

    restoringShot = true;                 // 恢复期间：挡住新的拍照打卡，界面显示"正在恢复"
    render();
    window.MedPhoto.fromRestored(photoPath, p.doseId || 'restored').then(function (r) {
      restoringShot = false;
      if (!r.ok) { render(); toast('照片没保存下来，这次打卡请重新点一下'); return; }
      if (p.kind === 'all') {
        var created = checkInAll();
        stampPhoto(created, r.rel, false);
        save(); render();
        toast('拍照已完成，今天的打卡已记上');
        return;
      }
      /* ★ kind === 'one'：这里的 p.doseId 实际是**药品 id**（photoGate 的调用方
       * 从 data-checkin 拿到的就是 med.id —— 见 bindToday）。原来错当成 dose.id 去
       * todayDoses() 里找，永远找不到 → 静默丢打卡、照片变孤儿。
       * 正确做法与 checkIn 一致：按 medId 走一次正常打卡流程。 */
      var arr = checkIn(p.doseId);
      if (arr) {
        stampPhoto(arr, r.rel, false);
        save(); render();
        var med0 = medById(p.doseId);
        toast((med0 ? med0.name : '') + ' 已打卡 · 排了 ' + arr.length + ' 次提醒');
        return;
      }
      /* checkIn 返回 null 有两种：今天已排过（也算成功，把照片挂到今天的剂量上）、
       * 或药品已被删除。前者把照片补挂上，别让它变孤儿。 */
      var l = todayDoses(), hit = null;
      for (var i = 0; i < l.length; i++) if (l[i].medId === p.doseId) { hit = l[i]; break; }
      if (hit) { stampPhoto([hit], r.rel, false); save(); render(); toast('拍照已完成，今天的打卡已记上'); return; }
      render();
    });
  }

  /* 照片目录占用 —— 不清点它，D-1 的存储卡就会漏报照片空间，数字变成错的 */
  function refreshPhotoStats() {
    if (!(window.MedPhoto && window.MedPhoto.ready())) return;
    window.MedPhoto.dirStats().then(function (s) {
    if (s.files === photoStats.files && s.bytes === photoStats.bytes) return;
    setPhotoStats(s);       // 走 setter：photoStats 现在住在 core/store.js（import 绑定只读）
      queueStorageRefresh();
    });
  }

  /* 当前所有被记录引用的照片路径 —— 供孤儿回收判断"谁还活着" */
  function allPhotoRefs() {
    var refs = [];
    Object.keys(S.doses || {}).forEach(function (k) {
      (S.doses[k] || []).forEach(function (d) { if (d.photo) refs.push(d.photo); });
    });
    return refs;
  }







  /* ---------------- toast ---------------- */
  var toastTimer = null;
  function toast(msg) {
    var el = $('#toast');
    el.textContent = msg;
    el.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function () { el.classList.remove('show'); }, 2000);
  }

  /* ---------------- icons ---------------- */
  var ICON = {
    check: '<svg width="14" height="14" viewBox="0 0 24 24" fill="none"><path d="M4.5 12.5 9.5 17.5 19.5 7" stroke="#7D8187" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
    pill: '<svg width="18" height="18" viewBox="0 0 24 24" fill="none"><rect x="3" y="9" width="18" height="6" rx="3" transform="rotate(-45 12 12)" stroke="currentColor" stroke-width="1.8"/><path d="M9 9l6 6" stroke="currentColor" stroke-width="1.8"/></svg>',
    chev: '<svg width="16" height="16" viewBox="0 0 24 24" fill="none"><path d="M9.5 5.5 16 12l-6.5 6.5" stroke="#7D8187" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>',
    cam: '<svg width="15" height="15" viewBox="0 0 24 24" fill="none"><path d="M3.5 8.5c0-1.1.9-2 2-2h1.6c.6 0 1.1-.3 1.4-.8l.5-.9c.3-.5.8-.8 1.4-.8h4.2c.6 0 1.1.3 1.4.8l.5.9c.3.5.8.8 1.4.8h1.6c1.1 0 2 .9 2 2v8c0 1.1-.9 2-2 2H5.5c-1.1 0-2-.9-2-2v-8Z" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/><circle cx="12" cy="12.5" r="3.4" stroke="currentColor" stroke-width="1.7"/></svg>'
  };

  /* ---------------- render: TODAY ---------------- */
  function medCardHtml(m, clickable) {
    return '<' + (clickable ? 'button' : 'div') + ' class="card medcard' + (clickable ? ' tappable' : '') + '"'
      + (clickable ? ' data-checkin="' + m.id + '"' : '')
      + ' style="display:flex;flex-direction:column;gap:14px;text-align:left;color:inherit;font:inherit">'
      + '<div class="card-row">'
      + '<span style="font-size:calc(16px * var(--fs));line-height:calc(22px * var(--fs))">' + esc(m.name) + '</span>'
      + '<span class="pill">' + esc(medScheduleLabel(m)) + '</span>'
      + '</div>'
      + '<p class="meta">' + esc(medMetaText(m)) + '</p>'
      + '</' + (clickable ? 'button' : 'div') + '>';
  }

  function renderToday() {
    var host = $('#todayView');
    var list = todayDoses();
    var html = '';

    // header row
    var d = new Date();
    var wk = ['日', '一', '二', '三', '四', '五', '六'][d.getDay()];
    html += '<div class="card-row" style="padding:0 0 4px">'
      + '<span class="meta">' + (d.getMonth() + 1) + '月' + d.getDate() + '日 周' + wk + '</span>'
      + '<span class="eyebrow">MED TRACKER</span></div>';

    /* 存储写失败比收不到提醒更严重 —— 前者意味着数据正在丢，所以排在权限提示前面 */
    html += storageAlertHtml();

    /* 权限提示：原生与浏览器两种模式下都可能有权限问题，
     * 之前只在原生 + denied 时提示，PWA 用户被拒授权后既收不到提醒也无任何说明。 */
    var permCard = permCardHtml();
    if (permCard) html += permCard;

    /* 字号可调，但入口在「记录」页 —— 真正需要它的人（尤其上了年纪的）不会去找。
     * 首次启动提示一次，点过永不再出现；字号已不是标准值时也不提示。 */
    if (fontScale === 1 && !fsHintDismissed()) html += fsHintHtml();

    if (!list.length) {
      // ---- 未打卡 ----
      /* ★ 恢复中（拍照期间被系统杀掉、正等 appRestoredResult 回放）：
       * 必须**诚实显示**，否则用户看到"今天还没打卡"会以为刚才白拍了，
       * 又去点一次 → 正是真机反馈的"又提示我拍照"。 */
      if (restoringShot) {
        html += '<div class="sect" style="gap:10px">'
          + '<span class="eyebrow">TODAY</span>'
          + '<h1 class="h1">正在恢复上次拍照…</h1>'
          + '<p class="body">刚才拍的照片还在处理，马上就把今天的打卡记上。请稍等片刻，不用再点一次。</p>'
          + '</div>';
        host.innerHTML = html;
        bindToday();
        return;
      }
      html += '<div class="sect" style="gap:10px">'
        + '<span class="eyebrow">TODAY</span>'
        + '<h1 class="h1">今天还没打卡</h1>'
        + '<p class="body">每天第一次服药后点下方按钮打卡，我们会按你设定的间隔，依次提醒今天的每一次。</p>'
        + '</div>';

      if (!S.meds.length) {
        // ---- 首次使用：空态引导（此处过去会预置两条示例药品，已移除）----
        html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
          + '<span class="eyebrow">HOW IT WORKS · 三步</span>'
          + '<p class="body" style="color:var(--sub)">1 · 在「药品」页添加药名和服药间隔</p>'
          + '<p class="body" style="color:var(--sub)">2 · 每天第一次服药后，点「打卡」</p>'
          + '<p class="body" style="color:var(--sub)">3 · 之后按间隔自动提醒，锁屏、息屏也会响</p>'
          + '<p class="meta" style="margin-top:2px">注意：不打卡就不会有提醒 —— 打卡是当天排程的起点。</p>'
          + '</div>'
          + '<button class="btn btn-primary" id="btnFirstMed">添加第一个药品</button>';
      } else {
        html += '<div class="sect">'
          + '<span class="eyebrow">MY MEDS</span>'
          + '<div class="list">';
        S.meds.forEach(function (m) { html += medCardHtml(m, true); });
        html += '</div>';
        if (S.meds.length > 1) html += '<p class="hint" style="margin-top:-4px">点击卡片可只打卡某一种药。</p>';
        html += '</div>';

        html += '<div style="padding-top:4px">'
          + '<button class="btn btn-primary" id="btnCheckin">打卡 · 开始今天的提醒</button>'
          + '</div>';
      }
    } else {
      // ---- 已打卡 ----
      var p = progress();
      var nxt = nextPending();
      var sorted = sortedDoses();

      html += '<div class="sect" style="gap:10px">'
        + '<span class="eyebrow">NEXT DOSE · ' + p.done + ' / ' + p.total + '</span>'
        + '<h1 class="h1 num" style="font-size:calc(40px * var(--fs));line-height:calc(44px * var(--fs))">' + (nxt ? minToStr(nxt.time) : '已完成') + '</h1>'
        + '<p class="body" id="countdown">' + countdownText(nxt) + '</p>'
        + '</div>';

      // 漏服可见化：显式告诉用户「漏了几次」，并给一键补记入口
      var missed = missedDoses();
      if (missed.length) {
        html += '<button class="card card-miss" id="btnMissAll" style="display:flex;flex-direction:column;gap:6px;width:100%;cursor:pointer;-webkit-tap-highlight-color:transparent">'
          + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:#FF7A17">今天漏了 ' + missed.length + ' 次服药</span>'
          + '<span class="meta">' + missed.map(function (d) { return minToStr(d.time); }).join('、')
          + ' 这几次没有打卡记录。已经吃过就点这里补记，没吃就留意一下。</span>'
          + '<span class="meta" style="color:#FF7A17">点此把漏掉的都记为已服用</span></button>';
      }

      html += droppedCardHtml();

      html += '<div class="card sched" style="padding-left:20px;padding-right:20px;padding-top:8px;padding-bottom:8px">';
      sorted.forEach(function (ds) {
        var med = medById(ds.medId);
        var isNext = nxt && ds.id === nxt.id;
        var tCls = isNext ? 'dose-t now' : 'dose-t';
        var nCls = ds.status === 'taken' ? 'dose-n' : (isNext ? 'dose-n on' : 'dose-n dim');
        var right;
        if (ds.status === 'taken') {
          /* 拍过照 → 一个可点的相机图标（点开看图）；主动跳过的 → 明确标「未拍照」。
           * 留痕是方案 B 的核心：跳过可以，但别想不留痕迹。 */
          var mark = ds.photo
            ? '<button class="cam-btn" data-photo="' + esc(ds.id) + '" aria-label="查看这次服药的照片">' + ICON.cam + '</button>'
            : (ds.photoSkipped ? '<span class="txt no-shot">未拍照</span>' : '');
          right = '<span class="dose-s">' + ICON.check + '<span class="txt">已服用</span>' + mark + '</span>';
        } else if (ds.status === 'skipped') {
          right = '<span class="dose-s"><span class="txt">已跳过</span></span>';
        } else if (isMissed(ds)) {
          // 已错过：不只是文案，给一个能补记的按钮，别让用户没法挽救
          right = '<button class="btn-miss" data-makeup="' + esc(ds.id) + '" '
            + 'aria-label="补记 ' + esc(minToStr(ds.time)) + ' 这次服药">已错过 · 补记</button>';
        } else if (isNext) {
          right = snoozeTag(ds) + '<span class="pill accent">待服用</span>';
        } else {
          right = snoozeTag(ds) + '<span class="dose-s"><span class="txt">待服用</span></span>';
        }
        var rowMiss = isMissed(ds) ? ' dose-miss' : '';
        html += '<div class="dose' + rowMiss + '">'
          + '<div class="dose-l"><span class="' + tCls + '">' + doseClockHtml(ds) + '</span>'
          + '<span class="' + nCls + '">' + esc(med ? med.name : '已删除药品') + '</span></div>'
          + right + '</div>';
      });
      html += '</div>';
      if (sorted.some(function (ds) { return ds.status === 'taken' && !ds.takenAt; })) {
        html += '<p class="hint" style="margin-top:-4px">带 ≈ 的时刻是旧记录，按当时的计划时刻推定，不是真实打卡时刻。</p>';
      }

      // 今天还没打卡的药
      var unchecked = S.meds.filter(function (m) {
        return !list.some(function (d) { return d.medId === m.id; });
      });
      if (unchecked.length) {
        html += '<div class="sect">'
          + '<span class="eyebrow">NOT CHECKED IN</span>'
          + '<div class="list">';
        unchecked.forEach(function (m) { html += medCardHtml(m, true); });
        html += '</div><p class="hint" style="margin-top:-4px">这些药今天还没打卡，服药后点卡片开始排程。</p></div>';
      }

      if (nxt) {
        html += '<div class="btnrow">'
          + '<button class="btn btn-primary" id="btnEarly">现在服用</button>'
          + '<button class="btn btn-ghost" id="btnSkip">跳过本次</button>'
          + '</div>';
      } else if (!unchecked.length) {
        html += '<p class="body" style="text-align:center">今天的药都吃完了，明天见。</p>';
      }
    }

    host.innerHTML = html;
    bindToday();
  }

  function countdownText(nxt) {
    if (!nxt) return '今天已无待服用的药。';
    var diff = dueAt(nxt) - nowMin();
    if (diff <= 0) return '现在就该吃药了。';
    var h = Math.floor(diff / 60), m = diff % 60;
    return '距下次服药还有 ' + (h ? h + ' 小时 ' + m + ' 分' : m + ' 分钟');
  }

  /* 权限卡点击：**先真正申请一次，拿不到再跳系统设置**。
   *
   * 上一版写的是「denied 就直接跳设置，不再申请」—— 那是错的。
   * Android 官方文档说用户在弹窗选「不允许」与「在系统设置里手动关闭」**效果相似**，
   * 但**再请求时是否弹窗并不等价**：手动关闭相当于 revoke，多数设备上重新申请
   * 仍会弹出授权框。上一版把这条路砍掉了，等于白白丢掉唯一一次能直接授权的机会
   * （秦老师真机反馈：「双清后第一次能授权，手动关闭后再点就拿不到权限」）。
   *
   * 现在的行为：能弹窗的设备一次点击就授权成功；系统性不再弹窗（或用户再次拒绝）时，
   * 自动落到系统设置页 —— 那是唯一 100% 有效的路径。两条路都走通。
   * 抽成具名函数而非内联，是为了这段分支能被测试直接覆盖。 */
  /* 收不到提醒的具体原因 —— 显示出来，用户才知道该去关哪一层开关 */
  function permBlockReason(p) {
    if (!p) return '读取不到状态';
    if (p.display === 'denied') return '权限被拒';
    if (p.enabled === false) return 'App 通知开关已关';
    if (p.channelImportance === 0) return '「服药提醒」渠道被关闭';
    return '收不到提醒';
  }

  /* 点击权限卡。
   *
   * ⚠️ **不能用 requestPermissions() 的返回值判断成败** —— 这是本轮反馈的根因。
   * 插件源码（LocalNotificationsPlugin.requestPermissions）：
   *     if (SDK_INT < TIRAMISU || getPermissionState(...) == GRANTED) {
   *         resolve(当前状态);          // ← 不弹任何框
   *     } else { requestPermissionForAlias(...); }
   * 即 **Android 13 以下永远不弹授权框**；13 以上如果权限已是 granted（只是被关掉了
   * 总开关或本渠道）也不弹。上一版按返回值走：拿到 granted 就宣布「通知已开启」并
   * 撤下卡片 —— 于是用户看到的就是「点了没反应，也不跳设置」，而问题其实还在。
   *
   * 现在每一步都回到**真实状态**复检（probe + isBlocked）：
   *   本就能收到 → 直接说明，不做多余动作
   *   确实收不到 → 先申请一次（能弹框的设备在这一步就解决了）
   *   仍然收不到 → 跳系统设置（唯一 100% 有效的路径）
   *   跳不过去   → 明确写出失败原因与手动路径，绝不「点了没反应」
   */
  function onPermCardTap() {
    if (!(window.MedNotify && window.MedNotify.native)) {
      permAction = '浏览器模式：请在浏览器的网站权限里改通知';
      render();
      toast('浏览器模式无法跳系统设置，请在网站权限里把通知改为「允许」');
      askNotify();
      return;
    }
    permAction = '检测中…';
    render();

    window.MedNotify.probe().then(function (before) {
      if (!window.MedNotify.isBlocked(before)) {
        permProbe = before;
        notifyPerm = before.display === 'granted' ? 'granted' : 'unknown';
        permAction = '复检：本就能收到提醒，无需处理';
        render();
        toast('通知本来就是开着的');
        return null;
      }
      permAction = '确认收不到（' + permBlockReason(before) + '）→ 先申请授权';
      render();

      return window.MedNotify.requestPermission().then(function (p) {
        return window.MedNotify.probe().then(function (after) {
          if (!window.MedNotify.isBlocked(after)) {
            permProbe = after;
            lastProbeSig = JSON.stringify(after);
            notifyPerm = 'granted';
            permAction = '申请成功 · 已授权（' + p + '）';
            render();
            toast('通知已开启');
            return null;
          }
          permAction = '系统未弹授权框（Android 13 以下不会弹）→ 跳系统设置';
          render();
          return window.MedNotify.openSettings().then(function (r) {
            permAction = r.ok ? '已打开系统设置，请把「通知」打开' : ('跳设置失败：' + r.reason);
            render();
            toast(r.ok
              ? '请在系统设置里打开「通知」'
              : '请到「设置 → 应用 → 定时服药提醒 → 通知」手动打开');
          });
        });
      });
    }).catch(function (e) {
      permAction = '出错：' + ((e && e.message) ? e.message : e);
      render();
    });
  }

  function bindToday() {
    var perm = $('#btnPerm');
    if (perm) perm.onclick = onPermCardTap;

    var da = $('#btnDropAck');
    if (da) da.onclick = function () {
      ackDropped();
      render();
      toast('已隐藏，明天若又跨越零点会再提示');
    };

    var fsh = $('#btnFsHint');
    if (fsh) fsh.onclick = function () {
      dismissFsHint();
      setTab('records');
      render();
      toast('字号档位在下面「显示 · 字号」里');
    };

    /* 补记：把「已错过」的剂量记为已服用。
     * 真实服药时刻无从得知（用户是事后补记），所以 takenAt 用「现在」——
     * 显示上会带 ≈ 前缀，与真实打卡区分（doseClockHtml 已支持）。
     * 补记只改状态、不触发顺延：scheduleShift 依赖 taked 顺序，事后补记会把
     * 后续剂量推乱，反而制造虚假排程。 */
    function makeUp(dose, silent) {
      if (!dose || dose.status !== 'pending') return false;
      dose.status = 'taken';
      dose.takenAt = Date.now();
      dose.makeup = true;      // 标记为补记，导出 CSV 时可与真实打卡区分
      if (window.MedNotify) window.MedNotify.cancelOne(dose.id);
      return true;
    }
    $$('[data-makeup]').forEach(function (el) {
      el.onclick = function (ev) {
        ev.stopPropagation();
        var id = el.getAttribute('data-makeup');
        var l = todayDoses(), ds = null;
        for (var i = 0; i < l.length; i++) if (l[i].id === id) { ds = l[i]; break; }
        if (!makeUp(ds)) { toast('这次已经处理过了'); render(); return; }
        save(); render(); syncNotifications();
        toast('已补记 ' + minToStr(ds.time) + ' 这次服药');
      };
    });
    var missAll = $('#btnMissAll');
    if (missAll) missAll.onclick = function () {
      var n = 0;
      missedDoses().forEach(function (d) { if (makeUp(d)) n++; });
      if (!n) { render(); return; }
      save(); render(); syncNotifications();
      toast('已补记 ' + n + ' 次服药');
    };
    $$('[data-checkin]').forEach(function (el) {
      el.onclick = function () {
        var id = el.getAttribute('data-checkin');
        var med = medById(id);
        if (todayDoses().some(function (d) { return d.medId === id; })) { toast('今天已经打过卡了'); return; }
        photoGate({ desc: '这次是：' + (med ? med.name : '服药') + '。', kind: 'one', doseId: id }, function (rel, skipped) {
          askNotify();
          var arr = checkIn(id);
          if (!arr) { render(); toast('今天已经打过卡了'); return; }
          stampPhoto(arr, rel, skipped);
          save(); render();
          toast((med ? med.name : '') + ' 已打卡 · 排了 ' + arr.length + ' 次提醒');
        });
      };
    });
    $$('[data-photo]').forEach(function (el) {
      el.onclick = function (ev) {
        if (ev && ev.stopPropagation) ev.stopPropagation();
        openPhoto(el.getAttribute('data-photo'));
      };
    });

    var b = $('#btnCheckin');
    if (b) b.onclick = function () {
      if (todayDoses().length) { toast('今天已经打过卡了'); return; }
      var names = S.meds.map(function (m) { return m.name; }).join('、');
      photoGate({ desc: '今天要打卡：' + names + '。', kind: 'all' }, function (rel, skipped) {
        askNotify();
        var created = checkInAll();
        stampPhoto(created, rel, skipped);
        save(); render();
        toast(created.length ? '已打卡 · 今天共排了 ' + todayDoses().length + ' 次提醒' : '今天已经打过卡了');
      });
    };
    var fm = $('#btnFirstMed');
    if (fm) fm.onclick = function () { setTab('meds'); openSheet(null); };
    var e = $('#btnEarly');
    if (e) e.onclick = function () {
      var nxt = nextPending(); if (!nxt) return;
      var med = medById(nxt.medId);
      photoGate({ desc: '这次是：' + (med ? med.name : '服药') + '。', kind: 'one', doseId: nxt.id }, function (rel, skipped) {
        stampPhoto([nxt], rel, skipped);
        var at = Date.now();
        var r = markTaken(nxt, at);
        save(); render();
        toast(takenToast(med, at, r));
      });
    };
    var s = $('#btnSkip');
    if (s) s.onclick = function () {
      var nxt = nextPending(); if (!nxt) return;
      var med = medById(nxt.medId);
      var sorted = sortedDoses();
      var after = null;
      for (var i = 0; i < sorted.length; i++) {
        if (sorted[i].id === nxt.id) { for (var j = i + 1; j < sorted.length; j++) { if (sorted[j].status === 'pending') { after = sorted[j]; break; } } break; }
      }
      $('#skipBody').textContent = minToStr(nxt.time) + ' 的' + (med ? med.name : '这次') + '将记为未服用，会影响今日依从率。';
      $('#skipNext').textContent = after ? (minToStr(after.time) + ' · 不变') : '今日无后续提醒';
      pendingSkipId = nxt.id;
      openDlg($('#dlgSkip'));
    };
  }

  /* ---------------- render: MEDS ---------------- */
  function renderMeds() {
    var host = $('#medsView');
    var html = ''
      + '<div class="card-row" style="padding:0 0 4px">'
      + '<h1 class="h1">我的药品</h1>'
      + '<button class="icon-btn" id="btnAdd" aria-label="添加药品">'
      + '<svg width="18" height="18" viewBox="0 0 24 24" fill="none"><path d="M12 5v14M5 12h14" stroke="#fff" stroke-width="1.8" stroke-linecap="round"/></svg>'
      + '</button></div>';

    if (!S.meds.length) {
      html += '<div class="card"><p class="body" style="text-align:center">还没有药品。点右上角 + 添加第一个。</p></div>';
    } else {
      html += '<div class="list">';
      S.meds.forEach(function (m) {
        html += '<button class="meditem" data-med="' + m.id + '">'
          + '<span class="badge badge-36" style="color:#DADBDF">' + ICON.pill + '</span>'
          + '<span class="medinfo"><span class="n">' + esc(m.name) + '</span>'
          + '<span class="m">' + esc(medScheduleLabel(m)) + ' · ' + (todayDoseCount(m.id) ? ('今日 ' + todayDoseCount(m.id) + ' 次') : '未排程') + '</span></span>'
          + ICON.chev + '</button>';
      });
      html += '</div>';
    }
    html += '<p class="hint" style="margin-top:-8px">点任意药品，可以改名称、改服药间隔，或者删掉它。</p>';

    var legacy = legacySampleMeds();
    if (legacy.length) {
      html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
        + '<span class="eyebrow">SAMPLE · 示例数据</span>'
        + '<p class="body">检测到 ' + legacy.length + ' 个从未使用过的示例药品（'
        + esc(legacy.map(function (m) { return m.name; }).join('、'))
        + '）。旧版本首次启动时会自动生成它们，不是你手动添加的。</p>'
        + '<button class="btn btn-ghost" id="btnDropLegacy" style="height:44px;font-size:calc(14px * var(--fs))">删除这 ' + legacy.length + ' 项</button>'
        + '</div>';
    }

    host.innerHTML = html;

    /* ⚠️ 这里**故意不**逐个绑 onclick —— 改由 boot() 里的一次性事件委托处理。
     * 原因见 boot 里那段注释：render() 会把视图 innerHTML 整个重建，
     * 手指落下到 click 之间只要重绘一次，正在点的元素就被换掉 → 点击被吞。 */
  }

  /* ---------------- render: RECORDS ---------------- */
  function renderRecords() {
    var host = $('#recordsView');
    var html = '<h1 class="h1">服药记录</h1>';

    html += storageAlertHtml();

    // week dots
    var d = new Date();
    var dow = (d.getDay() + 6) % 7; // Monday = 0
    var monday = new Date(d.getFullYear(), d.getMonth(), d.getDate() - dow);
    var labels = ['一', '二', '三', '四', '五', '六', '日'];
    html += '<div class="card" style="display:flex;flex-direction:column;gap:18px">'
      + '<span class="eyebrow">THIS WEEK</span><div class="week">';
    for (var i = 0; i < 7; i++) {
      var day = new Date(monday.getFullYear(), monday.getMonth(), monday.getDate() + i);
      var k = fmtDate(day);
      var arr = S.doses[k] || [];
      var taken = arr.some(function (x) { return x.status === 'taken'; });
      var isToday = k === todayKey();
      var cls = isToday ? 'dot today' : (taken ? 'dot fill' : 'dot');
      html += '<div class="day"><span class="lb' + (isToday ? ' on' : '') + '">' + labels[i] + '</span><span class="' + cls + '"></span></div>';
    }
    html += '</div></div>';

    // stats
    var streak = calcStreak();
    var ads = adherenceStats(histMedId);
    var selMed = histMedId ? medById(histMedId) : null;
    html += '<div class="statrow">'
      + '<div class="stat"><span class="eyebrow">STREAK</span><span class="meta">连续打卡</span>'
      + '<span class="v"><span class="n">' + streak + '</span><span class="u">天</span></span></div>'
      + '<div class="stat"><span class="eyebrow">ADHERENCE</span><span class="meta">本月依从率'
      + (selMed ? ' · ' + esc(selMed.name) : '') + '</span>'
      + '<span class="v"><span class="n">' + (ads.rate === null ? '—' : ads.rate) + '</span><span class="u">%</span></span></div>'
      + '</div>';

    /* 明细必须露出来：只说一个百分比，用户没法判断它是怎么来的，
     * 也看不出"补记"被算在了哪里。 */
    if (ads.total) {
      html += '<p class="hint" style="margin-top:-8px">真实打卡 ' + ads.real
        + ' · 补记 ' + ads.makeup + ' · 跳过 ' + ads.skipped + ' · 漏服 ' + ads.missed
        + (ads.makeup ? '（依从率只算真实打卡，补记不计入）' : '') + '</p>';
    }
    html += '<p class="hint" style="margin-top:-8px">每次服药后打卡，记录会自动更新。</p>';

    /* ---- 历史记录：页面只放摘要，完整列表进二级菜单 ----
     * 之前这里把「近 14 天」的每一天都铺成卡片 —— 用得越久越长，翻不到底
     * （2026-09-28 反馈）。现在页面留摘要 + 最近 3 天，剩下的点按钮进二级菜单看。 */
    if (S.meds.length) {
      html += histChipsHtml();
    }
    html += historySummaryHtml();


    /* 拍照打卡统计。跳过率单独列出来 —— 它是这个功能该收紧还是放宽的依据。 */
    var ph = photoTally();
    if (ph.shot + ph.skipped > 0) {
      var tot = ph.shot + ph.skipped;
      html += '<p class="hint" style="margin-top:-8px">本月拍照打卡 ' + ph.shot + '/' + tot + ' 次'
        + (ph.skipped ? (' · 未拍照 ' + ph.skipped + ' 次（' + Math.round(ph.skipped / tot * 100) + '%）') : '')
        + '</p>';
    }

    /* 字号：只放大文字，不动布局。老年人看不清小字是真实痛点，而整页缩放会带来左右拖动。
     * 放在这里而不是做成双指手势 —— 手势缩放文字是非标准交互，且会与列表滚动抢事件；
     * 档位按钮可发现、可预期，也符合「文字可放大到 200% 而不丢内容」的无障碍要求。 */
    html += '<div class="card" style="display:flex;flex-direction:column;gap:12px">'
      + '<div style="display:flex;align-items:baseline;justify-content:space-between;gap:12px">'
      + '<span class="eyebrow">显示 · 字号</span>'
      + '<span class="meta" id="fsNow">' + esc(fsLabel()) + '</span></div>'
      + '<p class="body" style="margin:0">只放大文字，页面布局不变。</p>'
      + '<div class="chip-row" id="fsRow">'
      + FS_LEVELS.map(function (lv) {
          var on = lv.v === fontScale;
          return '<button class="chip fs-chip' + (on ? ' on' : '') + '" data-fs="' + lv.v + '"'
            + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + lv.label + '</button>';
        }).join('')
      + '</div></div>';

    /* 存储状态卡放在备份卡之前：先知道「还剩多少空间」，再决定要不要导出/清理 */
    html += storageCardHtml();

    /* 自动备份卡。放在**手动导出之前** —— 它每天自动发生，是「数据没丢」的主要保障；
     * 手动导出是换机时才用的。局限（卸载会删）必须写在卡片上，不能含糊。 */
    html += autoBackupCardHtml();

    html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
      + '<span class="eyebrow">DATA · 备份</span>'
      + '<p class="body">记录只存在这台手机上：清缓存、换手机都会丢，也没法直接拿给医生看。定期导出留一份。</p>'
      + '<div class="btnrow" style="margin-top:2px">'
      + '<button class="btn btn-primary" id="btnBackup" style="height:44px;font-size:calc(14px * var(--fs))">导出备份</button>'
      + '<button class="btn btn-ghost" id="btnCsv" style="height:44px;font-size:calc(14px * var(--fs))">导出 CSV</button>'
      + '</div>'
      + '<button class="btn btn-ghost" id="btnRestore" style="height:44px;font-size:calc(14px * var(--fs))">从备份恢复</button>'
      + '</div>';

    html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
      + '<span class="eyebrow">DEBUG · 验收用</span>'
      + '<p class="body">想立刻确认提醒能不能正常响？点下面按钮，10 秒后会收到一条测试通知（息屏 / 锁屏也能测，不会写入任何服药记录）。</p>'
      + '<button class="btn btn-ghost" id="btnTest" style="align-self:flex-start;margin-top:2px">测试提醒 · 10 秒后响一次</button>'
      + diagHtml()
      + '<p class="hint">版本 v' + esc(APP_VERSION) + ' · ' + esc(APP_BUILD) + '</p>'
      + '</div>';

    host.innerHTML = html;

    var tb = $('#btnTest');
    if (tb) tb.onclick = testReminder;

    var pb = $('#btnPurgeNotif');
    if (pb) pb.onclick = function () {
      if (!(window.MedNotify && window.MedNotify.purge)) { toast('当前是浏览器模式，没有系统通知可清'); return; }
      toast('正在清除并重新登记…');
      window.MedNotify.purge().then(function () { return window.MedNotify.stat(); })
        .then(function (s) {
          lastProbeSig = null;          // 让诊断行立刻刷新
          permProbe = permProbe || null;
          refreshPerm();
          if (s && s.delivered) toast('仍剩 ' + s.delivered + ' 条在通知栏，可手动左滑划掉');
          else toast('已清除全部已登记的提醒');
        });
    };

    $$('[data-fs]').forEach(function (el) {
      el.onclick = function () {
        var v = parseFloat(el.getAttribute('data-fs'));
        if (v === fontScale) return;
        applyFontScale(v, true);
        render();
        toast('字号已设为「' + fsLabel() + '」');
      };
    });
    var ab = $('#btnAutoBak');
    if (ab) ab.onclick = function () {
      var A = window.MedAutoBackup;
      if (!A || !A.status().supported) { toast('这台设备不支持自动备份'); return; }
      toast('正在备份…');
      A.now().then(function (s) {
        render();
        toast(s && s.ok
          ? '已备份到本地（' + (s.count == null ? '1' : s.count) + ' 份）'
          : '备份失败：' + ((s && s.err) || '未知原因'));
      });
    };

    var bb = $('#btnBackup');
    if (bb) bb.onclick = function () { openDataDlg('backup'); };
    var bc = $('#btnCsv');
    if (bc) bc.onclick = function () { openDataDlg('csv'); };
    var br = $('#btnRestore');
    if (br) br.onclick = function () { openDataDlg('restore'); };
    var bcl = $('#btnClean');
    if (bcl) bcl.onclick = openCleanDlg;

    /* 历史行里的相机图标：点开看当时的照片 */
    $$('[data-photo]').forEach(function (el) {
      el.onclick = function (ev) {
        if (ev && ev.stopPropagation) ev.stopPropagation();
        openPhoto(el.getAttribute('data-photo'));
      };
    });
  }

  function calcStreak() {
    var n = 0;
    var d = new Date();
    // 今天没吃则从昨天开始算，不算断
    if (!hasTaken(fmtDate(d))) d = new Date(d.getFullYear(), d.getMonth(), d.getDate() - 1);
    while (hasTaken(fmtDate(d))) {
      n++;
      d = new Date(d.getFullYear(), d.getMonth(), d.getDate() - 1);
      if (n > 3650) break;
    }
    return n;
  }
  function hasTaken(k) {
    var arr = S.doses[k];
    if (!arr) return false;
    return arr.some(function (x) { return x.status === 'taken'; });
  }
  /* 依从率统计（medId 为空则统计全部药品）。
   *
   * ⚠️ 必须把「补记」与「真实打卡」分开。一键补记（btnMissAll）会把漏服标成 taken，
   * 混在一起算的话点一下就能把依从率刷到 100%，这个数字就彻底不可信了 ——
   * 而这正是补记功能自己带来的副作用。分子只算**真实打卡**（无 makeup 标记）。
   *
   * 分母用「已结算剂量」= 真实打卡 + 补记 + 主动跳过 + 已过宽限期的漏服。
   * 旧实现只算 taken/skipped，把漏服排除在分母外 → 漏得越多数字反而越好看。 */
  function adherenceStats(medId) {
    var d = new Date();
    var prefix = d.getFullYear() + '-' + pad(d.getMonth() + 1);
    var tk = todayKey(), now = nowMin();
    var st = { real: 0, makeup: 0, skipped: 0, missed: 0, total: 0, rate: null };
    Object.keys(S.doses).forEach(function (k) {
      if (k.indexOf(prefix) !== 0) return;
      var isToday = k === tk;
      (S.doses[k] || []).forEach(function (x) {
        if (medId && x.medId !== medId) return;
        if (x.status === 'taken') { if (x.makeup) st.makeup++; else st.real++; return; }
        if (x.status === 'skipped') { st.skipped++; return; }
        /* 只把「已过宽限期」的 pending 算作漏服：未来的剂量不能算漏；
         * 历史日期上的 pending 一律算漏。 */
        if (x.status === 'pending' && (!isToday || now - dueAt(x) > MISS_GRACE_MIN)) st.missed++;
      });
    });
    st.total = st.real + st.makeup + st.skipped + st.missed;
    st.rate = st.total ? Math.round(st.real / st.total * 100) : null;
    return st;
  }

  /* 旧的百分比入口（按药筛选时由调用方直接传 medId） */
  function calcAdherence(medId) {
    return adherenceStats(medId).rate;
  }

  /* 记录页的药品筛选：null = 全部 */
  var histMedId = null;
  var HIST_DAYS = 14;

  /* 近 N 天里「有记录」的日期键，新的在前 */
  function historyDays(limit) {
    return Object.keys(S.doses)
      .filter(function (k) { return (S.doses[k] || []).length; })
      .sort().reverse().slice(0, limit || HIST_DAYS);
  }

  /* 历史分组标题：今天单独标出来，其余用"月-日 周X" */
  function histDayLabel(k) {
    if (k === todayKey()) return '今天 · ' + k;
    var wd = ['日', '一', '二', '三', '四', '五', '六'];
    var p = k.split('-');
    var d = new Date(parseInt(p[0], 10), parseInt(p[1], 10) - 1, parseInt(p[2], 10));
    return k.slice(5) + ' 周' + wd[d.getDay()];
  }

  /* 历史行的状态文案。历史日期上的 pending 一律是「已错过」，
   * 不能拿 isMissed() 直接判（它按"现在"算）。 */
  function histStatusLabel(x, k) {
    if (x.status === 'taken') return x.makeup ? '补记' : '已服用';
    if (x.status === 'skipped') return '已跳过';
    if (k !== todayKey() || nowMin() - dueAt(x) > MISS_GRACE_MIN) return '已错过';
    return '未到时间';
  }

  /* ---------------- 历史：摘要 / 全部列表 / 二级菜单 ---------------- */

  /* 筛选 chips。**页面与二级菜单里各渲染一份** ——
   * 页面上那份还在驱动「本月依从率」（按药看依从率是有意义的），
   * 菜单里那份是因为菜单盖住了页面、够不着。两份都带 data-hist，走同一套委托处理。 */
  function histChipsHtml() {
    if (!S.meds.length) return '';
    return '<div class="chip-row">'
      + '<button class="chip fs-chip' + (histMedId ? '' : ' on') + '" data-hist=""'
      + ' aria-pressed="' + (histMedId ? 'false' : 'true') + '">全部</button>'
      + S.meds.map(function (m) {
          var on = histMedId === m.id;
          return '<button class="chip fs-chip' + (on ? ' on' : '') + '" data-hist="' + esc(m.id) + '"'
            + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + esc(m.name) + '</button>';
        }).join('')
      + '</div>';
  }

  /* 某天是否该出现（受按药筛选影响） */
  function histDayMatch(k) {
    return !histMedId || (S.doses[k] || []).some(function (x) { return x.medId === histMedId; });
  }
  /* 某天要显示的剂量（已按计划时刻排序） */
  function histDayDoses(k) {
    return (S.doses[k] || [])
      .filter(function (x) { return !histMedId || x.medId === histMedId; })
      .slice().sort(function (a, b) { return a.time - b.time; });
  }
  function histDays() {
    return historyDays(HIST_DAYS).filter(histDayMatch);
  }

  /* 一行剂量（历史列表与今日页排版一致，只是没有可点的打卡按钮） */
  function histDoseRowHtml(x, k) {
    var m = medById(x.medId);
    var lbl = histStatusLabel(x, k);
    var cam = x.photo
      ? '<button class="cam-btn" data-photo="' + esc(x.id) + '" aria-label="查看这次服药的照片">' + ICON.cam + '</button>'
      : '';
    return '<div class="dose">'
      + '<div class="dose-l"><span class="dose-t">' + esc(minToStr(x.time)) + '</span>'
      + '<span class="dose-n' + (x.status === 'taken' ? '' : ' dim') + '">'
      + esc(m ? m.name : '已删除药品') + '</span></div>'
      + '<span class="dose-s"><span class="txt' + (lbl === '已错过' ? ' no-shot' : '') + '">'
      + esc(lbl) + '</span>' + cam + '</span>'
      + '</div>';
  }

  /* 完整按天列表（二级菜单的内容） */
  function historyListHtml() {
    var hdays = histDays();
    if (!hdays.length) return '<p class="hint">还没有服药记录。</p>';
    return hdays.map(function (k) {
      return '<div class="card sched">'
        + '<span class="eyebrow" style="display:block;margin:6px 0 2px">' + esc(histDayLabel(k)) + '</span>'
        + histDayDoses(k).map(function (x) { return histDoseRowHtml(x, k); }).join('')
        + '</div>';
    }).join('');
  }

  /* 记录页上的摘要：总量 + 最近 3 天 + 入口按钮。
   * 摘要用「共 N 天有记录 · 合计 M 次」而不是只报最近一天 ——
   * 用户想知道的是"我坚持得怎么样"，不是"昨天吃了没"。 */
  function historySummaryHtml() {
    var hdays = histDays();
    if (!hdays.length) {
      return '<p class="hint">还没有服药记录。回到「今天」打卡后，这里会按天列出来。</p>';
    }
    var total = 0;
    hdays.forEach(function (k) { total += histDayDoses(k).length; });

    var html = '<div class="sect"><span class="eyebrow">HISTORY · 近 ' + HIST_DAYS + ' 天</span>'
      + '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
      + '<p class="body" style="margin:0">共 <b>' + hdays.length + '</b> 天有记录 · 合计 <b>'
      + total + '</b> 次服药</p>';

    hdays.slice(0, 3).forEach(function (k) {
      var arr = histDayDoses(k);
      var taken = arr.filter(function (x) { return x.status === 'taken'; }).length;
      html += '<div style="display:flex;align-items:baseline;justify-content:space-between;gap:12px">'
        + '<span class="meta">' + esc(histDayLabel(k)) + '</span>'
        + '<span class="meta">' + taken + ' / ' + arr.length + ' 次</span></div>';
    });
    if (hdays.length > 3) {
      html += '<p class="hint" style="margin:0">上面是最近 3 天，更早的在下面。</p>';
    }
    html += '<button class="btn btn-ghost" id="btnHistAll" data-hall="1" style="height:44px">'
      + '查看全部历史（' + hdays.length + ' 天）</button>'
      + '</div></div>';
    return html;
  }

  /* 二级菜单：渲染菜单里那份 chips 与全部列表，再打开 */
  function openHistoryDlg() {
    var hdays = histDays();
    $('#histHint').textContent = hdays.length
      ? '近 ' + HIST_DAYS + ' 天里共 ' + hdays.length + ' 天有记录。点上面的药品名可按药筛选。'
      : '近 ' + HIST_DAYS + ' 天还没有服药记录。';
    refreshHistoryDlg(true);
    openDlg($('#dlgHistory'));
  }

  /* 重绘菜单内容。force=false 时只在菜单开着才做（筛选变化时被调用）。 */
  function refreshHistoryDlg(force) {
    var wrap = $('#dlgHistory');
    if (!wrap) return;
    if (!force && !wrap.classList.contains('show')) return;
    $('#histFilter').innerHTML = histChipsHtml();
    var body = $('#histBody');
    body.innerHTML = historyListHtml();
    /* 历史行里的相机图标：**每次重绘后重新绑**。菜单内容只在筛选变化时重建，
     * 而重建正是用户主动点了一下之后发生的 —— 不存在「重绘吞掉点击」的窗口。 */
    $$('[data-photo]', body).forEach(function (el) {
      el.onclick = function (ev) {
        if (ev && ev.stopPropagation) ev.stopPropagation();
        openPhoto(el.getAttribute('data-photo'));
      };
    });
  }



  function openSheet(medId) {
    /* 已经开着就直接忽略。连点「+」时后续的点击没有意义，只会白跑一轮
     * renderPreview + 抢焦点，把 App 拖卡（2026-09-24 反馈：点不动就狂点）。
     * 浮层是模态的，盖着遮罩也点不到别处，所以不存在"想换成另一个药"的场景。 */
    if ($('#sheetMed').classList.contains('show')) return;
    sheetSaveReset();                 // 每次打开浮层都是一个新会话
    editingId = medId;
    var med = medId ? medById(medId) : null;
    $('#sheetTitle').textContent = med ? '编辑药品' : '添加药品';
    $('#medName').value = med ? med.name : '';
    stepVal = med ? med.interval : 8;
    /* 编辑态的临时值：改完点「保存」才写进数据，中途关掉不影响原有设置 */
    medModeDraft = med ? medMode(med) : 'interval';
    fixedTimes = med ? normTimes(med.times) : [];
    $('#deleteMed').classList.toggle('hidden', !med);
    renderPreview();
    /* 浮层打开时把焦点带进去（键盘用户不用满屏找输入框）。
     * 焦点与计数住在 ui/overlay.js —— 外部只能经这两个函数写。 */
    rememberFocus();
    $('#sheetMed').classList.add('show');
    setScrim(true); enterOverlay();
    var nf = $('#medName');
    if (nf) { try { nf.focus(); } catch (e) { /* ignore */ } }
  }
  function closeSheet() {
    $('#sheetMed').classList.remove('show'); setScrim(false); leaveOverlay();
    restoreFocus();
  }







  function closeTopDialog() {
    var closable = closableDialogs();
    if (!closable.length) return false;
    var top = closable[closable.length - 1];
    /* 关掉拍照对话框 = 放弃这次打卡。必须清掉「待恢复」标记，
     * 否则下次启动会被误当成「拍照途中被杀」而自动补一次打卡。 */
    if (top.id === 'dlgPhoto') { pendingShot = null; clearPendingShot(); }
    restoreArmed = false;
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


  /* ---------------- sheet: stepper + save ---------------- */
  var stepVal = 8, editingId = null, pendingSkipId = null;
  /* 药品编辑浮层里的临时状态：模式与时刻表。点「保存」才写进数据。 */
  var medModeDraft = 'interval';
  var fixedTimes = [];

  function renderPreview() {
    /* 先按当前模式切换字段可见性（模式是编辑期的临时状态，不碰数据） */
    var isFixed = medModeDraft === 'fixed';
    var fi = $('#fieldInterval'), ff = $('#fieldFixed'), fp = $('#fieldPreview');
    if (fi) fi.classList.toggle('hidden', isFixed);
    if (ff) ff.classList.toggle('hidden', !isFixed);
    if (fp) fp.classList.toggle('hidden', isFixed);   // 预览只对间隔模式有意义
    $$('#modeRow [data-mode]').forEach(function (el) {
      var on = el.getAttribute('data-mode') === medModeDraft;
      el.classList.toggle('on', on);
      el.setAttribute('aria-pressed', on ? 'true' : 'false');
    });
    if (isFixed) { renderTimes(); renderFixedHint(); return; }

    var box = $('#previewChips');
    var step = stepVal * 60;                 // 分钟
    var arr = [];
    for (var t = 360, i = 0; t < 1440 && i < 8; t += step, i++) arr.push(minToStr(t));
    box.innerHTML = arr.map(function (t) { return '<span class="chip">' + t + '</span>'; }).join('');

    /* 值 < 1 小时时数值与单位一起换（显示「30 分钟 / 次」，而不是「0.5 小时 / 次」） */
    var under = stepVal < 1;
    $('#stepNum').textContent = under ? String(Math.round(stepVal * 60)) : String(stepVal);
    var unit = $('#sheetMed .step-unit');
    if (unit) unit.textContent = under ? '分钟 / 次' : '小时 / 次';

    /* 「每天约 N 次」——间隔细化到 30 分钟后，光看数字不容易判断一天吃几次。
     * 以 06:00 起算，与上面预览的口径一致。 */
    var hint = $('#stepHint');
    if (hint) {
      var per = Math.floor((1440 - 360) / step) + 1;
      hint.textContent = '按 06:00 起算，一天约 ' + per + ' 次。'
        + (per > 8 ? '次数较多，记得确认是否与医嘱一致。' : '');
    }
  }
  /* F-1：步进细化为 30 分钟（0.5 小时），并吸附到 0.5 的整数倍 ——
   * 否则连续的 0.5 累加会漂成 0.30000000000000004 这类值写进数据。 */
  /* 固定时刻的编辑列表。行是动态生成的，事件靠**委托**绑在 #timeList 上
   * （它本身不被替换，所以只绑一次就够）。 */
  function renderTimes() {
    var box = $('#timeList');
    if (!box) return;
    if (!fixedTimes.length) {
      box.innerHTML = '<p class="hint" style="margin:0">还没有时刻。点下面添加一个 —— 至少要有一个才会提醒。</p>';
      return;
    }
    box.innerHTML = fixedTimes.map(function (t, i) {
      return '<div class="time-row">'
        + '<input type="time" class="time-input" data-ti="' + i + '" value="' + minToStr(t) + '"'
        + ' aria-label="第 ' + (i + 1) + ' 个服药时刻">'
        + '<button class="icon-btn" data-tdel="' + i + '" aria-label="删掉第 ' + (i + 1) + ' 个时刻">'
        + '<svg width="16" height="16" viewBox="0 0 24 24" fill="none"><path d="M6 6l12 12M18 6 6 18" stroke="#fff" stroke-width="1.8" stroke-linecap="round"/></svg>'
        + '</button></div>';
    }).join('');
  }

  function renderFixedHint() {
    var hint = $('#fixedHint');
    if (!hint) return;
    var ts = normTimes(fixedTimes);
    if (!ts.length) { hint.textContent = '至少要设一个时刻，否则它不会提醒。'; return; }
    hint.textContent = '每天 ' + ts.length + ' 次：' + ts.map(minToStr).join('、') + '。'
      + (ts.length > 8 ? '次数较多，确认一下是否与医嘱一致。' : '');
  }

  function clampStep() {
    if (!isFinite(stepVal)) stepVal = 8;
    stepVal = Math.round(stepVal * 2) / 2;
    if (stepVal < 0.5) stepVal = 0.5;
    if (stepVal > 24) stepVal = 24;
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
  function askNotify() {
    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.requestPermission().then(function (p) { notifyPerm = p; render(); });
      return;
    }
    // 浏览器：只有 default 时才允许弹授权框，denied 时浏览器会静默忽略，
    // 此时不刷新权限也不假报成功，让权限卡继续显示引导用户手动改。
    try {
      if ('Notification' in window && Notification.permission === 'default') {
        Notification.requestPermission().then(function () { notifyPerm = browserPerm(); render(); });
      } else {
        notifyPerm = browserPerm(); render();
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
          lastProbeSig = null;
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

  /* ---------------- 备份 / 导出 / 恢复 ----------------
   * 记录默认只存在本机，清缓存或换机即全丢。这里给两条零依赖的出路：
   *   · JSON 备份 —— 可完整恢复（含药品与全部剂量）
   *   · CSV      —— 给人看（就诊时打印或发给医生）
   * 不依赖任何 Capacitor 插件：Android WebView 里 a[download] 常常不生效，
   * 所以「复制到剪贴板」是主路径，「下载文件」是浏览器上的加分项。 */
  var BACKUP_FORMAT = 'medreminder.backup';
  var BACKUP_VERSION = 1;

  /* 导入体积上限。localStorage 在 Android WebView 里通常 5 MB（UTF-16 计 2 字节/字符），
   * 一份备份的字符数超过这个门槛就几乎必然塞爆配额 —— 与其导入到一半失败、
   * 把用户现有数据搞成"改了一半"，不如**在改数据之前**就明确拒绝。
   * 取 2 MB 字符：即便按 2 字节/字符也才 4 MB，低于 5 MB 预算并留出余量。
   * 正常用户一个月的记录约 30 KB（见测试用例 F），离上限还有两个数量级。 */
  var BACKUP_MAX_CHARS = 2 * 1024 * 1024;
  /* 剂量单日上限：一天最多 2880 个 30 分钟槽位，再往上必然是脏数据或恶意构造 */
  var BACKUP_MAX_DOSES_PER_DAY = 2880;

  /* status 只认这三个值 —— 其它值既不算已服也不算待服，会静默污染统计 */
  var DOSE_STATUSES = ['pending', 'taken', 'skipped'];

  /* 合法日期键：YYYY-MM-DD，且**必须是真实存在的日期**（含闰年规则）。
   * 用日期回写比对而不是正则，才能拦下 2026-02-30 / 2025-02-29 这种。 */
  function isDateKey(k) {
    if (typeof k !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(k)) return false;
    var y = +k.slice(0, 4), mo = +k.slice(5, 7), d = +k.slice(8, 10);
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
    var dt = new Date(y, mo - 1, d);
    return dt.getFullYear() === y && dt.getMonth() === mo - 1 && dt.getDate() === d;
  }

  var dataMode = null;       // backup | csv | restore
  var restoreArmed = false;  // 恢复需点两次，避免误覆盖

  function buildBackup() {
    // notified 是当天的提醒登记簿，属临时状态，不进备份
    return {
      format: BACKUP_FORMAT,
      version: BACKUP_VERSION,
      app: 'MedReminder',
      exportedAt: new Date().toISOString(),
      data: { meds: S.meds, doses: S.doses }
    };
  }

  /* ⚠️ 剂量侧的校验（H-3）。
   *
   * 这段存在的理由：doses 里的每一处非法值都不会**当场**报错，而是
   * 在被渲染/统计/排程时炸出来，用户看到的是"导入成功了，但记录页白屏"。
   * 逐个堵住后果：
   *   · 值不是数组        → S.doses[k].some/forEach → TypeError（整页崩）
   *   · 日期键不是真日期  → 排序、按月前缀过滤（adherenceStats 按 'YYYY-MM' 取前缀）全错
   *   · time 越界/非数字  → minToStr、排程比较得到荒谬结果
   *   · 缺 id             → 照片回收、去重按 undefined 归组
   *   · status 未知值     → 既不算已服也不算待服，静默丢出统计
   *   · 单日剂量爆炸      → 体积/内存
   * 返回 true 表示「有问题」，与上方 meds 的写法保持一致。 */
  function dosesInvalid(doses) {
    if (Array.isArray(doses)) return true;                  // 整体必须是「日期 → 数组」的对象，不是数组
    var keys = Object.keys(doses), total = 0;
    for (var i = 0; i < keys.length; i++) {
      var k = keys[i];
      if (!isDateKey(k)) return true;
      var arr = doses[k];
      if (!Array.isArray(arr)) return true;                 // 一天的值必须是数组
      if (arr.length > BACKUP_MAX_DOSES_PER_DAY) return true;
      total += arr.length;
      for (var j = 0; j < arr.length; j++) {
        var d = arr[j];
        if (!d || typeof d !== 'object' || Array.isArray(d)) return true;
        if (typeof d.id !== 'string' || !d.id) return true;
        if (typeof d.medId !== 'string' || !d.medId) return true;   // 允许指向已删除的药（保留历史）
        if (typeof d.time !== 'number' || !isFinite(d.time)) return true;
        if (d.time < 0 || d.time > 1439) return true;
        if (DOSE_STATUSES.indexOf(d.status) < 0) return true;
        /* takenAt 允许 null（未服）或数字（时间戳）；其它类型一律可疑 */
        if (d.takenAt != null && (typeof d.takenAt !== 'number' || !isFinite(d.takenAt))) return true;
      }
    }
    return false;
  }

  /* 导入前严格校验：宁可拒绝，也不要让半截数据覆盖掉用户现有记录 */
  function parseBackup(txt) {
    if (!txt || !String(txt).trim()) return { err: '请先粘贴备份内容' };
    /* 体积先于解析判断 —— JSON.parse 一个巨型字符串本身就可能把内存打满 */
    if (String(txt).length > BACKUP_MAX_CHARS) {
      return { err: '备份太大（超过 2 MB），可能不是本 App 导出的备份' };
    }
    var o;
    try { o = JSON.parse(txt); } catch (e) { return { err: '内容不是合法 JSON，可能复制不完整' }; }
    if (!o || o.format !== BACKUP_FORMAT) return { err: '这不是本 App 导出的备份' };
    if (typeof o.version !== 'number' || o.version > BACKUP_VERSION) return { err: '备份来自更新版本的 App' };
    if (!o.data || !Array.isArray(o.data.meds) || !o.data.doses || typeof o.data.doses !== 'object') {
      return { err: '备份内容不完整' };
    }
    var badMed = !o.data.meds.every(function (m) {
      if (!m || typeof m.id !== 'string' || typeof m.name !== 'string') return false;
      /* interval 必须仍合法：它是固定时刻模式下的「默认间隔」，也用于旧数据 */
      if (typeof m.interval !== 'number' || !isFinite(m.interval) || m.interval <= 0) return false;
      /* mode / times 是可选的（旧备份没有这两项）。有就必须合法 ——
       * 脏数据直接进排程是会死循环或排 0 次的。 */
      if (m.mode != null && m.mode !== 'interval' && m.mode !== 'fixed') return false;
      if (m.mode === 'fixed') {
        if (!Array.isArray(m.times) || !m.times.length) return false;
        if (!m.times.every(function (x) {
          return typeof x === 'number' && isFinite(x) && x >= 0 && x <= 1439;
        })) return false;
      }
      return true;
    });
    if (badMed) return { err: '备份里的药品数据有问题' };
    if (dosesInvalid(o.data.doses)) return { err: '备份里的服药记录有问题' };
    return { ok: o };
  }

  function csvCell(v) {
    var s = String(v == null ? '' : v);
    return /[",\r\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
  }
  function buildCsv() {
    var rows = [['日期', '计划时刻', '实际服药时刻', '药品', '间隔(小时)', '状态']];
    Object.keys(S.doses).sort().forEach(function (k) {
      (S.doses[k] || []).slice().sort(function (a, b) { return a.time - b.time; }).forEach(function (d) {
        var m = medById(d.medId);
        rows.push([
          k,
          minToStr(d.time),
          (d.status === 'taken' && d.takenAt) ? minToStr(minOfDay(d.takenAt)) : '',
          m ? m.name : '已删除药品',
          m ? m.interval : '',
          d.status === 'taken' ? '已服用' : (d.status === 'skipped' ? '已跳过' : '待服用')
        ]);
      });
    });
    return rows.map(function (r) { return r.map(csvCell).join(','); }).join('\r\n');
  }
  function doseRecordCount() {
    var n = 0;
    Object.keys(S.doses).forEach(function (k) { n += (S.doses[k] || []).length; });
    return n;
  }

  function downloadText(filename, text, mime) {
    try {
      var blob = new Blob([text], { type: mime + ';charset=utf-8' });
      var url = URL.createObjectURL(blob);
      var a = document.createElement('a');
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      setTimeout(function () {
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
      }, 1500);
      return true;
    } catch (e) { return false; }
  }


  function openDataDlg(mode) {
    dataMode = mode;
    restoreArmed = false;
    var ta = $('#dataArea');
    var isRestore = mode === 'restore';
    ta.value = '';
    ta.readOnly = !isRestore;

    if (mode === 'backup') {
      $('#dataEyebrow').textContent = 'BACKUP';
      $('#dataTitle').textContent = '导出备份';
      $('#dataHint').textContent = '复制下面全部内容，存进备忘录或网盘。换手机、清缓存后可用它完整恢复。';
      ta.value = JSON.stringify(buildBackup(), null, 2);
    } else if (mode === 'csv') {
      $('#dataEyebrow').textContent = 'CSV';
      $('#dataTitle').textContent = '导出服药记录';
      $('#dataHint').textContent = doseRecordCount()
        ? '下面是全部服药记录（计划时刻 / 实际服药时刻 / 状态），复制后粘进 Excel，或直接发给医生。'
        : '还没有任何服药记录，导出内容只有表头。';
      ta.value = buildCsv();
    } else {
      $('#dataEyebrow').textContent = 'RESTORE';
      $('#dataTitle').textContent = '从备份恢复';
      $('#dataHint').textContent = '把备份内容粘贴到下面（或点「从文件选择」），再点「恢复」。这会覆盖当前全部药品与记录。';
    }

    $('#dataCopy').classList.toggle('hidden', isRestore);
    $('#dataDownload').classList.toggle('hidden', isRestore || !ta.value);
    $('#dataPick').classList.toggle('hidden', !isRestore);
    $('#dataApply').classList.toggle('hidden', !isRestore);
    var ap = $('#dataApply');
    ap.textContent = '恢复';
    ap.classList.remove('btn-accent');
    ap.classList.add('btn-primary');
    $('#dataDownload').textContent = mode === 'csv' ? '下载 .csv 文件' : '下载 .json 文件';
    openDlg($('#dlgData'));
  }

  function applyRestore() {
    var r = parseBackup($('#dataArea').value);
    if (r.err) { toast(r.err); return; }
    var d = r.ok.data;
    var nMed = d.meds.length;
    var nDay = Object.keys(d.doses).length;
    if (!restoreArmed) {           // 第一次点：只做提示，不动数据
      restoreArmed = true;
      var b = $('#dataApply');
      b.textContent = '再点一次 · 覆盖当前数据';
      b.classList.remove('btn-primary');
      b.classList.add('btn-accent');
      toast('将覆盖现有内容（' + nMed + ' 个药品 · ' + nDay + ' 天记录）');
      return;
    }
    restoreArmed = false;
    S.meds = d.meds;
    S.doses = d.doses;
    S.notified = {};      // 提醒登记簿重建，避免旧标记把今天的提醒压掉
    silenceOverdue();     // 与启动逻辑一致：过期太久的不补响
    save();               // 内含 syncNotifications()
    closeDlg($('#dlgData'));
    render();
    syncNotifications();
    toast('已恢复 ' + nMed + ' 个药品 · ' + nDay + ' 天记录');
  }


  /* ---------------- render dispatcher ---------------- */
  /* ---------------- 通知权限状态 ----------------
   * notifyPerm 取值：granted / denied / prompt(未申请，浏览器) / unsupported / unknown
   * 原生：来自 LocalNotifications.checkPermissions()
   * 浏览器：来自 Notification.permission（default → prompt）
   * 只有 denied 才是「用户已经拒绝」，需要引导去系统/浏览器设置里改。 */
  function permCardHtml() {
    var isNative = !!(window.MedNotify && window.MedNotify.native);

    // 浏览器不支持通知：页面必须开着才提醒，说清楚，别让用户以为装完就万事大吉
    if (!isNative && notifyPerm === 'unsupported') {
      return '<div class="card" style="display:flex;flex-direction:column;gap:6px;border-color:#8A8F98">'
        + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:var(--sub)">当前环境不支持通知</span>'
        + '<span class="meta">提醒只在 App 页面前台时弹出，切走或锁屏不会响。装到手机上才能锁屏提醒。</span></div>';
    }

    // 未申请：不吓唬用户，只在浏览器模式提示，点了才发起授权
    if (!isNative && notifyPerm === 'prompt') {
      return '<button class="card" id="btnPerm" style="display:flex;flex-direction:column;gap:6px;width:100%;border-color:#FF7A17;cursor:pointer;-webkit-tap-highlight-color:transparent">'
        + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:#FF7A17">还没开启通知</span>'
        + '<span class="meta">开启后才能及时收到服药提醒，点这里授权。</span></button>';
    }

    // 权限不可用：原生下点卡片先重新申请，拿不到再跳系统设置页；浏览器只能让用户去浏览器设置里改
    if (notifyPerm === 'denied') {
      var hint = isNative
        ? '锁屏和息屏时收不到服药提醒。点这里重新申请；若系统不再弹授权框，会自动打开系统设置页，请在那里把「通知」打开。'
        : '浏览器已拒绝通知，需要到浏览器的网站权限设置里把通知改回「允许」，然后刷新页面。';
      return '<button class="card" id="btnPerm" style="display:flex;flex-direction:column;gap:6px;width:100%;border-color:#FF7A17;cursor:pointer;-webkit-tap-highlight-color:transparent">'
        + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:#FF7A17">通知未开启 · 收不到提醒</span>'
        + '<span class="meta">' + hint + '</span></button>';
    }

    return '';
  }

  /* 浏览器模式下把 Notification.permission 映射成与原生一致的取值。
   * 统一从 window 上取，避免 "检查 window、读取全局" 的不一致。 */
  function browserPerm() {
    try {
      var N = window.Notification || (typeof Notification !== 'undefined' ? Notification : null);
      if (!N) return 'unsupported';
      var p = N.permission;
      if (p === 'granted') return 'granted';
      if (p === 'denied') return 'denied';
      return 'prompt';   // default
    } catch (e) { return 'unsupported'; }
  }

  /* 最近一次原生探测结果。既用于判定，也用于 DEBUG 卡自检展示 ——
   * 真机验收时「看不到权限卡」若只靠猜，会来回折腾好几轮。 */
  var permProbe = null;
  /* 通知的两项计数（系统排程 / 通知栏显示）。用于诊断「一次冒出一堆通知」。 */
  var notifyStat = null;
  var lastProbeSig = '';
  /* 权限卡点击后走过的步骤。权限跳转是唯一没法远程验证的环节 ——
   * 把结果直接显示在 DEBUG 卡上，比来回猜快得多。 */
  var permAction = '';

  function refreshPerm() {
    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.probe().then(function (p) {
        /* 顺带取一次通知计数。它不进权限判定，只用于诊断显示 ——
         * 但「一次冒出来一堆通知」这种问题，没有数字就只能靠猜。 */
        var st = window.MedNotify.stat ? window.MedNotify.stat() : Promise.resolve(null);
        return st.then(function (ns) { return { p: p, ns: ns }; });
      }).then(function (r) {
        var p = r.p, ns = r.ns;
        var sig = JSON.stringify(p) + '|' + JSON.stringify(ns);
        // 只在探测结果真的变化时重绘。每 5 秒无条件 render() 会重建视图 DOM，
        // 打断用户滚动与输入（innerHTML 一换，滚动位置就回顶）。
        if (sig === lastProbeSig) return;
        lastProbeSig = sig;
        permProbe = p;
        notifyStat = ns;

        var next;
        if (window.MedNotify.isBlocked(p)) next = 'denied';
        else if (p.display === 'granted') next = 'granted';
        else next = 'unknown';

        if (next !== notifyPerm) notifyPerm = next;
        /* ⚠️ 有浮层开着时**不要**重绘：上面那句 render() 会把三个视图的 innerHTML
         * 整个重建，正在输入/正在点的元素会被换掉（"点了没反应"的主要来源之一）。
         * 代价：权限卡可能晚一点才更新 —— 而浮层盖着时本来就看不见它，
         * 之后任何一次操作都会重绘。 */
        if (isOverlayOpen()) return;
        render();
      });
      return;
    }
    var b = browserPerm();
    if (b !== notifyPerm) { notifyPerm = b; render(); }
  }

  /* DEBUG 卡的自检行：把原生探测到的原始值直接显示出来。
   * 「看不到权限卡」若只靠猜，会来回折腾好几轮；显示实际读数可以一次定位。 */
  /* 已注册的 Capacitor 插件清单。
   * 加这一行是因为踩过一个很隐蔽的坑：项目没有打包器，插件必须靠 <script> 引入
   * 才会注册进 Capacitor.Plugins；少了这一步，判断条件能通过、调用却失败，
   * 表现成「功能没反应」而且没有任何报错。有了这行，一眼就能看出插件在不在。 */
  function plugLine() {
    var c = window.Capacitor;
    var names = (c && c.Plugins) ? Object.keys(c.Plugins).sort() : [];
    var heads = (window.MedNotify && window.MedNotify.bridgeHeaders) ? window.MedNotify.bridgeHeaders() : 0;
    var st = (window.MedNotify && window.MedNotify.settingsState) ? window.MedNotify.settingsState() : 'missing';
    var stTxt = st === 'injected' ? '已注入' : (st === 'proxy' ? '代理兜底可用' : '不可用');
    var ua = (window.navigator && window.navigator.userAgent) || '';
    var m = /Android\s+([0-9.]+)/.exec(ua);
    var sys = '未知';
    if (m) {
      var major = parseInt(m[1], 10) || 0;
      sys = 'Android ' + m[1] + (major >= 13 ? '（可弹授权框）' : '（不弹授权框，只能跳设置）');
    }
    var ph = (window.MedPhoto && window.MedPhoto.hashCount) ? window.MedPhoto.hashCount() : 0;
    return '<p class="hint">已注册插件：'
      + (names.length ? esc(names.join(' / ')) : '无 —— 插件 JS 未加载')
      + '</p>'
      + '<p class="hint">照片校验：已登记 ' + ph + ' 个指纹（用于识别重复照片）</p>'
      + '<p class="hint">原生桥接：' + (heads ? ('已注入 ' + heads + ' 个插件头') : '未注入 —— 原生调用会失败')
      + ' · 跳设置插件：' + stTxt + '</p>'
      + '<p class="hint">系统：' + esc(sys) + '</p>';
  }

  /* 自动备份的状态卡 */
  function autoBackupCardHtml() {
    var A = window.MedAutoBackup;
    if (!A) return '';
    var s = A.status();
    var line;
    if (!s.supported) line = '这台设备不支持（浏览器模式没有文件系统）';
    else if (!s.at) line = '还没备份过 —— 改动一次数据就会自动存';
    else if (!s.ok) line = '上次没成功：' + (s.err || '未知原因');
    else line = '最近一次：' + A.stamp(s.at) + (s.count == null ? '' : ' · 保留 ' + s.count + ' 份');

    return '<div class="card" style="display:flex;flex-direction:column;gap:8px">'
      + '<span class="eyebrow">AUTO · 自动备份</span>'
      + '<p class="body">每次改动都会自动往手机里存一份完整数据，随时能找回来。</p>'
      + '<p class="hint">' + esc(line) + '</p>'
      /* 目录是长 token —— `/` 与 `.` 都不是断行点，默认整串不拆，会顶出卡片右边
       * （2026-09-28 真机反馈）。手工在每个 `/` 后插一个 <wbr>（零宽断行机会），
       * 断行就落在目录分隔处，比让浏览器在任意字符处硬断好看得多。
       * 先 esc 再插 <wbr>：`/` 不在 esc 的转义表里，先后无所谓，但先 esc 更稳妥。 */
      + '<p class="hint">位置：'
      + esc('Android/data/com.medreminder.app/files/' + A.DIR).replace(/\//g, '/<wbr>')
      + '（用电脑 USB 能看到）</p>'
      + '<p class="hint">⚠️ 卸载 App 会连这个目录一起删掉 —— <b>换手机或重装之前，'
      + '请先用上面「导出备份」另存一份到别处</b>。</p>'
      + '<button class="btn btn-ghost" id="btnAutoBak" style="height:44px;font-size:calc(14px * var(--fs))">立即备份一次</button>'
      + '</div>';
  }

  function diagHtml() {
    if (!(window.MedNotify && window.MedNotify.native)) {
      return '<p class="hint">通知模式：浏览器（无系统闹钟，页面关掉就不响）</p>' + plugLine();
    }
    var p = permProbe;
    if (!p) return '<p class="hint">通知状态：读取中…</p>' + plugLine();
    return '<p class="hint">通知状态：权限 ' + esc(p.display)
      + ' · App 开关 ' + (p.enabled === null ? '读不到' : (p.enabled ? '开' : '关'))
      + ' · 渠道 ' + (p.channelFound ? ('importance ' + p.channelImportance) : '未创建')
      + '</p>'
      + (permAction ? '<p class="hint">上次点击权限卡：' + esc(permAction) + '</p>' : '')
      + (notifyStat
          ? '<p class="hint">系统排程：' + (notifyStat.pending == null ? '读不到' : notifyStat.pending + ' 条')
            + ' · 通知栏显示：' + (notifyStat.delivered == null ? '读不到' : notifyStat.delivered + ' 条')
            + (notifyStat.test ? '（含 1 条测试）' : '') + '</p>'
          : '')
      + '<button class="btn btn-ghost" id="btnPurgeNotif" style="align-self:flex-start;margin-top:2px">清除全部提醒通知</button>'
      + '<p class="hint">点它会取消系统里所有已登记的提醒，并按今天的排程重新登记一遍。'
      + '通知栏里堆了重复的、或者早就过期的提醒时用它。</p>'
      + plugLine();
  }

  /* ---------------- 存储 UI（D-1） ---------------- */

  /* 写失败告警卡：**跨页可见**（今日页与记录页都放）。
   * 用户可能整天停在今日页打卡，只在记录页提示等于没提示。 */
  function storageAlertHtml() {
    if (!storageError) return '';
    var quota = storageError.kind === 'quota';
    var title = quota ? '数据没能保存 · 本地存储已满' : '数据没能保存';
    /* 文案里刻意说清「已存的数据没坏」——否则用户第一反应是「我的记录全没了」 */
    var body = quota
      ? '最近的改动没写进手机。重启 App 会退回上一次保存成功的状态，之前的记录没丢。请先导出备份，再清理旧记录。'
      : '写入失败：' + storageError.msg + '。请先导出备份，避免记录丢失。';
    /* 用 data 属性而非 id 绑定：这张卡在两个视图里各出现一次，用 id 会产生重复 id */
    return '<button class="card card-alert" data-storage-fix="1">'
      + '<span class="alert-title">⚠︎ ' + title + '</span>'
      + '<span class="meta">' + esc(body) + '</span>'
      + '<span class="alert-cta">去处理 →</span></button>';
  }

  /* 存储状态卡：让「什么时候会满」提前可见，而不是等它满了才知道 */
  function storageCardHtml() {
    var st = storageStats();
    var level, note;
    if (storageError)      { level = 'err';  note = '写入失败，新数据没能存进手机。请先导出备份，再清理旧记录。'; }
    else if (st.pct >= 85) { level = 'err';  note = '已接近上限，随时可能写不进去。请立即导出备份并清理旧记录。'; }
    else if (st.pct >= 70) { level = 'warn'; note = '占用偏高。建议导出备份后清理旧记录。'; }
    else                   { level = 'ok';   note = '记录只存在这台手机上，清缓存或换手机会丢，建议定期导出备份。'; }

    var color = level === 'err' ? '#FF3B30' : (level === 'warn' ? '#FF7A17' : '#3ECF8E');
    var barW = st.pct > 0 ? Math.max(2, st.pct) : 0;   // 有数据至少画一小段，0% 的空条看起来像坏了

    return '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
      + '<div style="display:flex;align-items:baseline;justify-content:space-between;gap:12px">'
      + '<span class="eyebrow">STORAGE · 本地存储</span>'
      + '<span class="meta">' + st.days + ' 天 · ' + st.doses + ' 条</span></div>'
      + '<div class="bar" role="img" aria-label="已用 ' + st.pct + '%"><i style="width:' + barW + '%;background:' + color + '"></i></div>'
      + '<p class="body" style="margin:0">已用约 ' + st.kb + ' KB'
      + (st.photoFiles
          ? ('（记录 ' + Math.round((st.bytes - st.photoBytes) / 1024) + ' KB + 照片 ' + st.photoFiles + ' 张 ' + Math.round(st.photoBytes / 1024) + ' KB）')
          : '')
      + '，上限 5 MB（' + st.pct + '%）。' + esc(note) + '</p>'
      + '<button class="btn btn-ghost" id="btnClean" style="height:44px;font-size:calc(14px * var(--fs));align-self:flex-start">清理旧记录</button>'
      + '</div>';
  }

  /* 清理对话框。默认停在「不清理」，且**必须用户主动点确认** —— 破坏性操作不设默认值。 */
  var CLEAN_LEVELS = [
    { label: '90 天', keep: 90 },
    { label: '180 天', keep: 180 },
    { label: '1 年', keep: 365 },
    { label: '不清理', keep: 0 }
  ];
  var cleanKeep = 0;

  function renderCleanRow() {
    var row = $('#cleanRow');
    if (!row) return;
    row.innerHTML = CLEAN_LEVELS.map(function (lv) {
      var on = lv.keep === cleanKeep;
      return '<button class="chip fs-chip' + (on ? ' on' : '') + '" data-clean="' + lv.keep + '"'
        + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + lv.label + '</button>';
    }).join('');
    $$('[data-clean]').forEach(function (el) {
      el.onclick = function () {
        cleanKeep = parseInt(el.getAttribute('data-clean'), 10) || 0;
        renderCleanRow();
        renderCleanPreview();
        var btn = $('#cleanConfirm');
        if (btn) btn.disabled = cleanKeep === 0;   // 「不清理」= 什么都不做，确认键就该是灰的
      };
    });
  }
  function renderCleanPreview() {
    var pv = $('#cleanPreview');
    if (!pv) return;
    if (!cleanKeep) { pv.textContent = '不清理 · 保留全部记录'; return; }
    var r = cleanPreview(cleanKeep);
    pv.textContent = r.days
      ? ('保留最近 ' + cleanKeep + ' 天 · 删除 ' + r.days + ' 天 / ' + r.doses + ' 条')
      : ('保留最近 ' + cleanKeep + ' 天 · 没有可删的记录');
  }
  function openCleanDlg() {
    cleanKeep = 0;          // 每次都从「不清理」开始，避免误触上一次的档位
    renderCleanRow();
    renderCleanPreview();
    var btn = $('#cleanConfirm');
    btn.disabled = true;    // 没选范围就不让点，避免「点了没反应」
    openDlg($('#dlgClean'));
  }

  function render() {
    renderToday(); renderMeds(); renderRecords();
  }

  /* ---------------- tabs ---------------- */
  function setTab(name) {
    $$('.tab').forEach(function (t) { t.classList.toggle('active', t.getAttribute('data-tab') === name); });
    $$('.view').forEach(function (v) { v.classList.toggle('active', v.getAttribute('data-view') === name); });
    $('#main').scrollTop = 0;
    currentTab = name;
  }
  var currentTab = 'today';
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

    /* 历史筛选与「查看全部历史」——**事件委托**（挂在 document，只注册一次）。
     * 为什么不用 $$(...).forEach 直接绑：chips 在页面与二级菜单里各有一份，
     * 而菜单内容会被重绘 —— 绑在元素上的 click 会随 innerHTML 重建而失效，
     * 表现成"点了没反应"（这个项目已经踩过一次，见 MEMORY-数据与交互）。 */
    document.addEventListener('click', function (ev) {
      var t = ev.target;
      if (!t || !t.closest) return;
      var chip = t.closest('[data-hist]');
      if (chip) {
        var v = chip.getAttribute('data-hist');
        histMedId = v ? v : null;
        render();                    // 页面：统计 + 摘要跟着变
        refreshHistoryDlg(false);    // 菜单：若开着，列表与 chips 一起重绘
        return;
      }
      if (t.closest('[data-hall]')) openHistoryDlg();
    });

    $('#scrim').onclick = function () { closeSheet(); };
    $('#sheetClose').onclick = closeSheet;
    $('#stepMinus').onclick = function () { stepVal -= 0.5; clampStep(); renderPreview(); };
    $('#stepPlus').onclick = function () { stepVal += 0.5; clampStep(); renderPreview(); };

    $('#saveMed').onclick = function () {
      /* 防重入：一次浮层会话只允许保存一次。连点会各新增一条药 —— 见 sheetSaveBegin 的注释。 */
      if (!sheetSaveBegin()) return;
      var isFirstMed = false;
      var name = $('#medName').value.trim();
      if (!name) { toast('请填写药品名称'); $('#medName').focus(); return; }

      /* 校验必须放在改数据**之前** —— 否则会「改一半」再报错，
       * 留下一个模式变了、时刻却没存上的坏状态。 */
      var isFixed = medModeDraft === 'fixed';
      var ts = isFixed ? normTimes(fixedTimes) : null;
      if (isFixed && !ts.length) { toast('请至少添加一个服药时刻'); return; }

      if (editingId) {
        var m = medById(editingId);
        if (m) {
          /* 先把"改动前"的样子拍下来再落值 —— 判断必须在赋值之前，
           * 否则拿新旧一比永远是"没变"。 */
          var prev = { mode: medMode(m), times: normTimes(m.times), interval: m.interval };
          var next = { mode: medModeDraft, times: ts, interval: stepVal };
          m.name = name;
          m.mode = medModeDraft;
          if (isFixed) m.times = ts;
          else m.interval = stepVal;
          /* 模式换了 / 间隔改了 / **固定模式时刻改了** → 今天的排程要重排（已打卡的记录一律保留）。
           * ⚠️ H-1：这里原先是 `modeChanged || intervalChanged`，而固定模式下两者恒为 false
           * （intervalChanged 带 `!isFixed` 前置，modeChanged 要求模式真的变了）——
           * 「只改时刻」于是被静默忽略：今天页与系统通知仍是旧时刻，要等次日生效，
           * 用户会以为"改了没用"。判定已抽成 needRebuildDoses（可单测，
           * 见 tests/fixed.spec.js 的 H 段）。
           * 重排只负责改数据 + 撤旧通知；新通知由随后的 save() → syncNotifications() 统一登记。 */
          if (needRebuildDoses(prev, next)) rebuildTodayDoses(m);
        }
        toast('已保存');
      } else {
        var nm = { id: uid(), name: name, interval: stepVal };
        if (isFixed) { nm.mode = 'fixed'; nm.times = ts; }
        S.meds.push(nm);
        toast('已添加 ' + name);
        isFirstMed = true;
      }

      save();
      /* 固定时刻模式不等打卡，保存后立刻把今天的时刻排出来 */
      if (isFixed) ensureFixedDoses();
      closeSheet(); render();
      /* 首次添加药品后主动引导一次通知授权（F-3）——放在 closeSheet 之后，
       * 否则系统权限弹窗会盖在药品编辑浮层上，关掉它才能继续操作。 */
      if (!editingId && isFirstMed) guideNotifyOnce();
    };

    $('#deleteMed').onclick = function () {
      if (!editingId) return;
      var id = editingId;
      var med = medById(id);
      var n = todayDoses().filter(function (d) { return d.medId === id; }).length;
      askConfirm(
        '删除「' + (med ? med.name : '这个药品') + '」？',
        '删除后不再提醒它' + (n ? '，今天的 ' + n + ' 次提醒也会一并取消' : '')
          + '。历史服药记录会保留，不会丢数据。',
        '确认删除',
        function () {
          var r = deleteMed(id);
          save(); closeSheet(); render();
          toast('已删除 ' + (r ? r.name : ''));
        });
    };

    /* ---- 服药方式切换（F-4） ---- */
    $$('#modeRow [data-mode]').forEach(function (el) {
      el.onclick = function () {
        var v = el.getAttribute('data-mode');
        if (v === medModeDraft) return;
        medModeDraft = v;
        /* 切到固定时刻但一个时刻都没有 → 先给两个常见起点（早 8 点、晚 8 点）。
         * 面对空白输入框比面对两个可改的默认值难用得多。 */
        if (v === 'fixed' && !fixedTimes.length) fixedTimes = [480, 1200];
        renderPreview();
      };
    });

    var at = $('#addTime');
    if (at) at.onclick = function () {
      if (fixedTimes.length >= MAX_TIMES) { toast('最多 ' + MAX_TIMES + ' 个时刻'); return; }
      var ts = normTimes(fixedTimes);
      /* 默认接在最后一个时刻后面 4 小时；还没有就用 08:00 */
      var next = ts.length ? Math.min(1439, ts[ts.length - 1] + 240) : 480;
      if (ts.length && next === ts[ts.length - 1]) { toast('已经是 23:59 了，加不了更晚的'); return; }
      fixedTimes = normTimes(ts.concat([next]));
      renderTimes(); renderFixedHint();
    };

    var tl = $('#timeList');
    if (tl) {
      tl.addEventListener('change', function (ev) {
        var el = ev.target;
        if (!el || !el.getAttribute) return;
        var i = el.getAttribute('data-ti');
        if (i == null) return;
        var v = timeInputToMin(el.value);
        if (v == null) { toast('这个时刻看不懂，请重新选一次'); renderTimes(); return; }
        /* 只改这一项、**不重排** —— 用户可能还要接着改别的，列表跳来跳去很难用 */
        fixedTimes[parseInt(i, 10)] = v;
        renderFixedHint();
      });
      tl.addEventListener('click', function (ev) {
        var el = ev.target && ev.target.closest ? ev.target.closest('[data-tdel]') : null;
        if (!el) return;
        fixedTimes.splice(parseInt(el.getAttribute('data-tdel'), 10), 1);
        renderTimes(); renderFixedHint();
      });
    }

    $('#confirmCancel').onclick = function () { clearConfirmCb(); closeDlg($('#dlgConfirm')); };
    $('#confirmOk').onclick = function () {
      var cb = takeConfirmCb();
      closeDlg($('#dlgConfirm'));
      if (cb) cb();
    };

    $('#skipCancel').onclick = function () { pendingSkipId = null; closeDlg($('#dlgSkip')); };
    $('#skipConfirm').onclick = function () {
      var l = todayDoses();
      for (var i = 0; i < l.length; i++) {
        if (l[i].id === pendingSkipId) { l[i].status = 'skipped'; break; }
      }
      save(); pendingSkipId = null; closeDlg($('#dlgSkip')); render(); toast('已跳过本次');
    };

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

    /* ---- 备份 / 导出 / 恢复 ---- */
    $('#dataClose').onclick = function () { restoreArmed = false; closeDlg($('#dlgData')); };
    $('#histClose').onclick = function () { closeDlg($('#dlgHistory')); };
    $('#dataCopy').onclick = function () {
      var txt = $('#dataArea').value;
      if (!txt) { toast('没有可复制的内容'); return; }
      copyText(txt).then(function (ok) {
        toast(ok ? '已复制到剪贴板' : '复制失败，请长按文本框全选后手动复制');
      });
    };
    $('#dataDownload').onclick = function () {
      var isCsv = dataMode === 'csv';
      var txt = $('#dataArea').value;
      if (!txt) { toast('没有可导出的内容'); return; }
      // CSV 前置 BOM，Excel 才会按 UTF-8 解析中文
      var ok = downloadText(
        'medreminder-' + (isCsv ? 'records' : 'backup') + '-' + fmtDate(new Date()) + (isCsv ? '.csv' : '.json'),
        (isCsv ? '\ufeff' : '') + txt,
        isCsv ? 'text/csv' : 'application/json'
      );
      toast(ok ? '已开始下载；若手机没有下载目录，改用「复制」' : '当前环境不支持下载，请改用「复制」');
    };
    $('#dataPick').onclick = function () { $('#pickBackup').click(); };
    $('#pickBackup').onchange = function (e) {
      var f = e.target.files && e.target.files[0];
      if (!f) return;
      var fr = new FileReader();
      fr.onload = function () { $('#dataArea').value = String(fr.result || ''); toast('已读入文件，确认内容后点「恢复」'); };
      fr.readAsText(f);
      e.target.value = '';   // 允许重复选择同一个文件
    };
    $('#dataApply').onclick = applyRestore;

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
      notifyPerm = browserPerm();
    }
    /* 固定时刻模式的药**不等打卡**，启动就要把今天的时刻排出来 ——
     * 否则第一个时刻的提醒永远不会响（那时用户还没打过卡）。 */
    ensureFixedDoses();
    /* ★ 2026-09-29：启动时若 localStorage 里还留着「拍照留痕」，说明上次拍照期间
     * 本 App 被系统杀掉、`appRestoredResult` 还没回放。先把界面切到「正在恢复」，
     * 并挡住新的拍照打卡 —— 否则用户会看到"今天还没打卡"而重复点（真机反馈）。
     * 留痕会在 resumeShot 里被清掉；万一事件迟迟不来（极少数），下一次正常
     * photoGate 也会通过 clearPendingShot 覆盖它，不会永久卡住。 */
    if (loadPendingShot()) restoringShot = true;
    render();
    /* 兜底：留痕可能是"上次拍照被取消/失败后没清干净"的残留（正常路径都会清，
     * 但异常路径不敢保证）。给它一个上限 —— 超过 30 秒没有任何恢复事件到达，
     * 就清掉留痕、解除锁定，别让用户永远点不了打卡。 */
    if (restoringShot) {
      setTimeout(function () {
        if (!restoringShot) return;              // 已被 resumeShot 正常处理
        restoringShot = false;
        clearPendingShot();
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
    lastDay = todayKey();
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
