/* S-2：库存管理（2026-10-01）
 *
 * 三个纯计算：`dailyDoseCount` / `stockInfo` / `deductStock`。
 * 共同原则是 **算不出来就不算** —— 猜出来的「还能吃 X 天」比不显示更糟，
 * 用户会照着一个错数字决定要不要买药。
 *
 * ⚠️ 查源码一律先剥离注释；抽真函数跑，不另写一份实现。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const UTIL_SRC = fs.readFileSync(path.join(ROOT, 'www/js/core/util.js'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');

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

/* 用**真实源码**搭 sandbox：datetime 工具取自 util.js，业务函数取自拼接后的模块图 */
function makeEnv() {
  const sb = {
    console: { warn() {}, log() {}, error() {} },
    Math, JSON, Object, Array, String, Number, Boolean, isFinite, parseInt, parseFloat, Date
  };
  sb.window = sb;
  vm.createContext(sb);
  /* ⚠️ pad 在 util.js 里是 `export var pad = function (n) {...}`（不是 function 声明），
   *    所以抽取头要写 `var pad = function`。 */
  vm.runInContext(cut(UTIL_SRC, 'var pad = function (n)'), sb);
  vm.runInContext(cut(UTIL_SRC, 'function fmtDate(d)'), sb);
  vm.runInContext(cut(APP_SRC, 'function normTimes(arr)'), sb);
  vm.runInContext(cut(APP_SRC, 'function medMode(m)'), sb);
  vm.runInContext(cut(APP_SRC, 'function dailyDoseCount(m)'), sb);
  vm.runInContext(cut(APP_SRC, 'function stockInfo(m)'), sb);
  vm.runInContext(cut(APP_SRC, 'function deductStock(m)'), sb);
  /* `export var` 常量不是函数、cut 抽不到 —— 用正则**从源码取值**（不写死，
   * 否则源码改了测试还会"绿"）。 */
  sb.MAX_TIMES = constOf(APP_SRC, 'MAX_TIMES');
  sb.LOW_STOCK_DAYS = constOf(APP_SRC, 'LOW_STOCK_DAYS');
  return sb;
}

/* 取 `[export] var NAME = <expr>;` 的值。
 * ⚠️ `sources.all()` 已经剥掉了 `export` 前缀，所以这里 export 是可选的。 */
function constOf(src, name) {
  const m = new RegExp('(?:export\\s+)?var\\s+' + name + '\\s*=\\s*([^;]+);').exec(src);
  if (!m) throw new Error('抽不到常量: ' + name);
  return vm.runInNewContext(m[1]);
}

const sb = makeEnv();

console.log('');
console.log('=== A. dailyDoseCount：每天吃几次 ===');
{
  t('fixed：按时刻表长度', sb.dailyDoseCount({ mode: 'fixed', times: [480, 1200] }) === 2);
  t('interval 8h → 3 次', sb.dailyDoseCount({ interval: 8 }) === 3);
  t('interval 12h → 2 次', sb.dailyDoseCount({ interval: 12 }) === 2);
  t('interval 0.5h → 48 次（F-1 的半小时粒度）', sb.dailyDoseCount({ interval: 0.5 }) === 48);
  t('★ 缺 interval → 0（宁可不显示，也不猜）', sb.dailyDoseCount({}) === 0);
  t('null → 0', sb.dailyDoseCount(null) === 0);
}

console.log('');
console.log('=== B. stockInfo：能算才算 ===');
{
  t('★ 没填剩余量 → null（旧数据完全不显示库存 UI）', sb.stockInfo({ name: 'X' }) === null);
  t('stock 是非法值 → null', sb.stockInfo({ stock: 'abc' }) === null);

  const full = sb.stockInfo({ stock: 28, dose: '1', unit: '片', mode: 'fixed', times: [480, 1200] });
  t('★ 三样齐全 → 算出天数（28 片 / 每天 2 片 = 14 天）',
    !!full && full.days === 14 && full.perDay === 2 && full.stock === 28);

  const noDose = sb.stockInfo({ stock: 28, mode: 'fixed', times: [480, 1200] });
  t('★ 缺剂量 → 只报剩余量、days 为 null（不硬算）',
    !!noDose && noDose.days === null && noDose.stock === 28 && noDose.low === false);

  const low = sb.stockInfo({ stock: 3, dose: '1', mode: 'fixed', times: [480, 1200] });
  t('★ 不足 4 天 → low = true（低库存提醒）', !!low && low.days === 1 && low.low === true);

  const enough = sb.stockInfo({ stock: 30, dose: '1', mode: 'fixed', times: [480, 1200] });
  t('充足 → 不报警', !!enough && enough.low === false);

  const zero = sb.stockInfo({ stock: 0, dose: '1', mode: 'fixed', times: [480, 1200] });
  t('★ stock = 0 → 不报警（已经没了，不该反复提醒）',
    !!zero && zero.stock === 0 && zero.days === 0 && zero.low === false);

  const half = sb.stockInfo({ stock: 0.5, dose: '0.5', unit: '片', mode: 'fixed', times: [480, 1200] });
  t('半片剂量也能算（7 天）', !!half && half.days === 0 && half.low === true);

  t('算出预计用完日（YYYY-MM-DD）',
    !!full && /^\d{4}-\d{2}-\d{2}$/.test(full.runOut), '格式不对: ' + (full && full.runOut));
}

console.log('');
console.log('=== C. deductStock：打卡扣一次 ===');
{
  const m1 = { stock: 10, dose: '1' };
  sb.deductStock(m1);
  t('扣一次', m1.stock === 9);

  const m2 = { stock: 0.5, dose: '1' };
  sb.deductStock(m2);
  t('★ 扣到 0 就停 —— 不出现负数（否则会显示「-3 天」）', m2.stock === 0);

  const m3 = { stock: 10, dose: '0.5' };
  sb.deductStock(m3); sb.deductStock(m3);
  t('★ 半片连扣两次无浮点误差', m3.stock === 9);

  const m4 = { stock: 10 };
  sb.deductStock(m4);
  t('没填剂量 → 不扣（算不清就别乱扣）', m4.stock === 10);

  const m5 = { dose: '1' };
  sb.deductStock(m5);
  t('没填剩余量 → 不扣、也不凭空造出字段', !('stock' in m5));

  const m6 = { stock: 0, dose: '1' };
  sb.deductStock(m6);
  t('已经是 0 → 保持 0', m6.stock === 0);
}

console.log('');
console.log('=== D. 源码级接线（防改回去）===');
{
  t('index.html 有剩余量输入框', HTML_SRC.indexOf('id="medStock"') >= 0, '缺');
  t('★ 打卡（markTaken）里会扣库存', /if \(m\) deductStock\(m\)/.test(APP), '没接');
  t('★ 药品卡片显示库存行', /stockHtml\(m\)/.test(APP), '没显示');
  t('★ 编辑时预填剩余量', /medStock'\)\.value = \(med && med\.stock != null\)/.test(APP), '没预填');
  t('★ 剩余量填了非数字会被拦下', /stockRaw !== '' && !/.test(APP), '没校验');
  t('剩余量存的是数字而不是字符串', /'stock', stockRaw === '' \? '' : Number\(stockRaw\)/.test(APP), '类型不对');
}

/* ⚠️ 汇总行必须是 `通过 N / 共 M` —— harness 按这个格式解析 */
console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fails.length) {
  console.log('失败项：');
  fails.forEach(f => console.log('  - ' + f));
  process.exit(1);
}
