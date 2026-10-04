/* ui/records.js —— 记录页：渲染 + 历史统计 / 列表 / 筛选
 *
 * 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 */
import { $, $$, esc, fmtDate, minToStr, nowMin, pad, todayKey } from '../core/util.js';
import { S } from '../core/store.js';
import { MISS_GRACE_MIN, dueAt, medById } from '../core/schedule.js';
import { openDlg } from './overlay.js';
import { render } from './render.js';
import { toast } from './toast.js';
import { ICON, storageAlertHtml } from './cards.js';
import { diagHtml, invalidatePermProbe, refreshPerm } from './permission.js';
import { FS_LEVELS, applyFontScale, fontScale, fsLabel } from './fontsize.js';
import { THEME_MODES, applyTheme, themeMode, themeLabel } from './theme.js';
import { openPhoto, photoTally } from './photo.js';
import { autoBackupCardHtml, openCleanDlg, openDataDlg, openReportDlg, storageCardHtml } from './data.js';

/* ---------------- render: RECORDS ---------------- */
export function renderRecords() {
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

  /* 显示设置：主题 + 字号。
   *
   * 主题放这里而不是做成"跟随系统就完事"：老人手机设置多为浅色，
   * 若只跟随系统，等于**替他们做了决定**且无从更改 —— 给三个明确的档位更可控。
   * 默认仍「深色」，与改造前一致（见 ui/theme.js 的注释）。 */
  html += '<div class="card" style="display:flex;flex-direction:column;gap:12px">'
    + '<div style="display:flex;align-items:baseline;justify-content:space-between;gap:12px">'
    + '<span class="eyebrow">显示 · 主题</span>'
    + '<span class="meta" id="thNow">' + esc(themeLabel()) + '</span></div>'
    + '<div class="chip-row" id="thRow">'
    + THEME_MODES.map(function (m) {
        var on = m.id === themeMode;
        /* ⚠️ 属性名用 data-theme-mode，**不能**用 data-theme ——
         * 那个已被 <html> 占用做主题标记（见 ui/theme.js）。 */
        return '<button class="chip fs-chip' + (on ? ' on' : '') + '" data-theme-mode="' + m.id + '"'
          + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + m.label + '</button>';
      }).join('')
    + '</div>'
    + '<p class="body" style="margin:0">「跟随系统」需要手机已开启深色模式。</p>'

    /* 字号：只放大文字，不动布局。老年人看不清小字是真实痛点，而整页缩放会带来左右拖动。
     * 放在这里而不是做成双指手势 —— 手势缩放文字是非标准交互，且会与列表滚动抢事件；
     * 档位按钮可发现、可预期，也符合「文字可放大到 200% 而不丢内容」的无障碍要求。 */
    + '<div style="display:flex;align-items:baseline;justify-content:space-between;gap:12px;margin-top:6px;border-top:1px solid var(--line);padding-top:14px">'
    + '<span class="eyebrow">显示 · 字号</span>'
    + '<span class="meta" id="fsNow">' + esc(fsLabel()) + '</span></div>'
    + '<div class="chip-row" id="fsRow">'
    + FS_LEVELS.map(function (lv) {
        var on = lv.v === fontScale;
        return '<button class="chip fs-chip' + (on ? ' on' : '') + '" data-fs="' + lv.v + '"'
          + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + lv.label + '</button>';
      }).join('')
    + '</div>'
    + '<p class="body" style="margin:0">只放大文字，页面布局不变。</p>'
    + '</div>';

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
    + '<button class="btn btn-ghost" id="btnReport" style="height:44px;font-size:calc(14px * var(--fs))">给医生看的报告（近 7 天）</button>'
    + '<button class="btn btn-ghost" id="btnRestore" style="height:44px;font-size:calc(14px * var(--fs))">从备份恢复</button>'
    + '</div>';

  html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
    + '<span class="eyebrow">DEBUG · 验收用</span>'
    + '<p class="body">想立刻确认提醒能不能正常响？点下面按钮，10 秒后会收到一条测试通知（息屏 / 锁屏也能测，不会写入任何服药记录）。</p>'
    + '<button class="btn btn-ghost" id="btnTest" style="align-self:flex-start;margin-top:2px">测试提醒 · 10 秒后响一次</button>'
    + diagHtml()
    + '<p class="hint">' + esc(versionLine()) + '</p>'
    + '</div>';

  host.innerHTML = html;

  var tb = $('#btnTest');
  /* 处理器由 app.js 在 boot 里注入（`setTestReminder`）——
   * 它依赖提醒流程（showReminder），那还在 app.js，不能反向 import。 */
  if (tb && testReminderImpl) tb.onclick = testReminderImpl;

  var pb = $('#btnPurgeNotif');
  if (pb) pb.onclick = function () {
    if (!(window.MedNotify && window.MedNotify.purge)) { toast('当前是浏览器模式，没有系统通知可清'); return; }
    toast('正在清除并重新登记…');
    window.MedNotify.purge().then(function () { return window.MedNotify.stat(); })
      .then(function (s) {
        invalidatePermProbe();        // 让诊断行立刻刷新
        refreshPerm();
        if (s && s.delivered) toast('仍剩 ' + s.delivered + ' 条在通知栏，可手动左滑划掉');
        else toast('已清除全部已登记的提醒');
      });
  };

  $$('[data-theme-mode]').forEach(function (el) {
    el.onclick = function () {
      var m = el.getAttribute('data-theme-mode');
      if (m === themeMode) return;
      applyTheme(m, true);
      render();                       // 重建本页 → 按钮选中态跟着更新
      toast('主题已设为「' + themeLabel() + '」');
    };
  });

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
  var brp = $('#btnReport');
  if (brp) brp.onclick = openReportDlg;
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

export function calcStreak() {
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

export function hasTaken(k) {
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
export function adherenceStats(medId) {
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
export function calcAdherence(medId) {
  return adherenceStats(medId).rate;
}

/* 记录页的药品筛选：null = 全部 */
export var histMedId = null;

export var HIST_DAYS = 14;

/* 近 N 天里「有记录」的日期键，新的在前 */
export function historyDays(limit) {
  return Object.keys(S.doses)
    .filter(function (k) { return (S.doses[k] || []).length; })
    .sort().reverse().slice(0, limit || HIST_DAYS);
}

/* 历史分组标题：今天单独标出来，其余用"月-日 周X" */
export function histDayLabel(k) {
  if (k === todayKey()) return '今天 · ' + k;
  var wd = ['日', '一', '二', '三', '四', '五', '六'];
  var p = k.split('-');
  var d = new Date(parseInt(p[0], 10), parseInt(p[1], 10) - 1, parseInt(p[2], 10));
  return k.slice(5) + ' 周' + wd[d.getDay()];
}

/* 历史行的状态文案。历史日期上的 pending 一律是「已错过」，
 * 不能拿 isMissed() 直接判（它按"现在"算）。 */
export function histStatusLabel(x, k) {
  if (x.status === 'taken') return x.makeup ? '补记' : '已服用';
  if (x.status === 'skipped') return '已跳过';
  if (k !== todayKey() || nowMin() - dueAt(x) > MISS_GRACE_MIN) return '已错过';
  return '未到时间';
}

/* 筛选 chips。**页面与二级菜单里各渲染一份** ——
 * 页面上那份还在驱动「本月依从率」（按药看依从率是有意义的），
 * 菜单里那份是因为菜单盖住了页面、够不着。两份都带 data-hist，走同一套委托处理。 */
export function histChipsHtml() {
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
export function histDayMatch(k) {
  return !histMedId || (S.doses[k] || []).some(function (x) { return x.medId === histMedId; });
}

/* 某天要显示的剂量（已按计划时刻排序） */
export function histDayDoses(k) {
  return (S.doses[k] || [])
    .filter(function (x) { return !histMedId || x.medId === histMedId; })
    .slice().sort(function (a, b) { return a.time - b.time; });
}

export function histDays() {
  return historyDays(HIST_DAYS).filter(histDayMatch);
}

/* 一行剂量（历史列表与今日页排版一致，只是没有可点的打卡按钮） */
export function histDoseRowHtml(x, k) {
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
export function historyListHtml() {
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
export function historySummaryHtml() {
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
export function openHistoryDlg() {
  var hdays = histDays();
  $('#histHint').textContent = hdays.length
    ? '近 ' + HIST_DAYS + ' 天里共 ' + hdays.length + ' 天有记录。点上面的药品名可按药筛选。'
    : '近 ' + HIST_DAYS + ' 天还没有服药记录。';
  refreshHistoryDlg(true);
  openDlg($('#dlgHistory'));
}

/* 重绘菜单内容。force=false 时只在菜单开着才做（筛选变化时被调用）。 */
export function refreshHistoryDlg(force) {
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

/* 历史筛选（chips）与「查看全部历史」——**事件委托**，挂在 document 上只注册一次。
 *
 * 为什么用委托而不是逐个 onclick：chips 在页面与二级菜单里各有一份，
 * 而菜单内容会被重绘 —— 绑在元素上的 click 会随 innerHTML 重建而失效，
 * 表现成"点了没反应"（项目踩过一次，见 MEMORY-数据与交互）。
 * 与 `histMedId` 同住：它的写点就在这里。 */
export function bindHistoryFilter() {
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
}

/* ---------------- 由 app.js 注入的两样东西 ----------------
 *
 * 为什么不直接 import：
 *  · **版本号**（`APP_VERSION` / `APP_BUILD`）住在 app.js 的 IIFE 里 ——
 *    既搬不出来（`build-apk.sh` 会往 app.js 写 `APP_BUILD`），
 *    也不能 import（会与 app.js 形成**循环依赖**）。
 *  · **测试提醒**（`testReminder`）依赖提醒流程（`showReminder`），那部分还在 app.js。
 *
 * 套路与 `setRenderers` / `setPhotoStats` 一致：**底层定入口，上层注入实现**。 */
var versionInfo = { version: '', build: '' };
var testReminderImpl = null;

export function setVersionInfo(version, build) {
  versionInfo = { version: version, build: build };
}

export function setTestReminder(fn) {
  testReminderImpl = fn;
}

export function versionLine() {
  return '版本 v' + versionInfo.version + ' · ' + versionInfo.build;
}
