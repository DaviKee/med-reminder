/* 固定时刻模式不该被"顺延"（2026-09-28 真机反馈）
 *
 * 用户报告：「我昨天 12:04 晚上点了一个固定时间服药，我今天早上顺延了。不是按照固定时间。」
 *
 * 根因是**两个缺陷叠加**：
 *   ① App 在后台时 JS 定时器被系统暂停 → 跨天检测（30 秒轮询）不跑；
 *      早上切回前台时 appStateChange 只调了 refreshPerm()，**没补跨天处理**
 *      → 今天一条剂量都没生成。
 *   ② 于是点「打卡」时 todayDoses().length === 0（保护放行）→ checkInAll()
 *      → checkIn 用**间隔逻辑**（从 nowMin() 起按 med.interval 排）给固定时刻的药也排了一遍
 *      → 用户看到「顺延了、不按固定时间」。
 *
 * 两条都要测：① 结构断言（跨天处理必须在「回到前台」时也跑）；
 *            ② 行为断言（checkIn 对 fixed 药必须按时刻表走，绝不按间隔排）。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = stripJs(APP_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}

function cut(src, header) {
  const i = src.indexOf(header);
  if (i < 0) throw new Error('抽不到: ' + header);
  const b = src.indexOf('{', i);
  let d = 0, j = b;
  while (j < src.length) {
    if (src[j] === '{') d++;
    else if (src[j] === '}') { d--; if (d === 0) break; }
    j++;
  }
  return src.slice(i, j + 1);
}

/* ═══════════ ① 结构断言 ═══════════ */
console.log('\n=== 结构：跨天处理必须也在「回到前台」时跑 ===');

t('★ 抽出了 rollDayIfNeeded（不再只挂在 30 秒轮询上）',
  APP.indexOf('function rollDayIfNeeded()') >= 0, '缺');

t('★ rollDayIfNeeded 里会 ensureFixedDoses（新的一天重排固定时刻）',
  /function rollDayIfNeeded\(\)[\s\S]{0,700}?ensureFixedDoses\(\)/.test(APP), '缺');

t('★ 回到前台（isActive）时调用了 rollDayIfNeeded',
  /onAppStateChange\(function \(isActive\)[\s\S]{0,400}?rollDayIfNeeded\(\)/.test(APP), '缺');

t('30 秒轮询改共用 rollDayIfNeeded',
  /setInterval\(rollDayIfNeeded,\s*30000\)/.test(APP), '缺');

t('旧的「内联跨天块」已移除（不再有两份重复实现）',
  !/setInterval\(\s*function\s*\(\)\s*\{\s*if\s*\(todayKey\(\)\s*!==\s*lastDay\)/.test(APP), '仍在');

console.log('\n=== 结构：checkIn 必须挡掉固定时刻模式 ===');

t('★ checkIn 里有 medMode(...) === \'fixed\' 的判断',
  /function checkIn\(medId\)[\s\S]{0,1000}?medMode\(med\)\s*===\s*'fixed'/.test(APP), '缺');

t('★ checkIn 对 fixed 会补调 ensureFixedDoses（兜底生成，不只是"什么都不做"）',
  /medMode\(med\)\s*===\s*'fixed'\)\s*\{\s*ensureFixedDoses\(\)/.test(APP), '缺');

/* ═══════════ ② 行为断言 ═══════════ */
console.log('\n=== 行为：checkIn 对两种模式的实际效果 ===');

const BLOCKS = [
  cut(APP_SRC, 'function medMode(m)'),
  cut(APP_SRC, 'function normTimes(arr)'),
  cut(APP_SRC, 'function medById(id)'),
  cut(APP_SRC, 'function todayDoses()'),
  cut(APP_SRC, 'function reindexMed(medId)'),
  cut(APP_SRC, 'function ensureFixedDoses()'),
  cut(APP_SRC, 'function checkIn(medId)'),
  /* ⚠️ S-3（2026-10-04）：ensureFixedDoses / rebuildTodayDoses 现在会调 `scheduledToday()`
   * 判断「今天该不该吃」—— 依赖链是 normSched → isScheduledDay → dowOf / dayDiff。
   * 不抽进来，整个 spec 会 ReferenceError 直接崩（表现与 S-2 那次一样）。
   * ⚠️ **手工清单抽函数的代价**：被测函数的依赖一变，就得同步这份清单 ——
   *    否则崩的不是被测代码，而是套件自己。 */
  cut(APP_SRC, 'function dowOf(key)'),
  cut(APP_SRC, 'function dayDiff(a, b)'),
  cut(APP_SRC, 'function normSched(m)'),
  cut(APP_SRC, 'function isScheduledDay(m, dateKey)'),
  cut(APP_SRC, 'function scheduledToday(m)')
].join('\n\n');

const TODAY = '2026-09-28';

function runCheckIn(med, nowMinutes) {
  const S = { meds: [med], doses: {}, notified: {} };
  let seq = 0;
  const sandbox = {
    console: { warn() {}, log() {}, error() {} },
    Math: Math, JSON: JSON, Object: Object, Array: Array,
    String: String, Number: Number, Date: Date, isNaN: isNaN,
    MAX_TIMES: 10,                 // normTimes 用到（app.js 的模块级常量）
    S: S,
    save() {},
    uid() { return 'uid' + (++seq); },
    nowMin() { return nowMinutes; },
    todayKey() { return TODAY; }
  };
  vm.createContext(sandbox);
  vm.runInContext(BLOCKS, sandbox);
  const ret = vm.runInContext('checkIn("m1")', sandbox);
  return { ret: ret, doses: S.doses[TODAY] || [] };
}

/* 固定时刻的药：时刻表 08:00 / 20:00；interval 故意设成 8 小时（480 分）。
 * 修复前：会按「09:00 起每 480 分」排出 540, 1020 —— 正是用户看到的"顺延"。 */
const rFixed = runCheckIn({ id: 'm1', name: '降压药', mode: 'fixed', times: [480, 1200], interval: 8 }, 540);
const fixedTimes = rFixed.doses.map(d => d.time).join(',');

t('★ 固定时刻的药：checkIn 返回 null（不把它当"首次打卡"处理）',
  rFixed.ret === null, '传了 ' + JSON.stringify(rFixed.ret));

t('★ 固定时刻的药：剂量严格按时刻表（480,1200），不是「从此刻起按间隔」（540,1020）',
  fixedTimes === '480,1200', '实际得到 ' + fixedTimes);

t('★ 固定时刻的药：保证"今天一定有时刻剂量"（兜底生成也是对的）',
  rFixed.doses.length === 2 && rFixed.doses.every(d => d.status === 'pending'),
  JSON.stringify(rFixed.doses.map(d => d.status)));

/* 间隔模式的药：对照 —— 不能因为修 fixed 而把 interval 也挡掉 */
const rInt = runCheckIn({ id: 'm1', name: '维生素', interval: 8 }, 540);
const intTimes = (rInt.ret || []).map(d => d.time).join(',');

t('对照：间隔模式的药仍按原语义生成（09:00 起每 8 小时）',
  rInt.ret !== null && intTimes.indexOf('540') === 0, '实际 ' + intTimes);

t('对照：间隔模式第一条直接算「已服用」（原有语义没被改动）',
  !!(rInt.ret && rInt.ret[0] && rInt.ret[0].status === 'taken'), '不是 taken');

/* ⚠️ 汇总行必须匹配 run-all.js 的正则 `通过 N / 共 M`，否则运行器会判成"套件出错"。 */
console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('失败清单：');
  fails.forEach(f => console.log('  - ' + f));
}
process.exit(fail ? 1 : 0);
