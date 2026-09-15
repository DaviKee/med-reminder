/* 定时服药提醒 — app logic
 * 打卡制：每天第一次服药打卡后，按药品间隔自动排程当天剩余提醒。
 */
(function () {
  'use strict';

  var $ = function (s, r) { return (r || document).querySelector(s); };
  var $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };
  var pad = function (n) { return String(n).padStart(2, '0'); };
  var uid = function () { return 'd' + Date.now().toString(36) + Math.random().toString(36).slice(2, 7); };

  /* ---------------- date / time helpers ---------------- */
  function fmtDate(d) { return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()); }
  function nowMin() { var d = new Date(); return d.getHours() * 60 + d.getMinutes(); }
  function minToStr(m) { m = ((m % 1440) + 1440) % 1440; return pad(Math.floor(m / 60)) + ':' + pad(m % 60); }
  function minOfDay(ms) { var d = new Date(ms); return d.getHours() * 60 + d.getMinutes(); }
  /* 展示用：已服用的剂量显示**真实打卡时刻**（takenAt），未服用/跳过显示计划时刻。
   * 旧记录没有 takenAt（此字段 2026-09-14 前只写不读），用 ≈ 标出这是按计划推定、非真实打卡。 */
  function doseClockHtml(ds) {
    if (ds.status === 'taken' && ds.takenAt) return minToStr(minOfDay(ds.takenAt));
    if (ds.status === 'taken') return '<span title="旧记录：按计划时刻推定，非真实打卡时刻">≈' + minToStr(ds.time) + '</span>';
    return minToStr(ds.time);
  }
  function todayKey() { return fmtDate(new Date()); }

  /* ---------------- state ---------------- */
  var KEY = 'medreminder.v1';
  var DEFAULT = { meds: [], doses: {}, notified: {} };

  function load() {
    try {
      var raw = localStorage.getItem(KEY);
      if (raw) {
        var o = JSON.parse(raw);
        return { meds: o.meds || [], doses: o.doses || {}, notified: o.notified || {} };
      }
    } catch (e) { /* ignore */ }
    return JSON.parse(JSON.stringify(DEFAULT));
  }
  var S = load();
  var notifyPerm = 'unknown';   // granted / denied / unsupported / unknown
  function save() {
    try { localStorage.setItem(KEY, JSON.stringify(S)); } catch (e) { /* ignore */ }
    syncNotifications();
  }

  /* ---------------- 后台通知登记 ---------------- */
  function dateAt(min) { var d = new Date(); d.setHours(Math.floor(min / 60), min % 60, 0, 0); return d; }
  function syncNotifications() {
    if (!window.MedNotify) return;
    var list = todayDoses()
      .filter(function (d) { return d.status === 'pending'; })
      .map(function (d) {
        var m = medById(d.medId);
        return { id: d.id, timeStr: minToStr(d.time), medName: m ? m.name : '服药', at: dateAt(d.time) };
      });
    window.MedNotify.sync(list);
  }

  /* 逾期太久的待服剂量不再补响 —— 启动与「恢复备份」共用同一套判断。
   * 注意：这只是「不再响」，不等于「算你服了」或「算你跳过」——
   * 剂量本身仍是 pending，由 missedDoses() 识别成「已错过」交给用户处理。 */
  function silenceOverdue() {
    var now = nowMin();
    todayDoses().forEach(function (d) {
      if (d.status === 'pending' && d.time < now && now - d.time > MISS_GRACE_MIN) S.notified[d.id] = 1;
    });
  }

  /* 「已错过」判定：仍是待服，但计划时刻已经过去很久。
   * 过去这些剂量被静默标记为已通知、界面上却依然写着「待服用」——
   * 用户既不知道自己漏了药，也无从补记。现在显式暴露出来。 */
  var MISS_GRACE_MIN = 30;
  function isMissed(d) {
    return d.status === 'pending' && nowMin() - d.time > MISS_GRACE_MIN;
  }
  function missedDoses() {
    return sortedDoses().filter(isMissed);
  }

  function todayDoses() {
    var k = todayKey();
    if (!S.doses[k]) S.doses[k] = [];
    return S.doses[k];
  }
  function medById(id) { for (var i = 0; i < S.meds.length; i++) if (S.meds[i].id === id) return S.meds[i]; return null; }

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

  function todayDoseCount(medId) {
    return todayDoses().filter(function (d) { return d.medId === medId; }).length;
  }

  /* ---------------- check-in / scheduling ---------------- */
  function checkIn(medId) {
    var list = todayDoses();
    var med = medById(medId);
    if (!med) return false;
    for (var i = 0; i < list.length; i++) if (list[i].medId === medId) return false; // 今天已排过
    var start = nowMin();
    var step = med.interval * 60;
    var arr = [];
    for (var t = start, idx = 0; t < 1440; t += step, idx++) {
      arr.push({
        id: uid(), medId: medId, time: t,
        status: idx === 0 ? 'taken' : 'pending',
        takenAt: idx === 0 ? Date.now() : null
      });
    }
    if (!arr.length) return false;
    arr.forEach(function (d, i) { d.idx = i; d.total = arr.length; });
    Array.prototype.push.apply(list, arr);
    save();
    return true;
  }
  function checkInAll() {
    var n = 0;
    S.meds.forEach(function (m) { if (checkIn(m.id)) n++; });
    return n;
  }

  /* 记录一次服药：写入真实打卡时刻，并让同一药品后续未服的剂量按「实际服药时刻 + 间隔」顺延。
   * 这是 2026-09-14 修掉的核心缺陷 —— 此前整天的计划在当天第一次打卡时就一次算死，
   * 迟服不会顺延，且界面上永远显示计划时刻。 */
  function markTaken(dose, takenMs) {
    dose.status = 'taken';
    dose.takenAt = takenMs;
    return rollForward(dose, takenMs);
  }

  /* 顺延：把该药今天**排在这一针之后**的 pending，从 takenMs 起按间隔重排。
   * 只顺延「时间在原计划之后」的剂量 —— 更早且已逾期的剂量不受影响，
   * 否则它们会被推到次日而遭丢弃，等于把漏服记录抹掉。
   * 越过今天 24:00 的不再排（留到明天重新打卡），避免出现当天永不触发的死条目。 */
  function rollForward(dose, takenMs) {
    var med = medById(dose.medId);
    if (!med) return { shifted: 0, dropped: 0 };
    var step = med.interval * 60;
    var plannedAt = dose.time;
    var rest = todayDoses()
      .filter(function (d) { return d.medId === dose.medId && d.status === 'pending' && d.time > plannedAt; })
      .sort(function (a, b) { return a.time - b.time; });
    var base = minOfDay(takenMs) + step;
    var dropIds = [], shifted = 0;
    rest.forEach(function (d, i) {
      var t = base + i * step;
      if (t >= 1440) { dropIds.push(d.id); return; }
      d.time = t;
      S.notified[d.id] = 0;   // 时刻变了，允许按新时刻重新提醒
      shifted++;
    });
    if (dropIds.length) {
      S.doses[todayKey()] = todayDoses().filter(function (d) { return dropIds.indexOf(d.id) < 0; });
    }
    reindexMed(dose.medId);
    return { shifted: shifted, dropped: dropIds.length };
  }

  /* 顺延后重排序号，保证「第 N / 共 M 次」仍然正确 */
  function reindexMed(medId) {
    var arr = todayDoses()
      .filter(function (d) { return d.medId === medId; })
      .sort(function (a, b) { return a.time - b.time; });
    arr.forEach(function (d, i) { d.idx = i; d.total = arr.length; });
  }

  /* 打卡后的提示语：说清真实时刻，以及后续是否顺延 */
  function takenToast(med, takenMs, r) {
    var s = '已记录 ' + minToStr(minOfDay(takenMs)) + ' · ' + (med ? med.name : '');
    if (r && r.shifted) s += '，后续 ' + r.shifted + ' 次已顺延';
    if (r && r.dropped) s += '，' + r.dropped + ' 次越过零点不再提醒';
    return s;
  }

  function sortedDoses() {
    return todayDoses().slice().sort(function (a, b) { return a.time - b.time; });
  }
  function nextPending() {
    var l = sortedDoses();
    for (var i = 0; i < l.length; i++) if (l[i].status === 'pending') return l[i];
    return null;
  }
  function progress() {
    var l = todayDoses();
    var done = l.filter(function (d) { return d.status === 'taken' || d.status === 'skipped'; }).length;
    return { done: done, total: l.length };
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
    chev: '<svg width="16" height="16" viewBox="0 0 24 24" fill="none"><path d="M9.5 5.5 16 12l-6.5 6.5" stroke="#7D8187" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>'
  };

  /* ---------------- render: TODAY ---------------- */
  function medCardHtml(m, clickable) {
    return '<' + (clickable ? 'button' : 'div') + ' class="card medcard' + (clickable ? ' tappable' : '') + '"'
      + (clickable ? ' data-checkin="' + m.id + '"' : '')
      + ' style="display:flex;flex-direction:column;gap:14px;text-align:left;color:inherit;font:inherit">'
      + '<div class="card-row">'
      + '<span style="font-size:16px;line-height:22px">' + esc(m.name) + '</span>'
      + '<span class="pill">每 ' + m.interval + ' 小时</span>'
      + '</div>'
      + '<p class="meta">' + (todayDoseCount(m.id) ? ('今日 ' + todayDoseCount(m.id) + ' 次 · 已排程') : ('每 ' + m.interval + ' 小时 · 打卡后开始计时')) + '</p>'
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

    /* 权限提示：原生与浏览器两种模式下都可能有权限问题，
     * 之前只在原生 + denied 时提示，PWA 用户被拒授权后既收不到提醒也无任何说明。 */
    var permCard = permCardHtml();
    if (permCard) html += permCard;

    if (!list.length) {
      // ---- 未打卡 ----
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
        + '<h1 class="h1 num" style="font-size:40px;line-height:44px">' + (nxt ? minToStr(nxt.time) : '已完成') + '</h1>'
        + '<p class="body" id="countdown">' + countdownText(nxt) + '</p>'
        + '</div>';

      // 漏服可见化：显式告诉用户「漏了几次」，并给一键补记入口
      var missed = missedDoses();
      if (missed.length) {
        html += '<button class="card card-miss" id="btnMissAll" style="display:flex;flex-direction:column;gap:6px;width:100%;cursor:pointer;-webkit-tap-highlight-color:transparent">'
          + '<span style="font-size:14px;line-height:20px;color:#FF7A17">今天漏了 ' + missed.length + ' 次服药</span>'
          + '<span class="meta">' + missed.map(function (d) { return minToStr(d.time); }).join('、')
          + ' 这几次没有打卡记录。已经吃过就点这里补记，没吃就留意一下。</span>'
          + '<span class="meta" style="color:#FF7A17">点此把漏掉的都记为已服用</span></button>';
      }

      html += '<div class="card sched" style="padding-left:20px;padding-right:20px;padding-top:8px;padding-bottom:8px">';
      sorted.forEach(function (ds) {
        var med = medById(ds.medId);
        var isNext = nxt && ds.id === nxt.id;
        var tCls = isNext ? 'dose-t now' : 'dose-t';
        var nCls = ds.status === 'taken' ? 'dose-n' : (isNext ? 'dose-n on' : 'dose-n dim');
        var right;
        if (ds.status === 'taken') {
          right = '<span class="dose-s">' + ICON.check + '<span class="txt">已服用</span></span>';
        } else if (ds.status === 'skipped') {
          right = '<span class="dose-s"><span class="txt">已跳过</span></span>';
        } else if (isMissed(ds)) {
          // 已错过：不只是文案，给一个能补记的按钮，别让用户没法挽救
          right = '<button class="btn-miss" data-makeup="' + esc(ds.id) + '" '
            + 'aria-label="补记 ' + esc(minToStr(ds.time)) + ' 这次服药">已错过 · 补记</button>';
        } else if (isNext) {
          right = '<span class="pill accent">待服用</span>';
        } else {
          right = '<span class="dose-s"><span class="txt">待服用</span></span>';
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
    var diff = nxt.time - nowMin();
    if (diff <= 0) return '现在就该吃药了。';
    var h = Math.floor(diff / 60), m = diff % 60;
    return '距下次服药还有 ' + (h ? h + ' 小时 ' + m + ' 分' : m + ' 分钟');
  }

  function bindToday() {
    var perm = $('#btnPerm');
    if (perm) perm.onclick = function () { askNotify(); };

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
        askNotify();
        var id = el.getAttribute('data-checkin');
        var med = medById(id);
        if (checkIn(id)) {
          render();
          toast((med ? med.name : '') + ' 已打卡 · 排了 ' + todayDoses().filter(function (d) { return d.medId === id; }).length + ' 次提醒');
        } else {
          toast('今天已经打过卡了');
        }
      };
    });
    var b = $('#btnCheckin');
    if (b) b.onclick = function () {
      askNotify();
      var n = checkInAll();
      render();
      toast(n ? '已打卡 · 今天共排了 ' + todayDoses().length + ' 次提醒' : '今天已经打过卡了');
    };
    var fm = $('#btnFirstMed');
    if (fm) fm.onclick = function () { setTab('meds'); openSheet(null); };
    var e = $('#btnEarly');
    if (e) e.onclick = function () {
      var nxt = nextPending(); if (!nxt) return;
      var med = medById(nxt.medId);
      var at = Date.now();
      var r = markTaken(nxt, at);
      save(); render();
      toast(takenToast(med, at, r));
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
          + '<span class="m">每 ' + m.interval + ' 小时 · ' + (todayDoseCount(m.id) ? ('今日 ' + todayDoseCount(m.id) + ' 次') : '打卡后开始计时') + '</span></span>'
          + ICON.chev + '</button>';
      });
      html += '</div>';
    }
    html += '<p class="hint" style="margin-top:-8px">点击任意药品，可修改名称与服药间隔。</p>';

    var legacy = legacySampleMeds();
    if (legacy.length) {
      html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
        + '<span class="eyebrow">SAMPLE · 示例数据</span>'
        + '<p class="body">检测到 ' + legacy.length + ' 个从未使用过的示例药品（'
        + esc(legacy.map(function (m) { return m.name; }).join('、'))
        + '）。旧版本首次启动时会自动生成它们，不是你手动添加的。</p>'
        + '<button class="btn btn-ghost" id="btnDropLegacy" style="height:44px;font-size:14px">删除这 ' + legacy.length + ' 项</button>'
        + '</div>';
    }

    host.innerHTML = html;

    var add = $('#btnAdd');
    if (add) add.onclick = function () { openSheet(null); };
    var dl = $('#btnDropLegacy');
    if (dl) dl.onclick = function () {
      var ids = legacySampleMeds().map(function (m) { return m.id; });
      if (!ids.length) return;
      S.meds = S.meds.filter(function (m) { return ids.indexOf(m.id) < 0; });
      save(); render();
      toast('已删除 ' + ids.length + ' 个示例药品');
    };
    $$('.meditem').forEach(function (el) {
      el.onclick = function () { openSheet(el.getAttribute('data-med')); };
    });
  }

  /* ---------------- render: RECORDS ---------------- */
  function renderRecords() {
    var host = $('#recordsView');
    var html = '<h1 class="h1">服药记录</h1>';

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
    var rate = calcAdherence();
    html += '<div class="statrow">'
      + '<div class="stat"><span class="eyebrow">STREAK</span><span class="meta">连续打卡</span>'
      + '<span class="v"><span class="n">' + streak + '</span><span class="u">天</span></span></div>'
      + '<div class="stat"><span class="eyebrow">ADHERENCE</span><span class="meta">本月依从率</span>'
      + '<span class="v"><span class="n">' + (rate === null ? '—' : rate) + '</span><span class="u">%</span></span></div>'
      + '</div>';

    html += '<p class="hint" style="margin-top:-8px">每次服药后打卡，记录会自动更新。</p>';

    html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
      + '<span class="eyebrow">DATA · 备份</span>'
      + '<p class="body">记录只存在这台手机上：清缓存、换手机都会丢，也没法直接拿给医生看。定期导出留一份。</p>'
      + '<div class="btnrow" style="margin-top:2px">'
      + '<button class="btn btn-primary" id="btnBackup" style="height:44px;font-size:14px">导出备份</button>'
      + '<button class="btn btn-ghost" id="btnCsv" style="height:44px;font-size:14px">导出 CSV</button>'
      + '</div>'
      + '<button class="btn btn-ghost" id="btnRestore" style="height:44px;font-size:14px">从备份恢复</button>'
      + '</div>';

    html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
      + '<span class="eyebrow">DEBUG · 验收用</span>'
      + '<p class="body">想立刻确认提醒能不能正常响？点下面按钮，10 秒后会收到一条测试通知（息屏 / 锁屏也能测，不会写入任何服药记录）。</p>'
      + '<button class="btn btn-ghost" id="btnTest" style="align-self:flex-start;margin-top:2px">测试提醒 · 10 秒后响一次</button>'
      + '</div>';

    host.innerHTML = html;

    var tb = $('#btnTest');
    if (tb) tb.onclick = testReminder;
    var bb = $('#btnBackup');
    if (bb) bb.onclick = function () { openDataDlg('backup'); };
    var bc = $('#btnCsv');
    if (bc) bc.onclick = function () { openDataDlg('csv'); };
    var br = $('#btnRestore');
    if (br) br.onclick = function () { openDataDlg('restore'); };
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
  function calcAdherence() {
    var d = new Date();
    var prefix = d.getFullYear() + '-' + pad(d.getMonth() + 1);
    var taken = 0, missed = 0;
    Object.keys(S.doses).forEach(function (k) {
      if (k.indexOf(prefix) !== 0) return;
      S.doses[k].forEach(function (x) {
        if (x.status === 'taken') taken++;
        else if (x.status === 'skipped') missed++;
      });
    });
    var total = taken + missed;
    if (!total) return null;
    return Math.round(taken / total * 100);
  }

  /* ---------------- overlays ---------------- */
  var openCount = 0;
  function setScrim(on) { $('#scrim').classList.toggle('show', on); }
  function openSheet(medId) {
    editingId = medId;
    var med = medId ? medById(medId) : null;
    $('#sheetTitle').textContent = med ? '编辑药品' : '添加药品';
    $('#medName').value = med ? med.name : '';
    stepVal = med ? med.interval : 8;
    $('#deleteMed').classList.toggle('hidden', !med);
    renderPreview();
    $('#sheetMed').classList.add('show');
    setScrim(true); openCount++;
  }
  function closeSheet() { $('#sheetMed').classList.remove('show'); setScrim(false); openCount = 0; }
  function openDlg(el) { el.classList.add('show'); openCount++; }
  function closeDlg(el) { el.classList.remove('show'); openCount = 0; }

  /* ---------------- sheet: stepper + save ---------------- */
  var stepVal = 8, editingId = null, pendingSkipId = null;

  function renderPreview() {
    var box = $('#previewChips');
    var arr = [];
    for (var t = 360, i = 0; t < 1440 && i < 8; t += stepVal * 60, i++) arr.push(minToStr(t));
    box.innerHTML = arr.map(function (t) { return '<span class="chip">' + t + '</span>'; }).join('');
    $('#stepNum').textContent = String(stepVal);
  }
  function clampStep() { if (stepVal < 1) stepVal = 1; if (stepVal > 24) stepVal = 24; }

  /* ---------------- reminder ---------------- */
  function tick() {
    // clock
    var d = new Date();
    $('#sbTime').textContent = d.getHours() + ':' + pad(d.getMinutes());

    var cd = $('#countdown');
    if (cd) cd.textContent = countdownText(nextPending());

    var list = todayDoses();
    var now = nowMin();
    for (var i = 0; i < list.length; i++) {
      var ds = list[i];
      if (ds.status !== 'pending') continue;
      if (S.notified[ds.id]) continue;
      if (ds.time > now) continue;
      if (now - ds.time > 30) { S.notified[ds.id] = 1; save(); continue; } // 过期太久，静默
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
    $('#remindMeta').textContent = minToStr(ds.time) + ' · 第 ' + (ds.idx + 1) + ' / ' + ds.total + ' 次 · 每 ' + (med ? med.interval : '-') + ' 小时';
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
    if (action === 'resume') { render(); syncNotifications(); return; }
    var l = todayDoses(), ds = null;
    for (var i = 0; i < l.length; i++) if (l[i].id === doseId) { ds = l[i]; break; }
    if (!ds) return;
    if (action === 'taken') {
      if (ds.status !== 'pending') return;
      var at = Date.now();
      var med = medById(ds.medId);
      var r = markTaken(ds, at);
      if (window.MedNotify) window.MedNotify.cancelOne(ds.id);
      save(); render(); toast(takenToast(med, at, r));
    } else if (action === 'snooze') {
      ds.time = Math.min(1439, ds.time + 10);
      S.notified[ds.id] = 0;
      save(); render(); toast('10 分钟后再提醒你');
    } else if (action === 'open') {
      if (ds.status === 'pending') showReminder(ds); else render();
    }
  }

  /* 测试提醒：验证锁屏/息屏能否正常响，不写任何服药数据 */
  function testReminder() {
    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.test(10000).then(function (ok) {
        toast(ok ? '已登记测试提醒，请留意锁屏 / 通知栏（约 10 秒后）' : '测试提醒登记失败，请确认通知权限已开');
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

  /* 导入前严格校验：宁可拒绝，也不要让半截数据覆盖掉用户现有记录 */
  function parseBackup(txt) {
    if (!txt || !String(txt).trim()) return { err: '请先粘贴备份内容' };
    var o;
    try { o = JSON.parse(txt); } catch (e) { return { err: '内容不是合法 JSON，可能复制不完整' }; }
    if (!o || o.format !== BACKUP_FORMAT) return { err: '这不是本 App 导出的备份' };
    if (typeof o.version !== 'number' || o.version > BACKUP_VERSION) return { err: '备份来自更新版本的 App' };
    if (!o.data || !Array.isArray(o.data.meds) || !o.data.doses || typeof o.data.doses !== 'object') {
      return { err: '备份内容不完整' };
    }
    var badMed = !o.data.meds.every(function (m) {
      return m && typeof m.id === 'string' && typeof m.name === 'string' && typeof m.interval === 'number';
    });
    if (badMed) return { err: '备份里的药品数据有问题' };
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

  function copyText(text) {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      return navigator.clipboard.writeText(text).then(
        function () { return true; },
        function () { return legacyCopy(); }
      );
    }
    return Promise.resolve(legacyCopy());
  }
  /* 剪贴板权限在各 WebView 上差异很大，兜底用「全选 + execCommand」 */
  function legacyCopy() {
    var ta = $('#dataArea');
    if (!ta) return false;
    var ro = ta.readOnly;
    ta.readOnly = false;
    ta.focus();
    ta.select();
    var ok = false;
    try { ok = document.execCommand('copy'); } catch (e) { ok = false; }
    ta.readOnly = ro;
    try { ta.setSelectionRange(0, 0); } catch (e) { /* ignore */ }
    return ok;
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

  /* ---------------- esc ---------------- */
  function esc(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
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
        + '<span style="font-size:14px;line-height:20px;color:var(--sub)">当前环境不支持通知</span>'
        + '<span class="meta">提醒只在 App 页面前台时弹出，切走或锁屏不会响。装到手机上才能锁屏提醒。</span></div>';
    }

    // 未申请：不吓唬用户，只在浏览器模式提示，点了才发起授权
    if (!isNative && notifyPerm === 'prompt') {
      return '<button class="card" id="btnPerm" style="display:flex;flex-direction:column;gap:6px;width:100%;border-color:#FF7A17;cursor:pointer;-webkit-tap-highlight-color:transparent">'
        + '<span style="font-size:14px;line-height:20px;color:#FF7A17">还没开启通知</span>'
        + '<span class="meta">开启后才能及时收到服药提醒，点这里授权。</span></button>';
    }

    // 已被拒绝：原生引导去系统设置，浏览器只能让用户去浏览器设置里改
    if (notifyPerm === 'denied') {
      var hint = isNative
        ? '锁屏和息屏时收不到服药提醒。点这里重新发起授权；若系统不再弹窗，请到「设置 → 应用 → MedReminder → 通知」里手动打开。'
        : '浏览器已拒绝通知，需要到浏览器的网站权限设置里把通知改回「允许」，然后刷新页面。';
      return '<button class="card" id="btnPerm" style="display:flex;flex-direction:column;gap:6px;width:100%;border-color:#FF7A17;cursor:pointer;-webkit-tap-highlight-color:transparent">'
        + '<span style="font-size:14px;line-height:20px;color:#FF7A17">通知权限被拒绝 · 收不到提醒</span>'
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

  function refreshPerm() {
    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.checkPermissions().then(function (p) {
        if (p !== 'unsupported' && p !== 'unknown') { notifyPerm = p; render(); }
      });
      return;
    }
    var b = browserPerm();
    if (b !== notifyPerm) { notifyPerm = b; render(); }
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

  /* ---------------- boot ---------------- */
  function boot() {
    // 不再预置任何示例药品：服药场景里「看起来像真药」的假数据会造成误导，
    // 用户可能以为自己在吃阿莫西林。空态改为引导（见 renderToday 的 HOW IT WORKS）。
    // 过期提醒静默
    silenceOverdue();
    save();

    $$('.tab').forEach(function (t) {
      t.onclick = function () { setTab(t.getAttribute('data-tab')); };
    });

    $('#scrim').onclick = function () { closeSheet(); };
    $('#sheetClose').onclick = closeSheet;
    $('#stepMinus').onclick = function () { stepVal--; clampStep(); renderPreview(); };
    $('#stepPlus').onclick = function () { stepVal++; clampStep(); renderPreview(); };

    $('#saveMed').onclick = function () {
      var name = $('#medName').value.trim();
      if (!name) { toast('请填写药品名称'); $('#medName').focus(); return; }
      if (editingId) {
        var m = medById(editingId);
        if (m) {
          var changed = m.interval !== stepVal;
          m.name = name; m.interval = stepVal;
          if (changed) { // 间隔变了，重排今天还没吃的
            var l = todayDoses().filter(function (d) { return d.medId === m.id && d.status === 'taken'; });
            var keep = todayDoses().filter(function (d) { return d.medId !== m.id; });
            if (l.length) {
              var start = l[0].time, step = stepVal * 60, arr = [];
              for (var t = start, i = 0; t < 1440; t += step, i++) {
                arr.push({ id: uid(), medId: m.id, time: t, status: i === 0 ? 'taken' : 'pending', takenAt: i === 0 ? l[0].takenAt : null });
              }
              arr.forEach(function (d, i) { d.idx = i; d.total = arr.length; });
              S.doses[todayKey()] = keep.concat(arr);
            }
          }
        }
        toast('已保存');
      } else {
        S.meds.push({ id: uid(), name: name, interval: stepVal });
        toast('已添加 ' + name);
      }
      save(); closeSheet(); render();
    };

    $('#deleteMed').onclick = function () {
      if (!editingId) return;
      S.meds = S.meds.filter(function (m) { return m.id !== editingId; });
      S.doses[todayKey()] = todayDoses().filter(function (d) { return d.medId !== editingId; });
      save(); closeSheet(); render(); toast('已删除');
    };

    $('#skipCancel').onclick = function () { pendingSkipId = null; closeDlg($('#dlgSkip')); };
    $('#skipConfirm').onclick = function () {
      var l = todayDoses();
      for (var i = 0; i < l.length; i++) {
        if (l[i].id === pendingSkipId) { l[i].status = 'skipped'; break; }
      }
      save(); pendingSkipId = null; closeDlg($('#dlgSkip')); render(); toast('已跳过本次');
    };

    $('#remindDone').onclick = function () {
      var msg = '已记录服药';
      if (remindDose) {
        var at = Date.now();
        var med = medById(remindDose.medId);
        var r = markTaken(remindDose, at);
        msg = takenToast(med, at, r);
        save();
      }
      closeDlg($('#dlgRemind')); remindDose = null; render(); toast(msg);
    };
    $('#remindSnooze').onclick = function () {
      if (remindDose) {
        remindDose.time = Math.min(1439, remindDose.time + 10);
        S.notified[remindDose.id] = 0; save();
      }
      closeDlg($('#dlgRemind')); remindDose = null; render(); toast('10 分钟后再提醒你');
    };

    /* ---- 备份 / 导出 / 恢复 ---- */
    $('#dataClose').onclick = function () { restoreArmed = false; closeDlg($('#dlgData')); };
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

    /* Esc 关闭浮层。只对「非打断式」浮层生效：
     * 提醒弹窗仍要求用户明确选择「已服用 / 稍后」，不能被一键抹掉。 */
    document.addEventListener('keydown', function (e) {
      if (e.key !== 'Escape' && e.key !== 'Esc') return;
      var closable = $$('.dlg-wrap.show').filter(function (el) {
        return el.id === 'dlgData' || el.id === 'dlgSkip';
      });
      if (closable.length) {
        restoreArmed = false;
        closeDlg(closable[closable.length - 1]);
        return;
      }
      if ($('#sheetMed').classList.contains('show')) closeSheet();
    });

    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.init(onNotifyAction);
      refreshPerm();
    } else {
      // 浏览器模式过去从不检查权限，导致被拒后没有任何提示
      notifyPerm = browserPerm();
    }
    render();
    syncNotifications();
    setTab('today');
    tick();
    setInterval(tick, 1000);
    setInterval(refreshPerm, 5000);   // 用户可能刚去系统设置里改了权限，回到前台要能自动反映

    // 跨天自动刷新
    var lastDay = todayKey();
    setInterval(function () {
      if (todayKey() !== lastDay) { lastDay = todayKey(); S.notified = {}; save(); render(); syncNotifications(); }
    }, 30000);

    // PWA
    if ('serviceWorker' in navigator) {
      window.addEventListener('load', function () {
        navigator.serviceWorker.register('sw.js').catch(function () { /* ignore */ });
      });
    }
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
  else boot();
})();
