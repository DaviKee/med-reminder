/* ui/today.js —— TODAY 视图：渲染 + 事件绑定 + 跳过确认
 *
 * 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 */
import { $, $$, esc, minToStr, nowMin } from '../core/util.js';
import { S, save } from '../core/store.js';
import { ackDropped, anyScheduledToday, checkIn, checkInAll, courseState, dueAt, isMissed, markTaken, medById, missedDoses, nextPending, progress, sortedDoses, todayDoses } from '../core/schedule.js';
import { closeDlg, openDlg } from './overlay.js';
import { render } from './render.js';
import { toast } from './toast.js';
import { dismissFsHint, syncNotifications, takenToast } from './actions.js';
import { askNotify, onPermCardTap, permCardHtml } from './permission.js';
import { ICON, doseClockHtml, droppedCardHtml, fsHintDismissed, fsHintHtml, medCardHtml, snoozeTag, storageAlertHtml } from './cards.js';
import { openPhoto, photoGate, restoringShot, stampPhoto } from './photo.js';
import { openSheet } from './sheet.js';
import { setTab } from './tabs.js';
import { fontScale } from './fontsize.js';

export function renderToday() {
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

    /* S-3：今天没有任何一种药该吃（周期规则下的休息日）—— **别催打卡**。
     * 老人看到「今天还没打卡」会以为漏了，其实今天本来就什么都不用吃。
     * 仍然把药品列表亮出来，让他能确认"药还在，只是今天不吃"。 */
    if (S.meds.length && !anyScheduledToday()) {
      /* ★ S-4：区分"疗程结束"与"周期休息日" —— 前者是**永久性**的，
       * 只写"今天不用吃药"会让人以为明天就恢复了，于是一直等。
       * （个别药用完只是短暂休息，这才是那句轻描淡写的正确场景。） */
      var allEnded = S.meds.every(function (m) { return courseState(m) === 'ended'; });
      html += '<div class="sect" style="gap:10px">'
        + '<span class="eyebrow">TODAY</span>'
        + '<h1 class="h1">今天不用吃药</h1>'
        + '<p class="body">' + (allEnded
            ? '所有药的疗程都已结束，不会再提醒了。要接着吃，就点下面的药把结束日期往后改。'
            : '按你设的服用周期，今天没有要服的药。到了该吃的日子会自动出现在这里。')
        + '</p>'
        + '</div>'
        + '<div class="sect">'
        + '<span class="eyebrow">MY MEDS</span>'
        + '<div class="list">';
      S.meds.forEach(function (m) { html += medCardHtml(m, true); });
      html += '</div></div>';
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

export function countdownText(nxt) {
  if (!nxt) return '今天已无待服用的药。';
  var diff = dueAt(nxt) - nowMin();
  if (diff <= 0) return '现在就该吃药了。';
  var h = Math.floor(diff / 60), m = diff % 60;
  return '距下次服药还有 ' + (h ? h + ' 小时 ' + m + ' 分' : m + ' 分钟');
}

export function bindToday() {
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

export var pendingSkipId = null;

/* 跳过确认框（#dlgSkip）的两个按钮 —— 与 `pendingSkipId` 同住。
 *
 * 为什么不留在 boot()：`pendingSkipId` 的**完整生命周期**都在这条链上
 * （今日页点「跳过」→ 设值 + 开框；这里取消 / 确认 → 清空）。
 * 硬拆开就得给它配 setter，反而更绕 —— 同 sheet 的 bindSheet。 */
export function bindSkipDlg() {
  $('#skipCancel').onclick = function () { pendingSkipId = null; closeDlg($('#dlgSkip')); };
  $('#skipConfirm').onclick = function () {
    var l = todayDoses();
    for (var i = 0; i < l.length; i++) {
      if (l[i].id === pendingSkipId) { l[i].status = 'skipped'; break; }
    }
    save(); pendingSkipId = null; closeDlg($('#dlgSkip')); render(); toast('已跳过本次');
  };
}
