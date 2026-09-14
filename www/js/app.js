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

  function todayDoses() {
    var k = todayKey();
    if (!S.doses[k]) S.doses[k] = [];
    return S.doses[k];
  }
  function medById(id) { for (var i = 0; i < S.meds.length; i++) if (S.meds[i].id === id) return S.meds[i]; return null; }
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

    if (window.MedNotify && window.MedNotify.native && notifyPerm === 'denied') {
      html += '<button class="card" id="btnPerm" style="display:flex;flex-direction:column;gap:6px;width:100%;border-color:#FF7A17;cursor:pointer">'
        + '<span style="font-size:14px;line-height:20px;color:#FF7A17">通知权限未开启</span>'
        + '<span class="meta">锁屏和息屏时收不到服药提醒，点这里去开启。</span></button>';
    }

    if (!list.length) {
      // ---- 未打卡 ----
      html += '<div class="sect" style="gap:10px">'
        + '<span class="eyebrow">TODAY</span>'
        + '<h1 class="h1">今天还没打卡</h1>'
        + '<p class="body">早晨第一次服药后点下方按钮，我们会按你设定的间隔依次提醒今天的每一次。</p>'
        + '</div>';

      html += '<div class="sect">'
        + '<span class="eyebrow">MY MEDS</span>'
        + '<div class="list">';
      if (!S.meds.length) {
        html += '<div class="card"><p class="body" style="text-align:center">还没有药品，先去「药品」页添加。</p></div>';
      } else {
        S.meds.forEach(function (m) { html += medCardHtml(m, true); });
      }
      html += '</div>';
      if (S.meds.length > 1) html += '<p class="hint" style="margin-top:-4px">点击卡片可只打卡某一种药。</p>';
      html += '</div>';

      html += '<div style="padding-top:4px">'
        + '<button class="btn btn-primary" id="btnCheckin"' + (S.meds.length ? '' : ' disabled style="opacity:.4"') + '>打卡 · 开始今天的提醒</button>'
        + '</div>';
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
        } else if (isNext) {
          right = '<span class="pill accent">待服用</span>';
        } else {
          right = '<span class="dose-s"><span class="txt">待服用</span></span>';
        }
        html += '<div class="dose">'
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
    if (perm) perm.onclick = function () {
      if (window.MedNotify) window.MedNotify.requestPermission().then(function (p) { notifyPerm = p; render(); });
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
    host.innerHTML = html;

    var add = $('#btnAdd');
    if (add) add.onclick = function () { openSheet(null); };
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
      + '<span class="eyebrow">DEBUG · 验收用</span>'
      + '<p class="body">想立刻确认提醒能不能正常响？点下面按钮，10 秒后会收到一条测试通知（息屏 / 锁屏也能测，不会写入任何服药记录）。</p>'
      + '<button class="btn btn-ghost" id="btnTest" style="align-self:flex-start;margin-top:2px">测试提醒 · 10 秒后响一次</button>'
      + '</div>';

    host.innerHTML = html;

    var tb = $('#btnTest');
    if (tb) tb.onclick = testReminder;
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
    if (window.MedNotify) {
      window.MedNotify.requestPermission().then(function (p) { notifyPerm = p; render(); });
      return;
    }
    try {
      if ('Notification' in window && Notification.permission === 'default') Notification.requestPermission();
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

  /* ---------------- esc ---------------- */
  function esc(s) {
    return String(s).replace(/[&<>"']/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
  }

  /* ---------------- render dispatcher ---------------- */
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
    // 首屏种子数据，避免空态
    if (!S.meds.length && !Object.keys(S.doses).length) {
      S.meds = [
        { id: uid(), name: '维生素 D3', interval: 8 },
        { id: uid(), name: '阿莫西林', interval: 6 }
      ];
      save();
    }
    // 过期提醒静默
    var now = nowMin();
    todayDoses().forEach(function (d) {
      if (d.status === 'pending' && d.time < now && now - d.time > 30) S.notified[d.id] = 1;
    });
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

    if (window.MedNotify && window.MedNotify.native) {
      window.MedNotify.init(onNotifyAction);
      window.MedNotify.checkPermissions().then(function (p) { notifyPerm = p; render(); });
    }
    render();
    syncNotifications();
    setTab('today');
    tick();
    setInterval(tick, 1000);

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
