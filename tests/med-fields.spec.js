/* S-1：剂量 / 单位 / 服用说明（2026-10-01）
 *
 * 三个字段**都可缺** —— 旧数据本来就没有它们，必须天然兼容：
 * 不能崩、不能把 `undefined` 显示到界面上。
 *
 * `doseLabel()` 是纯函数，这里把边界全跑一遍；
 * `setOrDel()` 保证「没填」只有一种表示法（属性不存在），
 * 否则导出 / 统计就得同时应付「空串」和「没有此字段」两种情况。
 *
 * ⚠️ 查源码一律先剥离注释（注释里会引用旧写法做对比，不剥离会假阳性）。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');

const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = stripJs(APP_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}

/* 从真实源码里抽一个函数块（与其它 spec 同一套做法） */
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

function loadFn(header) {
  const sb = {};
  vm.createContext(sb);
  vm.runInContext(cut(APP_SRC, header), sb);
  return sb;
}

console.log('');
console.log('=== A. doseLabel：边界与降级 ===');
{
  const sb = loadFn('function doseLabel(m)');
  const L = m => sb.doseLabel(m);

  t('无 med → 空串', L(null) === '' && L(undefined) === '');
  t('★ 无字段 → 空串（旧数据兼容）', L({}) === '' && L({ name: '阿司匹林' }) === '');
  t('仅剂量 → 「每次 1」', L({ dose: '1' }) === '每次 1');
  t('仅单位 → 「每次 片」', L({ unit: '片' }) === '每次 片');
  t('都有 → 「每次 1 片」', L({ dose: '1', unit: '片' }) === '每次 1 片');
  t('半片 → 「每次 0.5 片」', L({ dose: '0.5', unit: '片' }) === '每次 0.5 片');
  t('数字类型的 dose 也能显示（不是只认字符串）', L({ dose: 1, unit: '片' }) === '每次 1 片');
  t('空白字符视为空（不会拼出「每次  」）', L({ dose: '  ', unit: '' }) === '');
}

console.log('');
console.log('=== B. setOrDel：「没填」只有一种表示法 ===');
{
  const sb = loadFn('function setOrDel(obj, key, val)');
  const put = sb.setOrDel;

  const o1 = {};
  put(o1, 'dose', '1');
  put(o1, 'unit', '');            // 空 → 不该产生属性
  put(o1, 'note', '饭后服用');
  t('有值写入', o1.dose === '1' && o1.note === '饭后服用');
  t('★ 空值不产生空串属性', !('unit' in o1), '不该有 unit');

  const o2 = { dose: '2' };
  put(o2, 'dose', '');            // 清空 → 删属性，而不是留空串
  t('★ 清空走删属性（不是留空串）', !('dose' in o2), '应删除该键');
}

console.log('');
console.log('=== C. 源码级接线（防改回去）===');
{
  t('index.html 有剂量输入框', HTML_SRC.indexOf('id="medDose"') >= 0, '缺');
  t('index.html 有单位输入框', HTML_SRC.indexOf('id="medUnit"') >= 0, '缺');
  t('index.html 有单位候选 datalist', HTML_SRC.indexOf('id="unitList"') >= 0, '缺');
  t('index.html 有备注输入框', HTML_SRC.indexOf('id="medNote"') >= 0, '缺');
  t('★ 剂量填了非数字会被拦下',
    /doseRaw && !\/\^\\d\+\(\\\.\\d\+\)\?\$\//.test(APP), '没校验');
  t('★ 保存时写入三个字段（走 setOrDel）',
    /setOrDel\(nm, 'dose', doseRaw\)/.test(APP) && /setOrDel\(m, 'note', noteRaw\)/.test(APP), '没写入');
  t('★ 编辑时预填三个字段（否则一保存就清空原值）',
    /medDose'\)\.value = \(med && med\.dose != null\)/.test(APP)
    && /medNote'\)\.value = \(med && med\.note != null\)/.test(APP), '没预填');
  t('★ 药品卡片显示剂量', /doseLabel\(m\) \?/.test(APP), '没显示');
  t('★ 药品卡片显示备注', /m\.note \? '<p class="meta">'/.test(APP), '没显示');
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
