/* ui/cards.js —— 共享渲染片段：药品卡片 / 剂量行 / 图标 / 提示卡
 *
 * 2026-10-01 架构重构（A-2 第 6 步 · 第一块）：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * ⚠️ **这批能整体搬走，是因为它们只读底层状态（core/*），不碰 app.js 的可变状态。**
 *    当初 `fsLabel`（读 fontScale）与 `permCardHtml`（读 notifyPerm）就没搬 —— 那两个变量
 *    在 app.js 里被**整体赋值**，import 绑定只读，搬走就 TypeError。
 *    ✅ 两者后来各自随状态归位了：`fsLabel` → `ui/fontsize.js`（v1.5.11）、
 *       `permCardHtml` → `ui/permission.js`（v1.5.9）。
 *    以后往这里加片段，守着同一条：**只依赖 core，绝不反向依赖 app.js**。
 */
import { esc, minToStr, minOfDay } from '../core/util.js';
import { storageError } from '../core/store.js';
import { intervalLabel, medMode, normTimes, todayDoseCount, stockInfo,
         droppedToday, droppedAcked, schedLabel, scheduledToday,
         courseState, courseLabel } from '../core/schedule.js';

/* 服药方式的统一文案（药品卡 / 药品列表 / 提醒弹窗共用） */
export function medScheduleLabel(m) {
  var cyc = schedLabel(m);                       // '每天' / '每周一三五' / '每 2 天'
  if (medMode(m) === 'fixed') {
    var ts = normTimes(m && m.times);
    /* 一个时刻都没设 → 它还**不算一个排程**，连「每天」都不说 */
    if (!ts.length) return '未设时刻';
    var t = ts.map(minToStr).join(' / ');
    /* 每天时不加周期前缀（那是常态），但**「每天」两个字要留着** ——
     * 否则描述退化成光秃秃的 "08:00 / 20:00"，看不出是每日重复。 */
    return cyc === '每天' ? ('每天 ' + t) : (cyc + ' · ' + t);
  }
  var iv = '每 ' + intervalLabel(m && m.interval);   // 自带「每」，daily 时不再加前缀
  return cyc === '每天' ? iv : (cyc + ' · ' + iv);
}

/* 药品卡下面那行说明 */
export function medMetaText(m) {
  var n = todayDoseCount(m.id);
  if (n) return '今日 ' + n + ' 次 · 已排程';
  /* ★ S-4：疗程状态**优先于**周期 —— "疗程已结束"比"今天不用吃"信息量大得多，
   * 而且它是**永久性**的（不处理就永远不提醒），必须让用户一眼看见。 */
  var st = courseState(m);
  if (st === 'ended') return courseLabel(m) + ' · 疗程已结束，不再提醒';
  if (st === 'before') return courseLabel(m) + ' · 还没到开始日';
  /* S-3：今天不该吃 —— 必须**明说**。否则用户看到"没排程"会以为坏了，
   * 或者干等着一个永远不会来的提醒。 */
  if (!scheduledToday(m)) return schedLabel(m) + ' · 今天不用吃';
  if (medMode(m) === 'fixed') {
    return normTimes(m.times).length ? '今天还没排程 · 回「今天」页刷新' : '还没设时刻，去编辑里加一个';
  }
  return '每 ' + intervalLabel(m.interval) + ' · 打卡后开始计时';
}

/* 展示用：已服用的剂量显示**真实打卡时刻**（takenAt），未服用/跳过显示计划时刻。
 * 旧记录没有 takenAt（此字段 2026-09-14 前只写不读），用 ≈ 标出这是按计划推定、非真实打卡。 */
export function doseClockHtml(ds) {
  if (ds.status === 'taken' && ds.takenAt) return minToStr(minOfDay(ds.takenAt));
  if (ds.status === 'taken') return '<span title="旧记录：按计划时刻推定，非真实打卡时刻">≈' + minToStr(ds.time) + '</span>';
  return minToStr(ds.time);
}

/* 一次性提示：把"字可以调大"告诉需要它的人（A-1） */
export var FS_HINT_KEY = 'medreminder.fsHint.v1';

export function fsHintDismissed() {
  try { return localStorage.getItem(FS_HINT_KEY) === '1'; } catch (e) { return true; }
}

export function fsHintHtml() {
  return '<button class="card" id="btnFsHint" style="display:flex;flex-direction:column;gap:6px;width:100%;cursor:pointer;text-align:left;-webkit-tap-highlight-color:transparent">'
    + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs))">字太小看不清？</span>'
    + '<span class="meta">到「记录」页可以把字调大（大 / 特大），布局不会乱。点这里去看看。</span></button>';
}

export function droppedCardHtml() {
  var n = droppedToday();
  if (!n || droppedAcked()) return '';
  return '<div class="card card-miss" style="display:flex;flex-direction:column;gap:6px">'
    + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:#FF7A17">有 ' + n
    + ' 次服药排到了次日，今天不再提醒</span>'
    + '<span class="meta">这次打卡比较晚，按间隔本该排到明天凌晨。今天不会再响，建议明早打卡后补记一次。</span>'
    + '<button class="btn btn-ghost" id="btnDropAck" style="align-self:flex-start;min-height:44px;margin-top:2px">知道了</button>'
    + '</div>';
}

/* 已延后的剂量在排程行上标出来。不标的话，用户会以为"刚才明明延后了，
 * 怎么列表还是原来的时间"——其实计划时刻本就该保持原样，是响铃时刻变了。 */
export function snoozeTag(ds) {
  if (!ds || ds.status !== 'pending' || ds.snoozeUntil == null) return '';
  return '<span class="snooze-tag">延后至 ' + esc(minToStr(ds.snoozeUntil)) + '</span>';
}

/* ---------------- icons ---------------- */
export var ICON = {
  check: '<svg width="14" height="14" viewBox="0 0 24 24" fill="none"><path d="M4.5 12.5 9.5 17.5 19.5 7" style="stroke:var(--muted)" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  pill: '<svg width="18" height="18" viewBox="0 0 24 24" fill="none"><rect x="3" y="9" width="18" height="6" rx="3" transform="rotate(-45 12 12)" stroke="currentColor" stroke-width="1.8"/><path d="M9 9l6 6" stroke="currentColor" stroke-width="1.8"/></svg>',
  chev: '<svg width="16" height="16" viewBox="0 0 24 24" fill="none"><path d="M9.5 5.5 16 12l-6.5 6.5" style="stroke:var(--muted)" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  cam: '<svg width="15" height="15" viewBox="0 0 24 24" fill="none"><path d="M3.5 8.5c0-1.1.9-2 2-2h1.6c.6 0 1.1-.3 1.4-.8l.5-.9c.3-.5.8-.8 1.4-.8h4.2c.6 0 1.1.3 1.4.8l.5.9c.3.5.8.8 1.4.8h1.6c1.1 0 2 .9 2 2v8c0 1.1-.9 2-2 2H5.5c-1.1 0-2-.9-2-2v-8Z" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/><circle cx="12" cy="12.5" r="3.4" stroke="currentColor" stroke-width="1.7"/></svg>'
};

/* ---------------- render: TODAY ---------------- */
/* ★ S-1（2026-10-01）：剂量标签 —— 把 dose / unit 拼成「每次 1 片」。
 * 两个字段都可缺，优雅降级：只有单位 → 「每次 片」；都没有 → 空串（调用方不渲染）。
 * 纯函数，便于单测（tests/med-fields.spec.js）。 */
export function doseLabel(m) {
  if (!m) return '';
  var d = (m.dose == null ? '' : String(m.dose)).trim();
  var u = (m.unit == null ? '' : String(m.unit)).trim();
  if (!d && !u) return '';
  if (!d) return '每次 ' + u;
  return '每次 ' + d + (u ? ' ' + u : '');
}

/* ★ S-1：有值就写、没值就**删属性** ——
 * 让「没填」只有一种表示法（属性不存在）。这样旧数据（本来就没这字段）
 * 与新数据的空值形状一致，导出 / 统计不必区分「空串」与「没有此字段」。
 * ⚠️ 判空用 `'' / null / undefined`，**不能用 falsy** ——
 *    S-2 的剩余量是数字，`0`（吃完了）是**合法值**，`if (val)` 会把它当"没填"删掉。 */
export function setOrDel(obj, key, val) {
  if (val === '' || val == null) delete obj[key];
  else obj[key] = val;
}

/* ★ S-2：库存展示行。返回 '' 表示这药没填剩余量 → **完全不占位置**（旧数据友好）。
 * 文案分三档：① 能算出天数 ② 只知剩余量（剂量未知，**不猜天数**）③ 低库存警告。 */
export function stockHtml(m) {
  var s = stockInfo(m);
  if (!s) return '';
  var unit = (m.unit == null ? '' : String(m.unit)).trim();
  var left = s.stock + (unit ? ' ' + unit : '');
  if (s.days == null) return '<p class="meta">还剩 ' + esc(left) + '</p>';
  if (s.low) {
    return '<p class="meta" style="color:#FF7A17">⚠ 只剩 ' + esc(left)
      + '，约够 ' + s.days + ' 天 —— 该买药了</p>';
  }
  return '<p class="meta">还剩 ' + esc(left) + ' · 约够 ' + s.days + ' 天</p>';
}

export function medCardHtml(m, clickable) {
  return '<' + (clickable ? 'button' : 'div') + ' class="card medcard' + (clickable ? ' tappable' : '') + '"'
    + (clickable ? ' data-checkin="' + m.id + '"' : '')
    + ' style="display:flex;flex-direction:column;gap:14px;text-align:left;color:inherit;font:inherit">'
    + '<div class="card-row">'
    + '<span style="font-size:calc(16px * var(--fs));line-height:calc(22px * var(--fs))">' + esc(m.name) + '</span>'
    + '<span class="pill">' + esc(medScheduleLabel(m)) + '</span>'
    + '</div>'
    /* ★ S-1 / S-2：剂量、库存、备注各自成行 —— 没有就不占位置（不渲染空占位行） */
    + (doseLabel(m) ? '<p class="meta">' + esc(doseLabel(m)) + '</p>' : '')
    + stockHtml(m)
    + (m.note ? '<p class="meta">' + esc(m.note) + '</p>' : '')
    + '<p class="meta">' + esc(medMetaText(m)) + '</p>'
    + '</' + (clickable ? 'button' : 'div') + '>';
}

/* 写失败告警卡：**跨页可见**（今日页与记录页都放）。
 * 用户可能整天停在今日页打卡，只在记录页提示等于没提示。 */
export function storageAlertHtml() {
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
