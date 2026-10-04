/* core/schedule.js —— 剂量数据模型与排程规则
 *
 * 2026-09-24 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * 依赖方向：只依赖 util 与 store（都在更底层）—— 不依赖 UI、不依赖通知层的编排。
 * 「写完盘之后要做什么」一律由 store 的 saveHooks 负责，本模块只管调 save()。
 * ⚠️ window.MedNotify 属于**插件层**（notify.js 的独立 IIFE），不是 UI 层，可直接用。
 */

import { uid, nowMin, minToStr, minOfDay, todayKey, fmtDate, pad, dowOf, dayDiff } from './util.js';
import { S, save, readJSON, writeJSON } from './store.js';

/* 间隔的显示（F-1）。
 * interval 以「小时」为单位存储，允许 0.5 的整数倍（0.5 = 30 分钟）。
 * 小于 1 小时说分钟更好懂；1.5 小时比 90 分钟直观，所以只对 <1 的做换算。 */
export function intervalLabel(v) {
  var h = Number(v);
  if (!isFinite(h) || h <= 0) return '-';
  if (h < 1) return Math.round(h * 60) + ' 分钟';
  return (h % 1 === 0 ? String(h) : h.toFixed(1)) + ' 小时';
}

/* ---------------- 服药方式（F-4） ----------------
 * 'interval'（默认，字段缺省即此）：打卡后按间隔滚动排程，会随实际打卡时间顺延。
 * 'fixed'：每天固定几个时刻，**到点就提醒，不随打卡顺延**。
 * 旧数据没有 mode 字段 —— 一律视为 interval，完全兼容，备份格式也不用变。 */
export function medMode(m) { return (m && m.mode === 'fixed') ? 'fixed' : 'interval'; }

/* ---------------- 服用周期（S-3） ----------------
 * 医嘱里「隔天一次」「每周一三五」很常见，而原来只有"每天"一种。
 *
 * 模型：单个对象 `m.sched`（不往 med 上撒一堆散字段）。
 *   { mode:'daily' | 'weekly' | 'cycle',
 *     weekdays:[0..6],       // weekly：0=周一（与 dowOf 同一约定）
 *     everyDays:N,           // cycle：每 N 天一次（N≥2）
 *     anchor:'YYYY-MM-DD' }  // cycle：起算日（默认用药卡创建日）
 *
 * **字段缺省 = 每天** —— 旧数据完全没有 sched，天然兼容，备份格式也不用动。 */

export var WEEK_LABELS = ['一', '二', '三', '四', '五', '六', '日'];

export function normSched(m) {
  var s = (m && m.sched) || {};
  var mode = s.mode;
  if (mode !== 'weekly' && mode !== 'cycle') mode = 'daily';
  var out = { mode: mode, weekdays: [], everyDays: 0, anchor: null };

  if (mode === 'weekly') {
    var seen = {}, w = [];
    (Array.isArray(s.weekdays) ? s.weekdays : []).forEach(function (v) {
      /* ⚠️ 先挡 null / undefined / 空串：`Number(null) === 0` → 会被静默当成
       *    **周一**。这跟 normTimes 里那个「Number(null) 变成 0 点」是同一个坑，
       *    而且更隐蔽 —— 用户少选一天，界面却显示选了周一，还不报错。
       *    （这条是 tests/cycle.spec.js 抓出来的。） */
      if (v == null || v === '') return;
      var n = Math.round(Number(v));
      if (!isFinite(n) || n < 0 || n > 6 || seen[n]) return;
      seen[n] = 1; w.push(n);
    });
    w.sort(function (a, b) { return a - b; });
    /* ⚠️ 选了「每周」但一天都没选 = **一次都不吃**，不是"退回每天"。
     *    静默退回每天会让用户以为设置没生效（而实际每天都在催他吃药）。 */
    out.weekdays = w;
  } else if (mode === 'cycle') {
    var n = Math.round(Number(s.everyDays));
    if (!isFinite(n) || n < 2) n = 2;         // 「每 1 天」就是每天，无意义；下限 2
    if (n > 90) n = 90;                       // 上限：再长就不像"周期用药"了
    out.everyDays = n;
    out.anchor = /^\d{4}-\d{2}-\d{2}$/.test(String(s.anchor)) ? String(s.anchor) : null;
  }
  return out;
}

/* 这一天该不该吃。**纯函数**（日期显式传入，不读当前时间）→ spec 直接跑矩阵。
 *
 * ⚠️ cycle 模式**没有合法 anchor 时视为「该吃」** —— 不能因为字段缺失就整天不吃：
 *    那会让异常数据/老数据变成"药无声无息地停了"，用户毫不知情。宁可多提醒。 */
export function isScheduledDay(m, dateKey) {
  var s = normSched(m);
  if (s.mode === 'daily') return true;
  if (s.mode === 'weekly') return s.weekdays.indexOf(dowOf(dateKey)) >= 0;
  if (!s.anchor) return true;
  var diff = dayDiff(s.anchor, dateKey);
  if (!isFinite(diff)) return true;
  if (diff < 0) return false;                 // 锚点之前：不吃
  return (diff % s.everyDays) === 0;
}

/* 今天该不该吃（便利函数，只有它读当前时间） */
export function scheduledToday(m) { return isScheduledDay(m, todayKey()); }

/* 有没有任何一种药今天该吃 —— 今日页用来区分「今天还没打卡」与「今天不用吃药」 */
export function anyScheduledToday() {
  return S.meds.some(function (m) { return scheduledToday(m); });
}

/* 周期描述（药品卡 / 编辑页）。 */
export function schedLabel(m) {
  var s = normSched(m);
  if (s.mode === 'daily') return '每天';
  if (s.mode === 'weekly') {
    if (!s.weekdays.length) return '未选星期';
    return '每周' + s.weekdays.map(function (i) { return WEEK_LABELS[i]; }).join('');
  }
  return '每 ' + s.everyDays + ' 天';
}

export var MAX_TIMES = 12;

/* 时刻数组归一化：取整 → 钳到 0..1439 → 去重 → 升序 → 限个数。
 * 所有接受用户输入的地方都必须过这一遍，否则脏数据会直接进排程。 */
export function normTimes(arr) {
  var out = [], seen = {};
  (arr || []).forEach(function (v) {
    /* 先挡掉 null / undefined / 空串：Number(null) === 0，
     * 会被静默当成「0 点」—— 那不是「没填」，而是「半夜提醒」，错得很安静。 */
    if (v == null || v === '') return;
    var n = Math.round(Number(v));
    if (!isFinite(n)) return;
    if (n < 0) n = 0;
    if (n > 1439) n = 1439;
    if (seen[n]) return;
    seen[n] = 1;
    out.push(n);
  });
  out.sort(function (a, b) { return a - b; });
  return out.slice(0, MAX_TIMES);
}

/* 两个**已归一化**的时刻数组是否完全相同。
 * normTimes 已做过排序与去重，所以这个比较与输入顺序无关。 */
export function sameTimes(a, b) {
  if (a.length !== b.length) return false;
  for (var i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

/* 「这次保存要不要重排今天的排程」—— 抽成纯函数有两个理由：
 *   ① 可被 tests/fixed.spec.js 单独验证；
 *   ② 防止将来又漏掉某一种变化（**H-1 就是这么漏掉的**：
 *      原判断写成 `modeChanged || intervalChanged`，而固定模式下这两者恒为 false，
 *      「只改时刻」于是被静默忽略 —— 今天页与通知仍是旧时刻，用户以为"改了没用"）。
 * prev / next 均形如 { mode, times, interval }，其中 times 必须是 normTimes 归一化过的。 */
export function needRebuildDoses(prev, next) {
  if (prev.mode !== next.mode) return true;                              // 模式换了
  /* ★ S-3：**周期变了也要重排** —— 否则「改成今天不吃」不会撒掉今天已排的剂量，
   * 与 H-1「只改时刻被忽略」是同一类静默失效（界面与通知都还是旧的）。
   * 用 JSON 比较：sched 是嵌套对象，逐字段比不划算，而且加字段时容易漏。 */
  if (JSON.stringify(prev.sched || null) !== JSON.stringify(next.sched || null)) return true;
  if (next.mode === 'fixed') return !sameTimes(prev.times, next.times);  // 固定模式：时刻变了吗
  return prev.interval !== next.interval;                                // 间隔模式：间隔变了吗
}

/* <input type="time"> 的值 → 分钟数。非法一律返回 null（不要静默当 0 点）。 */
export function timeInputToMin(v) {
  var m = /^(\d{1,2}):(\d{2})$/.exec(String(v == null ? '' : v).trim());
  if (!m) return null;
  var h = parseInt(m[1], 10), mi = parseInt(m[2], 10);
  if (!(h >= 0 && h <= 23 && mi >= 0 && mi <= 59)) return null;
  return h * 60 + mi;
}

/* 逾期太久的待服剂量不再补响 —— 启动与「恢复备份」共用同一套判断。
 * 注意：这只是「不再响」，不等于「算你服了」或「算你跳过」——
 * 剂量本身仍是 pending，由 missedDoses() 识别成「已错过」交给用户处理。 */
export function silenceOverdue() {
  var now = nowMin();
  todayDoses().forEach(function (d) {
    if (d.status === 'pending' && d.time < now && now - d.time > MISS_GRACE_MIN) S.notified[d.id] = 1;
  });
}

/* 「已错过」判定：仍是待服，但计划时刻已经过去很久。
 * 过去这些剂量被静默标记为已通知、界面上却依然写着「待服用」——
 * 用户既不知道自己漏了药，也无从补记。现在显式暴露出来。 */
export var MISS_GRACE_MIN = 30;

export function isMissed(d) {
  /* 按「下次响铃时刻」算，而不是原计划时刻：用户主动延后 10 分钟后，
   * 宽限期也应该从延后时刻起算，否则一延后就立刻被判成漏服。 */
  return d.status === 'pending' && nowMin() - dueAt(d) > MISS_GRACE_MIN;
}

export function missedDoses() {
  return sortedDoses().filter(isMissed);
}

export function todayDoses() {
  var k = todayKey();
  if (!S.doses[k]) S.doses[k] = [];
  return S.doses[k];
}

export function medById(id) { for (var i = 0; i < S.meds.length; i++) if (S.meds[i].id === id) return S.meds[i]; return null; }

export function todayDoseCount(medId) {
  return todayDoses().filter(function (d) { return d.medId === medId; }).length;
}

/* ---------------- S-2：库存推算 ----------------
 * 纯计算、不碰数据（便于单测）。要算「还能吃几天」需要三样：
 *   `med.stock`（用户填的剩余量）+ `med.dose`（每次几片）+ 每天吃几次。
 * **缺任何一样就只能「少说一点」，绝不猜** ——
 * 猜出来的「还能吃 X 天」比不显示更糟（用户会照着一个错数字决定要不要买药）。 */

/* 低库存阈值：剩余不足这么多天的量就提醒。
 * 取 3 天 —— 大致够一次「发现→买药→拿到」的往返，再少就来不及了。 */
export var LOW_STOCK_DAYS = 3;

/* 每天服几次（**平均值**，可能是小数）。
 * fixed 模式 = 时刻表长度（确定值）；
 * interval 模式 = 24 / 间隔（**近似** —— 实际是打卡后滚动排程，
 *   一天可能只有 2 次而不是 3 次，所以下面算天数时会向下取整兜底）。
 * ⚠️ S-3：周期模式必须按占比折算 —— 否则「隔天吃」被当成每天吃，
 *    库存的「预计用完日」直接**砍半**，人会照着一个错的日子去囤药。 */
export function dailyDoseCount(m) {
  if (!m) return 0;
  var base;
  if (medMode(m) === 'fixed') {
    base = normTimes(m.times).length;
  } else {
    var h = Number(m.interval);
    if (!isFinite(h) || h <= 0) return 0;
    base = Math.max(1, Math.round(24 / h));
  }
  var s = normSched(m);
  if (s.mode === 'weekly') return base * s.weekdays.length / 7;
  if (s.mode === 'cycle') return base / s.everyDays;
  return base;
}

/* 库存快照。`med` 没有 stock 字段 → 返回 **null**（上层据此完全不显示库存 UI，
 * 所以旧数据天然兼容，不会冒出「还剩 undefined」）。 */
export function stockInfo(m) {
  if (!m || m.stock == null) return null;
  var stock = Number(m.stock);
  if (!isFinite(stock) || stock < 0) return null;

  var perDay = dailyDoseCount(m);
  var dose = Number(m.dose);
  /* 剂量或每日次数未知 → 只报「还剩多少」，**算不出天数就不硬算** */
  if (!isFinite(dose) || dose <= 0 || !perDay) {
    return { stock: stock, dose: null, perDay: perDay, perDayAmount: null, days: null, runOut: null, low: false };
  }

  var perDayAmount = dose * perDay;
  /* 向下取整：宁可说「还够 2 天」也不说「还够 3 天」，后者会让人拖延 */
  var days = Math.floor(stock / perDayAmount);
  var runOut = null;
  if (stock > 0) {
    var d = new Date();
    d.setDate(d.getDate() + days);
    runOut = fmtDate(d);
  }
  return {
    stock: stock,
    dose: dose,
    perDay: perDay,
    perDayAmount: perDayAmount,
    days: days,
    runOut: runOut,
    low: stock > 0 && days <= LOW_STOCK_DAYS
  };
}

/* ---------------- check-in / scheduling ---------------- */
/* 返回新建的剂量数组（失败返回 null）。之所以要返回而不是布尔值：
 * 拍照打卡需要在记录落库后，把照片路径挂到这一次新建的剂量上。 */
export function checkIn(medId) {
  var med = medById(medId);
  if (!med) return null;
  /* ⚠️ 固定时刻模式**不走这里**：它的剂量由 ensureFixedDoses() 按时刻表预生成。
   * 若误走下面「从此刻起按间隔排」，用户设的固定时刻会被改写成"从打卡时刻起的间隔"
   * —— 看起来就是「顺延了、不按固定时间」（2026-09-28 真机反馈）。
   * 顺手补调 ensureFixedDoses()：万一今天的剂量还没生成（例如 App 在后台过夜、
   * 定时器被系统暂停），这里就是补救点。 */
  if (medMode(med) === 'fixed') { ensureFixedDoses(); return null; }
  var list = todayDoses();
  for (var i = 0; i < list.length; i++) if (list[i].medId === medId) return null; // 今天已排过
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
  if (!arr.length) return null;
  arr.forEach(function (d, i) { d.idx = i; d.total = arr.length; });
  Array.prototype.push.apply(list, arr);
  save();
  return arr;
}

/* 打卡当天所有还没排过的药；返回新建的全部剂量（摊平） */
export function checkInAll() {
  var created = [];
  S.meds.forEach(function (m) {
    var arr = checkIn(m.id);
    if (arr) created = created.concat(arr);
  });
  return created;
}

/* 记录一次服药：写入真实打卡时刻，并让同一药品后续未服的剂量按「实际服药时刻 + 间隔」顺延。
 * 这是 2026-09-14 修掉的核心缺陷 —— 此前整天的计划在当天第一次打卡时就一次算死，
 * 迟服不会顺延，且界面上永远显示计划时刻。 */
export function markTaken(dose, takenMs) {
  dose.status = 'taken';
  dose.takenAt = takenMs;
  var m = medById(dose.medId);
  /* ★ S-2：自动扣库存。
   * 放在这里、而不是各个调用点 —— markTaken 是**唯一的「服用」入口**
   * （卡片打卡 / 提醒弹窗 / 漏服补记走的都是它），放这里才不会漏一处。
   * 扣减条件：药有剩余量 **且** 剂量是正数；两者缺一就不扣（算不清就别乱扣）。 */
  if (m) deductStock(m);
  /* 固定时刻模式**不顺延**：它的语义就是「到点吃」。
   * 晚吃两小时不该把 20:00 也推成 22:00 —— 那会越推越晚，
   * 而且把医嘱规定的时刻改掉了（这正是固定时刻模式存在的理由）。 */
  if (m && medMode(m) === 'fixed') return { shifted: 0, dropped: 0 };
  return rollForward(dose, takenMs);
}

/* ★ S-2：扣一次库存。
 * ⚠️ 扣到 0 就停，**绝不出现负数** —— 负数会让「还够几天」算出负值，
 *    界面上会显示成莫名其妙的「-3 天」。
 * ⚠️ 用十分位取整抹掉浮点误差：0.5 片连扣两次不该变成 0.9999999。 */
function deductStock(m) {
  var used = Number(m.dose);
  if (m.stock == null || !isFinite(used) || used <= 0) return;
  var left = Number(m.stock) - used;
  m.stock = left > 0 ? Math.round(left * 10) / 10 : 0;
}

/* 顺延：把该药今天**排在这一针之后**的 pending，从 takenMs 起按间隔重排。
 * 只顺延「时间在原计划之后」的剂量 —— 更早且已逾期的剂量不受影响，
 * 否则它们会被推到次日而遭丢弃，等于把漏服记录抹掉。
 * 越过今天 24:00 的不再排（留到明天重新打卡），避免出现当天永不触发的死条目。 */
/* ---------------- 跨天剂量的可见化（B-2） ----------------
 * rollForward 会丢弃越过零点的剂量（22:00 打卡 + 8 小时间隔 → 次日 06:00 那次不排）。
 * 这是**设计取舍**（排到次日会与「明天首次打卡才排程」的模型冲突），但过去只在
 * toast 里一闪而过，用户很难意识到"今天少了一次"。
 * 现在改成今日页常驻一条说明，关闭后当天不再出现。
 *
 * ⚠️ 不能存进 S：load() 只还原 meds / doses / notified，额外的顶层键下次启动就没了。
 * 用独立 key 也让备份格式保持不动。 */
export var DROP_KEY = 'medreminder.dropped.v1';

export var DROP_ACK_KEY = 'medreminder.droppedAck.v1';

export function noteDropped(n) {
  if (!n) return;
  var cur = readJSON(DROP_KEY), today = todayKey();
  if (cur && cur.date === today) cur.n = (cur.n || 0) + n;
  else cur = { date: today, n: n, at: Date.now() };
  writeJSON(DROP_KEY, cur);
}

export function droppedToday() {
  var cur = readJSON(DROP_KEY);
  if (!cur || cur.date !== todayKey() || !cur.n) return 0;
  return cur.n;
}

export function droppedAcked() { return readJSON(DROP_ACK_KEY) === todayKey(); }

export function ackDropped() { writeJSON(DROP_ACK_KEY, todayKey()); }

export function rollForward(dose, takenMs) {
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
    noteDropped(dropIds.length);   // 让"今天少了一次"这件事在页面上留得住，而不是只飘一下
  }
  reindexMed(dose.medId);
  return { shifted: shifted, dropped: dropIds.length };
}

export function reindexMed(medId) {
  var arr = todayDoses()
    .filter(function (d) { return d.medId === medId; })
    .sort(function (a, b) { return a.time - b.time; });
  arr.forEach(function (d, i) { d.idx = i; d.total = arr.length; });
}

/* 打卡后的提示语：说清真实时刻，以及后续是否顺延 */
/* 固定时刻模式：**不等打卡**就把今天的时刻排出来。
 * 这是两种模式的根本差别 —— 固定时刻的语义是「到点就吃」；
 * 若等打卡才生成，第一个时刻的提醒永远不会响（那时还没打卡）。
 * 幂等：已经有该药的剂量就只把序号理顺，绝不重复生成。
 * 启动时、跨天时、以及新增/编辑成固定模式后都要调一次。 */
export function ensureFixedDoses() {
  var list = todayDoses(), changed = false;
  S.meds.forEach(function (m) {
    if (medMode(m) !== 'fixed') return;
    if (!scheduledToday(m)) return;               // S-3：今天不该吃 → 不排
    var times = normTimes(m.times);
    if (!times.length) return;                    // 没设时刻 → 不排（药品卡会提示去设置）
    if (list.some(function (d) { return d.medId === m.id; })) { reindexMed(m.id); return; }
    var arr = times.map(function (t) {
      return { id: uid(), medId: m.id, time: t, status: 'pending', takenAt: null };
    });
    arr.forEach(function (d, i) { d.idx = i; d.total = arr.length; });
    Array.prototype.push.apply(list, arr);
    changed = true;
  });
  if (changed) save();
  return changed;
}

/* 服药方式变了（间隔改了 / 模式换了）→ 重排今天还没吃的那些。
 * 三条约束：
 *   ① **已打卡的记录一律保留** —— 那是发生过的事实，不能被重排抹掉；
 *      哪怕新时刻表里没有它，也追加留着。
 *   ② **先撤掉旧时刻的系统通知** —— 时刻要变了，旧闹钟必须收回，
 *      否则它会在错误的时间响（点了还找不到对应剂量）。
 *   ③ 间隔模式下如果今天压根没打过卡，就没有排程可言（保持原样）。 */
export function rebuildTodayDoses(m) {
  var old = todayDoses().filter(function (d) { return d.medId === m.id; });
  var pending = old.filter(function (d) { return d.status === 'pending'; });
  var done = old.filter(function (d) { return d.status === 'taken'; })
    .sort(function (a, b) { return a.time - b.time; });
  var keep = todayDoses().filter(function (d) { return d.medId !== m.id; });

  pending.forEach(function (d) { if (window.MedNotify) window.MedNotify.cancelOne(d.id); });

  /* S-3：今天不该吃 → 撤掉全部**未服**的（已服的保留 —— 那是发生过的事实）。
   * 典型场景：编辑药品时把周期改成"今天不吃"，已排的剂量必须跟着消失。 */
  if (!scheduledToday(m)) {
    done.forEach(function (d, i) { d.idx = i; d.total = done.length; });
    S.doses[todayKey()] = keep.concat(done);
    return done.length;
  }

  var arr = [];
  if (medMode(m) === 'fixed') {
    var used = {};
    done.forEach(function (d) { used[d.time] = d; });
    normTimes(m.times).forEach(function (t) {
      if (used[t]) { arr.push(used[t]); delete used[t]; return; }
      arr.push({ id: uid(), medId: m.id, time: t, status: 'pending', takenAt: null });
    });
    Object.keys(used).forEach(function (k) { arr.push(used[k]); });   // 打过卡但不在新时刻表里
  } else if (done.length) {
    var step = m.interval * 60;
    for (var t = done[0].time, i = 0; t < 1440; t += step, i++) {
      if (i === 0) { arr.push(done[0]); continue; }
      arr.push({ id: uid(), medId: m.id, time: t, status: 'pending', takenAt: null });
    }
    done.slice(1).forEach(function (d) { arr.push(d); });
  } else {
    arr = done;                                   // 间隔模式 + 今天没打卡 → 无排程
  }

  arr.sort(function (a, b) { return a.time - b.time; });
  arr.forEach(function (d, i) { d.idx = i; d.total = arr.length; });
  S.doses[todayKey()] = keep.concat(arr);
  return arr.length;
}

/* 按 id 在全部日期里找剂量 —— 照片可能属于历史记录，不只今天 */
export function findDoseById(id) {
  var keys = Object.keys(S.doses || {});
  for (var i = 0; i < keys.length; i++) {
    var arr = S.doses[keys[i]] || [];
    for (var j = 0; j < arr.length; j++) if (arr[j].id === id) return arr[j];
  }
  return null;
}

export function sortedDoses() {
  return todayDoses().slice().sort(function (a, b) { return a.time - b.time; });
}

/* ---------------- 延后（snooze） ----------------
 * 设计要点：**snooze 绝不写 d.time**。
 * d.time 是「计划服药时刻」——排程表的显示依据，也是顺延（rollForward）调整的对象。
 * 旧实现是 `ds.time = Math.min(1439, ds.time + 10)`，两个问题：
 *   ① 语义错位：snooze 是「下次响铃晚一点」，不该改计划时刻；
 *      改一次排程表上的显示时刻就被永久改掉（08:00 → 08:10），原始计划时间丢失，
 *      snooze 三次就偏 30 分钟。这是数据正确性问题，不是边界问题。
 *   ② 越界钳制：23:55 延后 10 分钟 → 1445 被钳成 1439（23:59），
 *      用户以为延后 10 分钟、实际只延后 4 分钟。
 * 现在改为独立字段 d.snoozeUntil 记录「下次响铃时刻」，d.time 始终是原计划时刻。 */
export var SNOOZE_MIN = 10;

/* 剂量的下次响铃时刻（分钟数）。没有延后过就是原计划时刻。 */
export function dueAt(d) {
  if (!d) return 0;
  return (d.snoozeUntil != null) ? d.snoozeUntil : d.time;
}

/* 按「下次响铃时刻」排序 —— 倒计时和"下一条该吃的"要用它，
 * 而排程表仍按计划时刻排序（sortedDoses），两者语义不同，不能混用。 */
export function sortedByDue() {
  return todayDoses().slice().sort(function (a, b) { return dueAt(a) - dueAt(b); });
}

/* 延后一次。只改 snoozeUntil / snoozeCount，**不动 d.time**。
 * 跨零点不延后：排到次日会与「明天首次打卡才排程」的模型冲突（计划 §10 Q2），
 * 所以明确告知，而不是悄悄钳到 23:59。 */
export function snoozeDose(d) {
  if (!d || d.status !== 'pending') return { ok: false, reason: 'not-pending' };
  var base = dueAt(d);
  var next = base + SNOOZE_MIN;
  if (next >= 1440) {
    /* 不再延后。标记已通知，避免 tick 立刻又弹一次；剂量本身仍是 pending，
     * 过了宽限期会被识别成「已错过」并出现在漏服卡里，用户仍可补记。 */
    S.notified[d.id] = 1;
    return { ok: false, reason: 'past-midnight', base: base };
  }
  d.snoozeUntil = next;
  d.snoozeCount = (d.snoozeCount || 0) + 1;
  S.notified[d.id] = 0;
  return { ok: true, at: next, count: d.snoozeCount };
}

/* 延后结果的提示文案 —— 通知栏按钮与页面内弹窗共用同一套语义 */
export function snoozeToast(r) {
  if (r && r.ok) {
    return '已延后 ' + SNOOZE_MIN + ' 分钟，' + minToStr(r.at) + ' 再提醒你'
      + (r.count > 1 ? '（第 ' + r.count + ' 次）' : '');
  }
  if (r && r.reason === 'past-midnight') {
    return '已过零点，今天不再延后；这次记为未处理，明早可以补记';
  }
  return '这条已经处理过了';
}

export function nextPending() {
  /* 用「下次响铃时刻」排序：若 08:00 那次被延后到 08:20，而 08:10 那次正常，
   * 下一条该吃的应该是 08:10 那条 —— 按计划时刻排序会给出错误答案。 */
  var l = sortedByDue();
  for (var i = 0; i < l.length; i++) if (l[i].status === 'pending') return l[i];
  return null;
}

export function progress() {
  var l = todayDoses();
  var done = l.filter(function (d) { return d.status === 'taken' || d.status === 'skipped'; }).length;
  return { done: done, total: l.length };
}

/* ---------------- S-7：依从性报告（近 N 天，给医生 / 家属看） ----------------
 *
 * 目标**不是**「导出全部原始记录」（那是 CSV 的活儿），而是回答医生唯一关心的那个问题：
 *   **「近 7 天到底按时没按时」**。
 *
 * 判定规则**与记录页完全一致**（已服 / 已跳过 / 已错过 / 未到时间），不另立一套 ——
 * 否则报告数字与界面显示会对不上，而那比「算错」更难查。
 *
 * ⚠️ **过去日期的 pending 一律算漏服**：那些时刻早就过去了，不会再变成「未到时间」。
 *    `isMissed()` 内部拿 `nowMin()` 比较，**只对今天安全**，所以这里不能直接复用：
 *    反例 —— 昨天 23:00 的剂量，今天 00:10 看，`nowMin() - dueAt = 10 - 1380` 是负的，
 *    复用会把它误判成「未到时间」。
 *
 * 依从率 = 已服 / (已服 + 漏服)。**「已跳过」不计入分母** ——
 * 跳过是用户主动的（如医嘱停药），算进去会冤枉人；单列出来让医生自己判断。 */
export function adherenceReport(days) {
  /* 没传 → 默认 7 天；传了但非法（NaN / ≤0）→ 至少 1 天。
   * ⚠️ 别写成 `Number(days) || 7` —— `0` 是 falsy，传 0 会被当成「没传」变成 7 天。 */
  var n = (days === undefined || days === null) ? 7 : Math.floor(Number(days));
  if (!isFinite(n) || n < 1) n = 1;
  var to = todayKey();
  var rows = [];
  var sum = { taken: 0, missed: 0, skipped: 0, pending: 0, rate: null };

  for (var i = n - 1; i >= 0; i--) {
    var k = dayKeyShift(to, -i);
    var list = (S.doses && S.doses[k]) || [];
    var r = { key: k, taken: 0, missed: 0, skipped: 0, pending: 0, rate: null };
    for (var j = 0; j < list.length; j++) {
      var d = list[j];
      if (d.status === 'taken') r.taken++;
      else if (d.status === 'skipped') r.skipped++;
      else if (k !== to || nowMin() - dueAt(d) > MISS_GRACE_MIN) r.missed++;
      else r.pending++;
    }
    r.rate = rateOf(r.taken, r.missed);
    sum.taken += r.taken; sum.missed += r.missed;
    sum.skipped += r.skipped; sum.pending += r.pending;
    rows.push(r);
  }
  sum.rate = rateOf(sum.taken, sum.missed);
  return { days: n, from: rows.length ? rows[0].key : to, to: to, rows: rows, total: sum };
}

/* 依从率（整数百分比）。**分母为 0 时返回 `null`，不是 0** ——
 * 「这几天没有数据」和「一次都没吃」是两回事，显示成 0% 会冤枉人。 */
function rateOf(taken, missed) {
  var denom = taken + missed;
  return denom > 0 ? Math.round(taken * 100 / denom) : null;
}

/* 日期 key 平移。**用本地时间构造 Date**，不走字符串解析 ——
 * `new Date('2026-10-01')` 按 UTC 解析，东八区会整体差一天。 */
function dayKeyShift(key, delta) {
  var p = String(key).split('-');
  var d = new Date(Number(p[0]), Number(p[1]) - 1, Number(p[2]) + delta);
  return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate());
}
