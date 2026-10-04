/* S-4 疗程管理（起止日期 + 到期自动停用）—— 2026-10-04
 *
 * 测三件事：
 *   ① `normSched` 的 from / to 归一化 —— **非法一律 null**，且 null 不能变成 0
 *      （`Number(null) === 0` 这个坑本项目栽过两次）。
 *   ② `isScheduledDay` 的**范围过滤是最外层** —— 含两端、且与 weekly/cycle 正确叠加。
 *   ③ `courseState` 四态与 `courseLabel` —— UI 靠它决定说什么话。
 *
 * ⚠️ 反直觉但**刻意**的两条（都在断言里钉住）：
 *   · 非法 / 缺省日期 → **不拦截**（失败方向必须是"多提醒"，不能是"药吃了没提醒"）
 *   · from > to（用户填反）→ isScheduledDay 恒 false；所以**保存时必须校验**，
 *     否则界面上只会显示"疗程已结束"，看不出是填错了。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const SHEET_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/sheet.js'), 'utf8');
const CARDS_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/cards.js'), 'utf8');
const TODAY_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/today.js'), 'utf8');
const SCHED_SRC = fs.readFileSync(path.join(ROOT, 'www/js/core/schedule.js'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');

const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = stripJs(APP_SRC);
const SHEET = stripJs(SHEET_SRC);
const CARDS = stripJs(CARDS_SRC);
const TODAY = stripJs(TODAY_SRC);

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

const TODAY_KEY = '2026-10-05';

function env() {
  const sb = {
    console: { warn() {}, log() {}, error() {} },
    Math, JSON, Object, Array, String, Number, Boolean, Date, isFinite, isNaN,
    parseInt, parseFloat,
    S: { meds: [] },
    todayKey: () => TODAY_KEY
  };
  vm.createContext(sb);
  vm.runInContext(cut(APP, 'function dowOf(key)'), sb);
  vm.runInContext(cut(APP, 'function dayDiff(a, b)'), sb);
  vm.runInContext(cut(APP, 'function normTimes(arr)'), sb);
  vm.runInContext(cut(APP, 'function medMode(m)'), sb);
  vm.runInContext((APP.match(/var MAX_TIMES\s*=\s*[^;]+;/) || ['var MAX_TIMES=12;'])[0], sb);
  vm.runInContext((APP.match(/var WEEK_LABELS\s*=\s*\[[^\]]*\];/) || ['var WEEK_LABELS=[];'])[0], sb);
  /* ⚠️ `asDay` **故意内联在 normSched 内部**（见源码注释）—— 模块级私有小函数会在
   * spec 的 cut() 沙箱里变成隐式依赖，一改就崩，已经踩过四次。所以这里不抽它。 */
  vm.runInContext(cut(APP, 'function normSched(m)'), sb);
  vm.runInContext(cut(APP, 'function isScheduledDay(m, dateKey)'), sb);
  vm.runInContext(cut(APP, 'function scheduledToday(m)'), sb);
  vm.runInContext(cut(APP, 'function courseState(m, dateKey)'), sb);
  vm.runInContext(cut(APP, 'function courseLabel(m)'), sb);
  return sb;
}

console.log('=== 0. 自检 ===');
{
  const sb = env();
  t('抽到了 normSched / isScheduledDay / courseState / courseLabel',
    typeof sb.normSched === 'function' && typeof sb.isScheduledDay === 'function'
    && typeof sb.courseState === 'function' && typeof sb.courseLabel === 'function', '改名了？');
}

console.log('');
console.log('=== 1. normSched：from / to 归一化 ===');
{
  const sb = env();
  const N = m => sb.normSched(m);
  eq('缺省 → 双 null（长期）', [N({}).from, N({}).to], [null, null]);
  eq('合法值保留', [N({ sched: { from: '2026-10-01', to: '2026-10-14' } }).from,
    N({ sched: { from: '2026-10-01', to: '2026-10-14' } }).to], ['2026-10-01', '2026-10-14']);
  eq('只设 from', [
    N({ sched: { from: '2026-10-01' } }).from, N({ sched: { from: '2026-10-01' } }).to], ['2026-10-01', null]);
  eq('⛔ 非法格式（斜杠）→ null', N({ sched: { from: '2026/10/01' } }).from, null);
  eq('⛔ 非法格式（缺位）→ null', N({ sched: { to: '2026-1-5' } }).to, null);
  eq('⛔ 空串 → null', N({ sched: { from: '' } }).from, null);
  /* ★ 这个坑栽过两次：Number(null) === 0 */
  eq('★★ null → null（不能变成 0，也不能变成 "null"）', N({ sched: { from: null, to: null } }).from, null);
  eq('★★ undefined → null', N({ sched: { from: undefined } }).from, null);
  eq('⛔ 数字 0 → null', N({ sched: { from: 0 } }).from, null);
  /* asDay 已**内联进 normSched**（见源码注释：模块级私有小函数会在 spec 沙箱里
     变成隐式依赖，踩过四次），所以这些白盒断言改成黑盒验证 —— 效果一样。 */
  eq('⛔ 带时间的字符串也拒（只认纯日期键）', N({ sched: { from: '2026-10-05T00:00' } }).from, null);
  eq('⛔ 数字型 20261005 拒（不做隐式转换）', N({ sched: { to: 20261005 } }).to, null);
  eq('⛔ 布尔 true 拒', N({ sched: { from: true } }).from, null);
}

console.log('');
console.log('=== 2. isScheduledDay：范围是最外层过滤 ===');
{
  const sb = env();
  const S = sb.isScheduledDay;
  const wk = { sched: { mode: 'weekly', weekdays: [0] } };   // 只在周一
  /* 2026-10-05 是周一；10-06 周二；10-12 下周一 */
  t('范围中（含两端）→ 该吃', S({ sched: { from: '2026-10-01', to: '2026-10-14' } }, '2026-10-05') === true, '中');
  t('★ from **当天**算之内（含左端）', S({ sched: { from: '2026-10-05', to: '2026-10-14' } }, '2026-10-05') === true, '含左');
  t('★ to **当天**算之内（含右端）', S({ sched: { from: '2026-10-01', to: '2026-10-05' } }, '2026-10-05') === true, '含右');
  t('开始日前一天 → 不吃', S({ sched: { from: '2026-10-06', to: '2026-10-14' } }, '2026-10-05') === false, '前');
  t('结束日后一天 → 不吃', S({ sched: { from: '2026-10-01', to: '2026-10-04' } }, '2026-10-05') === false, '后');
  t('只设 from：之后一直吃', S({ sched: { from: '2026-10-01' } }, '2030-01-01') === true, '长期');
  t('只设 to：之前一直吃', S({ sched: { to: '2026-10-14' } }, '2020-01-01') === true, '长期2');
  t('只设 to：之后不吃', S({ sched: { to: '2026-10-04' } }, '2026-10-05') === false, '过期');

  /* ★ 与周期叠加：先看范围，再看星期 */
  t('★ weekly 周一 + 范围内 → 吃', S({ sched: { mode: 'weekly', weekdays: [0], from: '2026-10-01', to: '2026-10-14' } }, '2026-10-05') === true, '叠加1');
  t('★ weekly 周一 + 范围外 → 不吃（范围优先）', S({ sched: { mode: 'weekly', weekdays: [0], to: '2026-10-04' } }, '2026-10-05') === false, '叠加2');
  t('★ weekly 周二 + 范围内 → 仍不吃（星期说了算）', S({ sched: { mode: 'weekly', weekdays: [0], from: '2026-10-01' } }, '2026-10-06') === false, '叠加3');

  /* ★ 但 weekly 的相位不能被疗程起止改变 —— 10-12 还是周一 */
  t('★ 疗程不改变周期相位（下周一仍然吃）',
    S({ sched: { mode: 'weekly', weekdays: [0], from: '2026-10-01', to: '2026-10-14' } }, '2026-10-12') === true, '相位');

  /* 非法日期 → 不拦截（安全失败方向：多提醒）
   * ⚠️ 这两个用例**只给一个非法值** —— 第一版我给 from 塞了非法值却同时留了个
   *    过去的 to，结果是被 to 拦下的，断言冤枉了实现。**先怀疑自己的用例。** */
  t('★★ 非法 from（格式坏）→ **不拦截**（宁可多提醒）',
    S({ sched: { from: 'garbage' } }, '2026-10-05') === true, '非法 from');
  t('★★ 非法 to → 不拦截', S({ sched: { to: 'garbage' } }, '2030-01-01') === true, '非法 to');

  /* 用户填反了：from > to → 永远不吃（所以保存必须校验） */
  t('⚠️ from > to → 恒 false（保存时必须拦住，否则界面只说"疗程已结束"）',
    S({ sched: { from: '2026-10-14', to: '2026-10-01' } }, '2026-10-05') === false, '填反');
}

console.log('');
console.log('=== 3. courseState：四态 ===');
{
  const sb = env();
  const C = m => sb.courseState(m);
  eq('没设范围 → none', C({}), 'none');
  eq('只有 from → active', C({ sched: { from: '2026-10-01' } }), 'active');
  eq('只有 to（未过）→ active', C({ sched: { to: '2026-10-14' } }), 'active');
  eq('在范围内 → active', C({ sched: { from: '2026-10-01', to: '2026-10-14' } }), 'active');
  eq('开始日当天 → active', C({ sched: { from: '2026-10-05', to: '2026-10-14' } }), 'active');
  eq('结束日当天 → active（含右端）', C({ sched: { from: '2026-10-01', to: '2026-10-05' } }), 'active');
  eq('开始日前 → before', C({ sched: { from: '2026-10-06', to: '2026-10-14' } }), 'before');
  eq('结束日后 → ended', C({ sched: { from: '2026-10-01', to: '2026-10-04' } }), 'ended');
  eq('只设 to 且已过 → ended', C({ sched: { to: '2026-10-04' } }), 'ended');
  eq('可指定日期（纯函数）', sb.courseState({ sched: { from: '2026-10-01', to: '2026-10-14' } }, '2026-11-01'), 'ended');
  eq('非法 from 不影响状态判定（stays active）', C({ sched: { from: 'garbage', to: '2026-10-14' } }), 'active');
}

console.log('');
console.log('=== 4. courseLabel：中文描述 ===');
{
  const sb = env();
  const L = m => sb.courseLabel(m);
  eq('没设 → 空串', L({}), '');
  eq('起止都有 → "A 至 B"', L({ sched: { from: '2026-10-01', to: '2026-10-14' } }), '2026-10-01 至 2026-10-14');
  eq('只有 from → "A 开始"', L({ sched: { from: '2026-10-01' } }), '2026-10-01 开始');
  eq('只有 to → "至 B"', L({ sched: { to: '2026-10-14' } }), '至 2026-10-14');
}

console.log('');
console.log('=== 5. 接线：UI ===');
t('★ sheet.js 保存时校验 from <= to',
  /cfrom && cto && cfrom > cto/.test(SHEET), '没校验');
t('★ sheet.js 的 schedFromDraft 会带上 from/to',
  /if \(from\) out\.from = from;/.test(SHEET) && /if \(to\) out\.to = to;/.test(SHEET), '没带');
t('★★ 只有「每天 + 无疗程」才返回 null（只设疗程时不能丢）',
  /out\.mode === 'daily' && !out\.from && !out\.to\) return null/.test(SHEET), '条件不对');
t('sheet.js 预填了起止日期', /#courseFrom'\)\.value = sc0\.from/.test(SHEET), '没预填');
t('sheet.js 有即时反馈 renderCourseHint', /function renderCourseHint/.test(SHEET), '没有');
t('sheet.js 绑定了日期的 change 事件',
  /'#courseFrom', '#courseTo'\]\.forEach/.test(SHEET), '没绑定');
t('cards.js 卡片会明说「疗程已结束，不再提醒」',
  /疗程已结束，不再提醒/.test(CARDS), '没接');
t('cards.js 会明说「还没到开始日」', /还没到开始日/.test(CARDS), '没接');
t('★ cards.js 里疗程判断**在**周期判断之前（信息量大者优先）',
  CARDS.indexOf('courseState(m)') < CARDS.indexOf('scheduledToday(m)'), '顺序反了');
t('today.js 空态区分「所有疗程都结束」', /allEnded/.test(TODAY), '没区分');
t('index.html 有起止日期输入', /id="courseFrom"/.test(HTML_SRC) && /id="courseTo"/.test(HTML_SRC), '没有');
t('★ 日期输入用 date-wrap（min-height，避免被固定高度裁掉）',
  /date-wrap/.test(HTML_SRC), '没加防线');

console.log('');
console.log('=== 6. 接线：核心 ===');
t('schedule.js export 了 courseState / courseLabel',
  /export function courseState/.test(SCHED_SRC) && /export function courseLabel/.test(SCHED_SRC), '缺导出');
t('★ isScheduledDay 里范围判断在 mode 判断**之前**',
  SCHED_SRC.indexOf("if (s.from && dayDiff(s.from, dateKey) < 0)") < SCHED_SRC.indexOf("if (s.mode === 'daily') return true;"),
  '顺序反了');
t('normSched 用 asDay 校验（不用 Number）',
  /out\.from = asDay\(s\.from\)/.test(SCHED_SRC), '用了别的方式');

console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('  失败清单：');
  fails.forEach(f => console.log('    - ' + f));
}
process.exit(fail ? 1 : 0);
