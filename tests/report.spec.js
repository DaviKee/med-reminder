/* S-7：依从性报告（2026-10-01）
 *
 * `adherenceReport(days)` 是纯函数，这里把边界全跑一遍。
 * 最要紧的两条：
 *   ① **过去日期的 pending 必须算漏服** —— 那些时刻早过了，不会再变回「未到时间」；
 *      现场判据 `isMissed()` 拿 `nowMin()` 比较，只对今天安全，**不能复用**。
 *   ② **「已跳过」不进依从率分母** —— 跳过是主动行为（医嘱停药），算进去会冤枉人。
 *
 * ⚠️ 查源码一律先剥离注释；抽真函数跑，不另写一份实现。
 * ⚠️ 把 `nowMin()` 固定成 600（10:00）—— 否则用例结果会随运行时刻漂移。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
/* ⚠️ `sources.all()` 会**剥掉 import / export**（为了能在 vm 里跑）。
 * 所以断言「有没有 export」「有没有 import 进来」必须读**原始文件**，
 * 拿 APP_SRC 查必然假红 —— 这个坑 2026-10-01 当场踩了一次。 */
const APP_RAW = fs.readFileSync(path.join(ROOT, 'www/js/app.js'), 'utf8');
const SCHEDULE_RAW = fs.readFileSync(path.join(ROOT, 'www/js/core/schedule.js'), 'utf8');
const UTIL_SRC = fs.readFileSync(path.join(ROOT, 'www/js/core/util.js'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const OVERLAY_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/overlay.js'), 'utf8');
const SW_SRC = fs.readFileSync(path.join(ROOT, 'www/sw.js'), 'utf8');

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

function constOf(src, name) {
  const m = new RegExp('var\\s+' + name + '\\s*=\\s*([^;]+);').exec(src);
  if (!m) throw new Error('抽不到常量: ' + name);
  return vm.runInNewContext(m[1]);
}

/* 用**真实源码**搭 sandbox：日期工具取自 util.js，排程与报告取自拼接后的模块图 */
function makeEnv() {
  const sb = {
    console: { warn() {}, log() {}, error() {} },
    Math, JSON, Object, Array, String, Number, Boolean, isFinite, parseInt, parseFloat, Date
  };
  vm.createContext(sb);
  vm.runInContext(cut(UTIL_SRC, 'var pad = function (n)'), sb);
  vm.runInContext(cut(UTIL_SRC, 'function fmtDate(d)'), sb);
  vm.runInContext(cut(UTIL_SRC, 'function todayKey()'), sb);
  vm.runInContext(cut(UTIL_SRC, 'function nowMin()'), sb);
  vm.runInContext(cut(APP_SRC, 'function dueAt(d)'), sb);
  vm.runInContext(cut(APP_SRC, 'function rateOf(taken, missed)'), sb);
  vm.runInContext(cut(APP_SRC, 'function dayKeyShift(key, delta)'), sb);
  vm.runInContext(cut(APP_SRC, 'function adherenceReport(days)'), sb);
  sb.MISS_GRACE_MIN = constOf(APP_SRC, 'MISS_GRACE_MIN');
  sb.S = { meds: [], doses: {} };
  sb.nowMin = function () { return 600; };   // 固定 10:00 —— 让判定可复现
  return sb;
}

const env = makeEnv();
const TODAY = env.todayKey();
const YDAY = env.dayKeyShift(TODAY, -1);

/* 造一条剂量 */
function dose(time, status, extra) {
  const d = { medId: 'm1', time: time, status: status };
  if (extra) Object.keys(extra).forEach(k => { d[k] = extra[k]; });
  return d;
}

console.log('');
console.log('=== A. 基本统计与空数据 ===');
{
  const sb = makeEnv();
  const r = sb.adherenceReport(7);

  t('空数据：行数 = 天数', r.rows.length === 7, '得到 ' + r.rows.length);
  t('空数据：总计全 0', r.total.taken === 0 && r.total.missed === 0 && r.total.skipped === 0, '不为 0');
  t('★ 空数据：依从率是 null 而不是 0（「没数据」≠「没吃」）', r.total.rate === null, '得到 ' + r.total.rate);
  t('from / to 正确', r.from === sb.dayKeyShift(TODAY, -6) && r.to === TODAY, r.from + '~' + r.to);
  t('最后一行是今天', r.rows[6].key === TODAY, r.rows[6].key);
  t('第一行是 6 天前', r.rows[0].key === sb.dayKeyShift(TODAY, -6), r.rows[0].key);
}

console.log('');
console.log('=== B. 依从率计算 ===');
{
  const sb = makeEnv();
  sb.S.doses[TODAY] = [dose(480, 'taken'), dose(540, 'taken'), dose(600, 'taken')];
  let r = sb.adherenceReport(1);
  t('全服 → 100%', r.total.rate === 100, '得到 ' + r.total.rate);

  sb.S.doses[TODAY] = [dose(480, 'taken'), dose(540, 'taken'), dose(420, 'pending')];
  r = sb.adherenceReport(1);   // 420 已过点(now=600，超 180 > 30 宽限) → 漏服
  t('2 服 1 漏 → 67%（四舍五入）', r.total.rate === 67, '得到 ' + r.total.rate);

  sb.S.doses[TODAY] = [dose(480, 'taken'), dose(420, 'pending')];
  r = sb.adherenceReport(1);
  t('1 服 1 漏 → 50%', r.total.rate === 50, '得到 ' + r.total.rate);

  sb.S.doses[TODAY] = [dose(420, 'pending')];
  r = sb.adherenceReport(1);
  t('全漏 → 0%', r.total.rate === 0, '得到 ' + r.total.rate);
}

console.log('');
console.log('=== C. ★ 过去日期的 pending 必须算漏服（不能复用 isMissed）===');
{
  const sb = makeEnv();
  sb.S.doses[YDAY] = [dose(480, 'taken'), dose(1380, 'pending')];   // 昨天 23:00 没打卡
  const r = sb.adherenceReport(7);

  t('★ 昨天 23:00 的 pending → 算漏服', r.total.missed === 1, '得到 ' + r.total.missed);
  t('昨天那条不会被算成「未到时间」', r.total.pending === 0, '得到 ' + r.total.pending);
  t('昨天依从率 50%', r.rows[5].rate === 50, '得到 ' + r.rows[5].rate);

  const sb2 = makeEnv();
  sb2.S.doses[YDAY] = [dose(480, 'taken'), dose(540, 'skipped')];
  const r2 = sb2.adherenceReport(7);
  t('过去的 skipped 仍是 skipped（不并入漏服）', r2.total.skipped === 1 && r2.total.missed === 0, '分类错');
}

console.log('');
console.log('=== D. 今天的 pending 要按时间判 ===');
{
  const sb = makeEnv();   // now = 600
  sb.S.doses[TODAY] = [
    dose(480, 'taken'),
    dose(540, 'pending'),    // 已过 60 分钟 > 30 宽限 → 漏服
    dose(601, 'pending'),    // 刚过 1 分钟，在宽限内 → 未到时间
    dose(720, 'pending')     // 未到点 → 未到时间
  ];
  const r = sb.adherenceReport(1);
  t('今天：刚过点但在宽限内 → 不算漏服', r.total.pending === 2, '得到 ' + r.total.pending);
  t('今天：超过宽限期才转漏服', r.total.missed === 1, '得到 ' + r.total.missed);
}

console.log('');
console.log('=== E. ★「已跳过」不进依从率分母 ===');
{
  const sb = makeEnv();
  sb.S.doses[TODAY] = [dose(480, 'taken'), dose(540, 'skipped')];
  const r = sb.adherenceReport(1);
  t('★ 1 服 + 1 跳过 → 依从率 100%（跳过不冤枉）', r.total.rate === 100, '得到 ' + r.total.rate);
  t('跳过的次数仍单列出来', r.total.skipped === 1, '得到 ' + r.total.skipped);

  const sb2 = makeEnv();
  sb2.S.doses[TODAY] = [dose(480, 'taken'), dose(540, 'skipped'), dose(420, 'pending')];
  const r2 = sb2.adherenceReport(1);
  t('分母只算 服+漏：1 服 1 漏 1 跳过 → 50%', r2.total.rate === 50, '得到 ' + r2.total.rate);
}

console.log('');
console.log('=== F. 统计范围与跨月 ===');
{
  const sb = makeEnv();
  const r30 = sb.adherenceReport(30);
  t('days=30 → 30 行', r30.rows.length === 30, '得到 ' + r30.rows.length);
  t('days 参数缺省 → 默认 7 天', sb.adherenceReport().days === 7, '缺省不对');
  t('days 非法值（0 / 负数）→ 至少 1 天，不崩', sb.adherenceReport(0).rows.length === 1
    && sb.adherenceReport(-5).rows.length === 1, '崩了或算错');

  /* 跨月：往前推 40 天必须落在上个月，且日期合法 */
  const back = sb.dayKeyShift(TODAY, -40);
  t('跨月日期合法（能对上 Date 解析）', /^\d{4}-\d{2}-\d{2}$/.test(back)
    && new Date(back + 'T12:00:00').getDate() === Number(back.slice(8)), back);
  t('日期严格递减（无重复无跳跃）',
    r30.rows.every((r, i) => i === 0 || r.key > r30.rows[i - 1].key), '顺序错');
}

console.log('');
console.log('=== G. 源码级接线（防改回去）===');
{
  t('schedule.js 导出 adherenceReport',
    /export function adherenceReport\(days\)/.test(SCHEDULE_RAW), '缺导出');
  t('★ 报告已接入 app.js（不是写完没人调）',
    /adherenceReport\(/.test(APP) && /import[\s\S]{0,900}adherenceReport/.test(APP_RAW), '没接上');
  t('index.html 有报告浮层', HTML_SRC.indexOf('id="dlgReport"') >= 0, '缺');
  t('★ 报告浮层已进 closableDialogs（否则返回键关不掉，且不报错）',
    /closableDialogs[\s\S]{0,400}dlgReport/.test(OVERLAY_SRC), '返回键会失效');
  t('★ 改了 html/css 必须升 SW 缓存版本',
    /var CACHE = 'medreminder-v\d+'/.test(SW_SRC), '缺缓存版本');
}

console.log('');
/* ⚠️ 汇总行必须是 `通过 N / 共 M` —— harness 按这个格式解析（见 harness/gates.py 第 11 行）。
 * 写成别的样子会被判为「未打印汇总行 → 套件自身出错」。 */
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fails.length) {
  console.log('失败项：');
  fails.forEach(f => console.log('  - ' + f));
  process.exit(1);
}
