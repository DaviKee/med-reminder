/* ui/viz.js —— S-6 数据可视化：月历 / 月度统计 / 漏服时段 / 计划 vs 实际偏差
 *
 * 设计取舍（都写在对应函数的注释里，这里说三条总的）：
 *
 * 1. **统计口径只有一份** —— `statsRange()` 与记录页的 `adherenceStats` 必须
 *    完全同口径（补记不算分子、漏服只算过宽限期的）。所以把计数逻辑**下沉到这里**，
 *    `adherenceStats` 变成薄委托。两份实现迟早漂移，漂移了用户不知道该信哪个。
 *
 * 2. **不读 `histMedId`**（历史筛选）—— records.js 已经 import 本模块，
 *    再反向读它的状态就是循环依赖。月历是「全部药」的月度概览，筛选只作用于
 *    下面的历史列表 —— 两块用途不同，不必强行一致。
 *
 * 3. **`viz` 状态与 `bindViz` 同住** —— 翻月状态的写点全在绑定里（同
 *    `bindSkipDlg` / `bindDataDlg` 的「状态与绑定同住」纪律）。
 *
 * 全部颜色走 CSS 变量（S-5 硬约束）：新增 `--ok` / `--on-ok`，两套主题都有定义。
 */

import { S } from '../core/store.js';
import { dueAt, MISS_GRACE_MIN } from '../core/schedule.js';
import { pad, fmtDate, todayKey, nowMin } from '../core/util.js';
import { openDlg, closeDlg } from './overlay.js';
import { photoTally } from './photo.js';

/* ---------------- 状态（只由 bindViz 写） ---------------- */

/* 当前查看的月份。y 为 0 表示「未初始化」（首次渲染时取当月）。
 * m 用 0-11（与 Date.getMonth 一致）。 */
export var viz = { y: 0, m: 0 };

function todayParts() {
  var d = new Date();
  return { y: d.getFullYear(), m: d.getMonth(), key: todayKey() };
}

/* 把日期键转成本地时区的 ms —— ⚠️ 不能用 Date.parse（按 UTC 解析，东八区差 8 小时）。
 * 已内联进 devStats：它是那里的唯一使用者，模块级私有小函数会在 spec 的
 * cut() 沙箱里变成隐式依赖（工程纪律第 5 条，这个坑踩过四次）。 */

/* ---------------- 统计（口径的唯一实现） ----------------

 * 分子 = 真实打卡（无 makeup 标记）。
 * 分母 = 已结算剂量 = 真实打卡 + 补记 + 主动跳过 + 已过宽限期的漏服。
 * 「今天还没到时刻」的 pending **不算漏** —— 未来不能预判缺席。
 *
 * 返回：st（合计）+ days（按天细分，月历用；adherenceStats 忽略它）。 */
export function statsRange(prefix, medId) {
  var tk = todayKey(), now = nowMin();
  var st = { real: 0, makeup: 0, skipped: 0, missed: 0, total: 0, rate: null, days: {} };
  Object.keys(S.doses).forEach(function (k) {
    if (prefix && k.indexOf(prefix) !== 0) return;
    var isToday = k === tk;
    (S.doses[k] || []).forEach(function (x) {
      if (medId && x.medId !== medId) return;
      var day = st.days[k] || (st.days[k] = { real: 0, makeup: 0, skipped: 0, missed: 0, total: 0, pending: 0 });
      if (x.status === 'taken') {
        if (x.makeup) { st.makeup++; day.makeup++; }
        else { st.real++; day.real++; }
        return;
      }
      if (x.status === 'skipped') { st.skipped++; day.skipped++; return; }
      if (x.status === 'pending') {
        if (!isToday || now - dueAt(x) > MISS_GRACE_MIN) { st.missed++; day.missed++; }
        else { day.pending++; }
      }
    });
  });
  Object.keys(st.days).forEach(function (k) {
    var d = st.days[k];
    d.total = d.real + d.makeup + d.skipped + d.missed;
  });
  st.total = st.real + st.makeup + st.skipped + st.missed;
  st.rate = st.total ? Math.round(st.real / st.total * 100) : null;
  return st;
}

/* 月历格子状态。返回 '' / 'future' / 'none' / 'good' / 'partial' / 'bad' / 'part'。
 * 字段做了缺省兜底 —— 调用方正常都来自 statsRange().days（全字段），但别赌。 */
export function dayStatus(cell, key, isToday) {
  if (key > todayParts().key) return 'future';           // 还没到的日子
  if (!cell.total) return 'none';                        // 没排程（或全被撤了）
  if (cell.pending) return 'part';                       // 今天还有没结算的 → 进行中
  if (!cell.missed) return 'good';                       // 全结算且无漏（含全跳过）
  if ((cell.real || 0) + (cell.makeup || 0) > 0) return 'partial';   // 有吃有漏
  return 'bad';                                          // 一次没吃
}

/* 漏服按**计划时刻**分桶 —— 回答「总是在哪个时段漏」。
 * 只统计漏服（missed），按 `d.time`（计划分钟数）落桶。 */
export function missedBuckets(prefix, medId) {
  var tk = todayKey(), now = nowMin();
  var buckets = [
    { label: '清晨（9 点前）', count: 0, max: 540 },
    { label: '上午（9–12 点）', count: 0, max: 720 },
    { label: '下午（12–18 点）', count: 0, max: 1080 },
    { label: '晚上（18 点后）', count: 0, max: 1440 }
  ];
  Object.keys(S.doses).forEach(function (k) {
    if (prefix && k.indexOf(prefix) !== 0) return;
    var isToday = k === tk;
    (S.doses[k] || []).forEach(function (x) {
      if (medId && x.medId !== medId) return;
      if (x.status !== 'pending') return;
      if (isToday && now - dueAt(x) <= MISS_GRACE_MIN) return;   // 还没结算的不算
      var t = x.time || 0;
      var b = t < 540 ? 0 : t < 720 ? 1 : t < 1080 ? 2 : 3;
      buckets[b].count++;
    });
  });
  return buckets;
}

/* 计划 vs 实际偏差 —— 竞品没有的数据（依赖我们的打卡时间戳）。
 * 只统计**真实打卡**（补记没有可信的「实际时刻」）。
 * 偏差 = takenAt - 计划时刻(ms)。负数（提前吃）按 0 算 —— 「提前 40 分钟」
 * 和「按时」在医嘱意义上是一回事，分开说反而让人焦虑。 */
export function devStats(prefix, medId) {
  var st = { count: 0, ontime: 0, late: 0, verylate: 0, lateSum: 0 };
  Object.keys(S.doses).forEach(function (k) {
    if (prefix && k.indexOf(prefix) !== 0) return;
    var kp = k.split('-');
    var base = new Date(+kp[0], +kp[1] - 1, +kp[2]).getTime();   // 本地时区（见函数头注释）
    (S.doses[k] || []).forEach(function (x) {
      if (medId && x.medId !== medId) return;
      if (x.status !== 'taken' || x.makeup || !x.takenAt) return;
      var dev = (x.takenAt - (base + (x.time || 0) * 60000)) / 60000;
      if (dev < 0) dev = 0;
      st.count++;
      if (dev <= 10) { st.ontime++; return; }
      if (dev <= 60) { st.late++; st.lateSum += dev; return; }
      st.verylate++; st.lateSum += dev;
    });
  });
  st.lateAvg = (st.late + st.verylate) ? Math.round(st.lateSum / (st.late + st.verylate)) : null;
  return st;
}

/* ---------------- 渲染 ---------------- */

var WD = ['一', '二', '三', '四', '五', '六', '日'];

function esc(s) {
  return String(s == null ? '' : s).replace(/[&<>"']/g, function (c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
  });
}

function daysInMonth(y, m) { return new Date(y, m + 1, 0).getDate(); }

export function vizHtml() {
  var t = todayParts();
  if (!viz.y) { viz.y = t.y; viz.m = t.m; }              // 首次：当月
  var prefix = viz.y + '-' + pad(viz.m + 1);
  var st = statsRange(prefix, null);

  var atMonth = (viz.y === t.y && viz.m === t.m);
  var lastValid = (viz.y > t.y || (viz.y === t.y && viz.m > t.m));   // 理论到不了，防御
  var html = '<div class="card" style="display:flex;flex-direction:column;gap:14px">'
    + '<div style="display:flex;align-items:center;justify-content:space-between;gap:10px">'
    + '<span class="eyebrow">月历 · ' + viz.y + ' 年 ' + (viz.m + 1) + ' 月</span>'
    + '<span style="display:flex;gap:8px">'
    + '<button class="btn btn-ghost viz-nav" data-viz-nav="-1" aria-label="上个月" style="width:44px;height:36px;padding:0">‹</button>'
    + '<button class="btn btn-ghost viz-nav" data-viz-nav="1" aria-label="下个月"'
    + (atMonth || lastValid ? ' disabled style="width:44px;height:36px;padding:0;opacity:.4"' : ' style="width:44px;height:36px;padding:0"')
    + '>›</button></span></div>';

  /* 月历网格：7 列，周一开头（与页面顶部 THIS WEEK 同一约定）。 */
  var offset = (new Date(viz.y, viz.m, 1).getDay() + 6) % 7;
  var n = daysInMonth(viz.y, viz.m);
  var rows = Math.ceil((offset + n) / 7);
  html += '<div class="cal">';
  html += '<div class="cal-row cal-head">' + WD.map(function (w) { return '<span class="cal-w">' + w + '</span>'; }).join('') + '</div>';
  for (var r = 0; r < rows; r++) {
    html += '<div class="cal-row">';
    for (var c = 0; c < 7; c++) {
      var idx = r * 7 + c - offset + 1;
      if (idx < 1 || idx > n) { html += '<span class="cal-cell empty"></span>'; continue; }
      var key = viz.y + '-' + pad(viz.m + 1) + '-' + pad(idx);
      var cell = st.days[key];
      var stat = dayStatus(cell || { total: 0 }, key, key === t.key);
      var cls = 'cal-cell' + (stat ? ' ' + stat : '') + (key === t.key ? ' today' : '');
      html += '<span class="' + cls + '">' + idx + '</span>';
    }
    html += '</div>';
  }
  html += '</div>';

  /* 图例 —— 颜色语义必须当场说清，不然「橙色 = 什么」只能靠猜。 */
  html += '<div class="cal-legend">'
    + '<span class="lg"><i class="cal-cell good"></i>全服</span>'
    + '<span class="lg"><i class="cal-cell partial"></i>有漏</span>'
    + '<span class="lg"><i class="cal-cell bad"></i>全漏</span>'
    + '<span class="lg"><i class="cal-cell part"></i>进行中</span>'
    + '</div>';

  /* 月度统计 —— 数字与记录页顶部同口径，只是范围变成「所选月份」。 */
  html += '<div class="cal-stats">'
    + '<span>真实打卡 <b>' + st.real + '</b></span>'
    + '<span>补记 <b>' + st.makeup + '</b></span>'
    + '<span>跳过 <b>' + st.skipped + '</b></span>'
    + '<span>漏服 <b>' + st.missed + '</b></span>'
    + '<span>依从率 <b>' + (st.rate === null ? '—' : st.rate + '%') + '</b></span>'
    + '</div>';

  /* 漏服时段 —— 回答「总在哪个时段漏」，帮用户调整生活安排。 */
  var mb = missedBuckets(prefix, null);
  var maxC = Math.max.apply(null, mb.map(function (b) { return b.count; }));
  html += '<div style="display:flex;flex-direction:column;gap:6px">'
    + '<span class="eyebrow">漏服时段（按计划时刻）</span>';
  if (!maxC) {
    html += '<p class="hint" style="margin:0">这个月没有漏服。</p>';
  } else {
    mb.forEach(function (b) {
      if (!b.count) return;
      var w = Math.max(6, Math.round(b.count / maxC * 100));
      html += '<div class="bar-row"><span class="bar-lb">' + esc(b.label) + '</span>'
        + '<span class="bar"><i style="width:' + w + '%"></i></span>'
        + '<span class="bar-n">' + b.count + '</span></div>';
    });
  }
  html += '</div>';

  /* 计划 vs 实际 —— 竞品普遍没有的一项（我们有打卡时间戳）。 */
  var dv = devStats(prefix, null);
  html += '<div style="display:flex;flex-direction:column;gap:6px">'
    + '<span class="eyebrow">计划 vs 实际（真实打卡）</span>';
  if (!dv.count) {
    html += '<p class="hint" style="margin:0">这个月还没有按时打卡的记录。</p>';
  } else {
    html += '<p class="body" style="margin:0">按时（±10 分钟内）<b>' + dv.ontime + '</b> 次 · '
      + '晚点 <b>' + (dv.late + dv.verylate) + '</b> 次'
      + (dv.lateAvg ? '（平均晚 ' + dv.lateAvg + ' 分钟）' : '') + '</p>';
    if (dv.verylate) {
      html += '<p class="hint" style="margin:0">有 ' + dv.verylate
        + ' 次晚了一小时以上 —— 如果总在同一个药上，看看是不是间隔设得太紧。</p>';
    }
  }
  /* 拍照打卡统计 —— 原在记录页铺着，并入本浮层（统计性质，见 2026-10-07 整合） */
  var ph = photoTally();
  if (ph.shot + ph.skipped > 0) {
    var tot = ph.shot + ph.skipped;
    html += '<p class="hint" style="margin:0">本月拍照打卡 ' + ph.shot + '/' + tot + ' 次'
      + (ph.skipped ? (' · 未拍照 ' + ph.skipped + ' 次（' + Math.round(ph.skipped / tot * 100) + '%）') : '')
      + '</p>';
  }
  html += '</div></div>';
  return html;
}

/* 打开「月历与统计」浮层。内容每次打开 / 翻月都重算 —— 状态在 viz 里，刷新即最新。 */
export function openVizDlg() {
  if (!viz.y) { var t = todayParts(); viz.y = t.y; viz.m = t.m; }
  document.getElementById('vizBody').innerHTML = vizHtml();
  openDlg(document.getElementById('dlgViz'));
}

/* 翻月绑定：挂在 **document** 上只注册一次（boot 里调）。
 * 记录页的 innerHTML 每次重绘都会把按钮换掉，绑在元素上会失效
 * ——「事件委托绑到不会被重建的容器」是本项目的既定纪律。 */
export function bindViz() {
  document.addEventListener('click', function (ev) {
    var el = ev.target;
    while (el && el !== document && !(el.getAttribute && el.getAttribute('data-viz-nav'))) {
      el = el.parentNode;
    }
    if (!el || el === document) return;
    if (el.hasAttribute('disabled')) return;               // 已经到当月，不许再往后翻
    var dir = parseInt(el.getAttribute('data-viz-nav'), 10) || 0;
    if (!dir) return;
    var t = todayParts();
    if (!viz.y) { viz.y = t.y; viz.m = t.m; }
    var y = viz.y, m = viz.m + dir;
    if (m < 0) { m = 11; y--; }
    if (m > 11) { m = 0; y++; }
    if (y > t.y || (y === t.y && m > t.m)) return;         // 不看未来
    if (y < 2020) return;                                  // 别翻到没有意义的年代
    viz.y = y; viz.m = m;
    var body = document.getElementById('vizBody');
    if (body) body.innerHTML = vizHtml();   // 只刷浮层内容，不动背后的页面
  });
}
