/* S-3 服用周期（每周指定星期 / 每隔 N 天）—— 2026-10-04
 *
 * 测四层：
 *   ① `dowOf` / `dayDiff` —— 日期工具。**这两个最容易错**：`dayDiff` 若用
 *      `new Date('YYYY-MM-DD')`（按 UTC 解析）在东八区会整体差 8 小时，
 *      跨月/跨年时算出**差一天**，而且只在部分日期发作。
 *   ② `normSched` —— 归一化。重点：**「选了每周但一天没选」必须保留为空数组**，
 *      不能静默退回"每天"（那等于每天都在催一个本该休息的人吃药）。
 *   ③ `isScheduledDay` —— 判定矩阵。重点：**cycle 缺 anchor 时判为「该吃」** ——
 *      宁可多提醒，也不能因为字段缺失让药无声无息地停掉。
 *   ④ 折算与接线：`dailyDoseCount` 要按周期打折（否则"隔天吃"的预计用完日砍半）。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const CARDS_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/cards.js'), 'utf8');
const TODAY_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/today.js'), 'utf8');
const SW_SRC = fs.readFileSync(path.join(ROOT, 'www/js/core/schedule.js'), 'utf8');

const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = stripJs(APP_SRC);
const CARDS = stripJs(CARDS_SRC);
const TODAY = stripJs(TODAY_SRC);
const SCHED = stripJs(SW_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}
const eq = (name, a, b) => t(name, JSON.stringify(a) === JSON.stringify(b),
  JSON.stringify(a) + ' !== ' + JSON.stringify(b));

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

const TODAY_KEY = '2026-10-05';               // 周一（0 = 周一）

/* 沙箱：真源码 + 少量桩。
 * `todayKey` 必须是桩 —— 要固定"今天"，否则测试会随运行日期飘。 */
function env(meds) {
  const sb = {
    console: { warn() {}, log() {}, error() {} },
    Math, JSON, Object, Array, String, Number, Boolean, Date, isFinite, isNaN,
    parseInt, parseFloat,
    S: { meds: meds || [] },
    todayKey: () => TODAY_KEY
  };
  vm.createContext(sb);
  vm.runInContext(cut(APP, 'function dowOf(key)'), sb);
  vm.runInContext(cut(APP, 'function dayDiff(a, b)'), sb);
  vm.runInContext(cut(APP, 'function normTimes(arr)'), sb);
  /* 常量从源码抠原文（**不写死**，否则源码改了 spec 还会绿） */
  vm.runInContext((APP.match(/var MAX_TIMES\s*=\s*[^;]+;/) || ['var MAX_TIMES=12;'])[0], sb);
  vm.runInContext(cut(APP, 'function medMode(m)'), sb);
  vm.runInContext((APP.match(/var WEEK_LABELS\s*=\s*\[[^\]]*\];/) || ['var WEEK_LABELS=[];'])[0], sb);
  vm.runInContext(cut(APP, 'function normSched(m)'), sb);
  vm.runInContext(cut(APP, 'function isScheduledDay(m, dateKey)'), sb);
  vm.runInContext(cut(APP, 'function scheduledToday(m)'), sb);
  vm.runInContext(cut(APP, 'function anyScheduledToday()'), sb);
  vm.runInContext(cut(APP, 'function schedLabel(m)'), sb);
  vm.runInContext(cut(APP, 'function dailyDoseCount(m)'), sb);
  return sb;
}

console.log('=== 0. 自检：真源码到位 ===');
t('抽到了 dowOf / dayDiff / normSched / isScheduledDay',
  typeof env().dowOf === 'function' && typeof env().dayDiff === 'function'
  && typeof env().normSched === 'function' && typeof env().isScheduledDay === 'function',
  '函数名改名了？');

console.log('');
console.log('=== 1. dowOf：0 = 周一（不是周日）===');
{
  const sb = env();
  eq('2026-10-05（周一）→ 0', sb.dowOf('2026-10-05'), 0);
  eq('2026-10-06（周二）→ 1', sb.dowOf('2026-10-06'), 1);
  eq('2026-10-11（周日）→ 6', sb.dowOf('2026-10-11'), 6);
  eq('2026-10-12（下周一）→ 0', sb.dowOf('2026-10-12'), 0);
  eq('非法输入 → -1（调用方据此判不匹配）', sb.dowOf('not-a-date'), -1);
  eq('空串 → -1', sb.dowOf(''), -1);
  /* 跨年也要对：2027-01-01 是周五 */
  eq('2027-01-01（周五）→ 4', sb.dowOf('2027-01-01'), 4);
}

console.log('');
console.log('=== 2. dayDiff：跨月/跨年/负值 ===');
{
  const sb = env();
  eq('同一天 → 0', sb.dayDiff('2026-10-05', '2026-10-05'), 0);
  eq('+1 天', sb.dayDiff('2026-10-05', '2026-10-06'), 1);
  eq('倒过来 → -1', sb.dayDiff('2026-10-06', '2026-10-05'), -1);
  eq('跨月：10-31 → 11-01 = 1', sb.dayDiff('2026-10-31', '2026-11-01'), 1);
  eq('跨年：12-31 → 次年 01-01 = 1', sb.dayDiff('2026-12-31', '2027-01-01'), 1);
  eq('整月：10-05 → 11-05 = 31', sb.dayDiff('2026-10-05', '2026-11-05'), 31);
  eq('整年 = 365（2026 非闰年）', sb.dayDiff('2026-01-01', '2027-01-01'), 365);
  eq('闰年 = 366（2028 是闰年）', sb.dayDiff('2028-01-01', '2029-01-01'), 366);
  /* ⚠️ 这条是**最容易踩的坑**：用 `new Date('YYYY-MM-DD')`（UTC）解析会差一天。
   * 夏令时/时区边界后仍要是整数 1。 */
  eq('★ 跨月首日仍是整 1 天（不是 0.96 天）', sb.dayDiff('2026-06-30', '2026-07-01'), 1);
  t('非法输入 → NaN', isNaN(sb.dayDiff('x', '2026-10-05')), String(sb.dayDiff('x', '2026-10-05')));
}

console.log('');
console.log('=== 3. normSched：归一化 ===');
{
  const sb = env();
  eq('无 sched（旧数据）→ daily', sb.normSched({}).mode, 'daily');
  eq('无 sched 的 med → daily', sb.normSched({ name: 'x', interval: 8 }).mode, 'daily');
  eq('null 安全 → daily', sb.normSched(null).mode, 'daily');
  eq('非法 mode → daily（不猜）', sb.normSched({ sched: { mode: 'weird' } }).mode, 'daily');

  eq('weekly：去重 + 升序', sb.normSched({ sched: { mode: 'weekly', weekdays: [4, 0, 2, 0] } }).weekdays, [0, 2, 4]);
  eq('weekly：越界值（-1 / 7 / 99）被丢弃',
    sb.normSched({ sched: { mode: 'weekly', weekdays: [-1, 7, 99, 3] } }).weekdays, [3]);
  eq('weekly：非数字被丢弃',
    sb.normSched({ sched: { mode: 'weekly', weekdays: ['a', null, 1] } }).weekdays, [1]);
  eq('weekly：小数被取整', sb.normSched({ sched: { mode: 'weekly', weekdays: [1.4, 2.6] } }).weekdays, [1, 3]);
  /* ★ 关键：选了每周但一天没选 = **一次都不吃**，不是"退回每天" */
  eq('★ weekly 空数组保留为空（不静默退回 daily）',
    sb.normSched({ sched: { mode: 'weekly', weekdays: [] } }).weekdays, []);
  eq('★ weekly 空数组的 mode 仍是 weekly',
    sb.normSched({ sched: { mode: 'weekly', weekdays: [] } }).mode, 'weekly');

  eq('cycle：everyDays=3 保留', sb.normSched({ sched: { mode: 'cycle', everyDays: 3, anchor: '2026-01-01' } }).everyDays, 3);
  eq('cycle：everyDays=1 被抬到 2（每 1 天就是每天，无意义）',
    sb.normSched({ sched: { mode: 'cycle', everyDays: 1 } }).everyDays, 2);
  eq('cycle：非数字 → 2', sb.normSched({ sched: { mode: 'cycle', everyDays: 'x' } }).everyDays, 2);
  eq('cycle：上限 90', sb.normSched({ sched: { mode: 'cycle', everyDays: 999 } }).everyDays, 90);
  eq('cycle：非法 anchor → null', sb.normSched({ sched: { mode: 'cycle', everyDays: 3, anchor: '2026/01/01' } }).anchor, null);
  eq('cycle：合法 anchor 保留',
    sb.normSched({ sched: { mode: 'cycle', everyDays: 3, anchor: '2026-01-01' } }).anchor, '2026-01-01');
}

console.log('');
console.log('=== 4. isScheduledDay：判定矩阵（纯函数）===');
{
  const sb = env();
  const daily = { interval: 8 };
  t('daily：任意日期都吃', sb.isScheduledDay(daily, '2026-10-05')
    && sb.isScheduledDay(daily, '2026-10-11') && sb.isScheduledDay(daily, '2030-01-01'), 'daily 判错了');

  /* weekly：[0,2,4] = 周一、周三、周五 */
  const wk = { sched: { mode: 'weekly', weekdays: [0, 2, 4] } };
  t('weekly 周一 → 吃', sb.isScheduledDay(wk, '2026-10-05') === true, '周一');
  t('weekly 周二 → 不吃', sb.isScheduledDay(wk, '2026-10-06') === false, '周二');
  t('weekly 周三 → 吃', sb.isScheduledDay(wk, '2026-10-07') === true, '周三');
  t('weekly 周六 → 不吃', sb.isScheduledDay(wk, '2026-10-10') === false, '周六');
  t('weekly 周日 → 不吃（0 是周一，容易错成周日）', sb.isScheduledDay(wk, '2026-10-11') === false, '周日');
  t('★ weekly 一天没选 → 全年都不吃', sb.isScheduledDay(
    { sched: { mode: 'weekly', weekdays: [] } }, '2026-10-05') === false, '空数组');
  t('weekly 跨周仍按星期判（下周一也吃）', sb.isScheduledDay(wk, '2026-10-12') === true, '下周一');

  /* cycle：从 2026-10-05 起每 2 天 */
  const cy = { sched: { mode: 'cycle', everyDays: 2, anchor: '2026-10-05' } };
  t('cycle 锚点当天 → 吃', sb.isScheduledDay(cy, '2026-10-05') === true, '+0');
  t('cycle +1 天 → 不吃', sb.isScheduledDay(cy, '2026-10-06') === false, '+1');
  t('cycle +2 天 → 吃', sb.isScheduledDay(cy, '2026-10-07') === true, '+2');
  t('cycle +3 天 → 不吃', sb.isScheduledDay(cy, '2026-10-08') === false, '+3');
  t('★ cycle 锚点**之前** → 不吃', sb.isScheduledDay(cy, '2026-10-04') === false, '锚点前');
  t('cycle 跨月仍连续（10-05 + 32 天 = 11-06）',
    sb.isScheduledDay({ sched: { mode: 'cycle', everyDays: 2, anchor: '2026-10-05' } }, '2026-11-06') === true,
    '跨月连续性');
  /* ★★ 最重要的一条：缺 anchor 不能"停药" */
  t('★★ cycle 缺 anchor → 判为**该吃**（宁可多提醒，不能静默停药）',
    sb.isScheduledDay({ sched: { mode: 'cycle', everyDays: 3 } }, '2026-10-05') === true, '缺 anchor');
  t('cycle 非法 anchor → 同样判为该吃',
    sb.isScheduledDay({ sched: { mode: 'cycle', everyDays: 3, anchor: 'garbage' } }, '2026-10-05') === true, '非法 anchor');

  /* 非法日期：不应抛，也不应静默变成"吃"以外的东西 */
  t('weekly + 非法 dateKey → 不炸（判 false）',
    sb.isScheduledDay(wk, 'garbage') === false, '非法日期');
}

console.log('');
console.log('=== 5. scheduledToday / anyScheduledToday ===');
{
  const sb = env([{ id: 'a', interval: 8 }, { id: 'b', sched: { mode: 'weekly', weekdays: [1] } }]);
  /* TODAY_KEY = 2026-10-05 是周一：a 每天吃，b 只在周二吃 */
  t('scheduledToday：daily 的是 true', sb.scheduledToday({ interval: 8 }) === true, 'daily');
  t('scheduledToday：周二限定的在周一 → false',
    sb.scheduledToday({ sched: { mode: 'weekly', weekdays: [1] } }) === false, 'weekly');
  t('anyScheduledToday：有任一该吃 → true', sb.anyScheduledToday() === true, 'any');
  const sb2 = env([{ id: 'b', sched: { mode: 'weekly', weekdays: [1] } }]);
  t('anyScheduledToday：全都今天不吃 → false', sb2.anyScheduledToday() === false, 'any=false');
  t('anyScheduledToday：没有药 → false', env([]).anyScheduledToday() === false, '空列表');
}

console.log('');
console.log('=== 6. schedLabel：中文描述 ===');
{
  const sb = env();
  eq('daily → 每天', sb.schedLabel({}), '每天');
  eq('weekly [0,2,4] → 每周一三五', sb.schedLabel({ sched: { mode: 'weekly', weekdays: [0, 2, 4] } }), '每周一三五');
  eq('weekly [6] → 每周日', sb.schedLabel({ sched: { mode: 'weekly', weekdays: [6] } }), '每周日');
  eq('★ weekly 空 → 未选星期（不是"每天"）',
    sb.schedLabel({ sched: { mode: 'weekly', weekdays: [] } }), '未选星期');
  eq('cycle 2 → 每 2 天', sb.schedLabel({ sched: { mode: 'cycle', everyDays: 2 } }), '每 2 天');
  eq('cycle 3 → 每 3 天', sb.schedLabel({ sched: { mode: 'cycle', everyDays: 3 } }), '每 3 天');
}

console.log('');
console.log('=== 7. ★ dailyDoseCount 按周期折算（库存要用）===');
{
  const sb = env();
  eq('daily + fixed 2 次 → 2', sb.dailyDoseCount({ mode: 'fixed', times: [480, 1200] }), 2);
  eq('daily + interval 8h → 3', sb.dailyDoseCount({ interval: 8 }), 3);
  /* weekly 3 天 / 7 × 每天 2 次 = 0.857 */
  t('★ weekly 3 天 → 2 × 3/7 ≈ 0.857',
    Math.abs(sb.dailyDoseCount({ mode: 'fixed', times: [480, 1200], sched: { mode: 'weekly', weekdays: [0, 2, 4] } }) - 6 / 7) < 1e-9,
    String(sb.dailyDoseCount({ mode: 'fixed', times: [480, 1200], sched: { mode: 'weekly', weekdays: [0, 2, 4] } })));
  /* cycle 2 天 × 每天 3 次 = 1.5 */
  eq('★ cycle 每 2 天 → 3 / 2 = 1.5', sb.dailyDoseCount({ interval: 8, sched: { mode: 'cycle', everyDays: 2 } }), 1.5);
  eq('★ cycle 每 3 天 → 3 / 3 = 1', sb.dailyDoseCount({ interval: 8, sched: { mode: 'cycle', everyDays: 3 } }), 1);
  eq('★ weekly 空 → 0（一天都不吃，不消耗）',
    sb.dailyDoseCount({ mode: 'fixed', times: [480, 1200], sched: { mode: 'weekly', weekdays: [] } }), 0);
}

console.log('');
console.log('=== 8. 接线：生成入口真的会判周期 ===');
t('★ ensureFixedDoses 里有 scheduledToday 判断', /if \(!scheduledToday\(m\)\) return;/.test(SCHED), '没接');
t('★ rebuildTodayDoses 里今天不该吃会清掉未服的',
  /if \(!scheduledToday\(m\)\)[\s\S]{0,200}S\.doses\[todayKey\(\)\] = keep\.concat\(done\)/.test(SCHED), '没清');
t('rebuildTodayDoses 保留已服的（那是事实）', /keep\.concat\(done\)/.test(SCHED), '把 done 也丢了');
t('schedule.js export 了 normSched / isScheduledDay / scheduledToday / anyScheduledToday / schedLabel',
  /export function normSched/.test(SW_SRC) && /export function isScheduledDay/.test(SW_SRC)
  && /export function scheduledToday/.test(SW_SRC) && /export function anyScheduledToday/.test(SW_SRC)
  && /export function schedLabel/.test(SW_SRC), '缺导出');
t('util.js export 了 dowOf / dayDiff',
  /export function dowOf/.test(fs.readFileSync(path.join(ROOT, 'www/js/core/util.js'), 'utf8'))
  && /export function dayDiff/.test(fs.readFileSync(path.join(ROOT, 'www/js/core/util.js'), 'utf8')), '缺导出');

console.log('');
console.log('=== 9. 接线：UI ===');
t('cards.js 的 medScheduleLabel 拼了周期前缀', /schedLabel\(m\)/.test(CARDS), '没接');
t('★ cards.js 的「未设时刻」不带周期前缀（与改造前一致）', /if \(!ts\.length\) return '未设时刻'/.test(CARDS), '退化');
t('cards.js 的 medMetaText 会明说「今天不用吃」', /今天不用吃/.test(CARDS), '没接');
t('today.js 有「今天不用吃药」空态', /今天不用吃药/.test(TODAY), '没接');
t('today.js 用了 anyScheduledToday 判断', /anyScheduledToday\(\)/.test(TODAY), '没接');
t('★ today.js 的空态在「今天还没打卡」**之前**（否则永远走不到）',
  TODAY.indexOf('今天不用吃药') < TODAY.indexOf('今天还没打卡'), '顺序反了');

console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('  失败清单：');
  fails.forEach(f => console.log('    - ' + f));
}
process.exit(fail ? 1 : 0);
