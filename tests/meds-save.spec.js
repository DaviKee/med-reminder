/* 「保存」防重入（2026-09-24 真机 bug 回归）
 *
 * 现场：药品页出现 **7 条同名同刻的药**（都叫「1」、都是「每天 14:45 / 18:45」）。
 *
 * 排查结论：能凭空多出药品的路径**只有一条** —— 在「添加」模式下点「保存」
 * （`S.meds.push`）；编辑已有药品走的是原地修改，绝不会新增。
 * 所以那 7 条 = 「保存」被触发了 7 次，且都在同一个浮层会话里
 * （同一个会话里名字输入框不会被清空，所以每条都一模一样）。
 *
 * 慢设备（或开了双击缩放）上连点会派发多次 click，而每次点击都是一次完整的「新增」。
 * 修法：用「本次浮层会话是否已保存过」做判据 —— 而不是「函数当前是否在执行」，
 * 后者挡不住"排队之后才派发"的重复点击，而那正是连点的特征。
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
const eq = (name, a, b) => t(name, JSON.stringify(a) === JSON.stringify(b),
  JSON.stringify(a) + ' !== ' + JSON.stringify(b));

/* 从真实源码里按花括号配对抽块（不能按分号截断：函数体里就有分号） */
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

function makeEnv() {
  const sandbox = { console: { log() {}, warn() {}, error() {} } };
  vm.createContext(sandbox);
  vm.runInContext(
    'var sheetSaveDone = false;\n'
    + cut(APP_SRC, 'function sheetSaveBegin()') + '\n'
    + cut(APP_SRC, 'function sheetSaveReset()') + '\n',
    sandbox);
  return sandbox;
}

console.log('=== A. 防重入的基本行为 ===');
{
  const S = makeEnv();
  t('★ 第一次点「保存」→ 放行', S.sheetSaveBegin() === true, '被挡了');
  t('★ 紧接着第二次（连点）→ 挡掉', S.sheetSaveBegin() === false, '没挡住 —— bug 会复发');
  t('第三次、第四次仍然挡掉', S.sheetSaveBegin() === false && S.sheetSaveBegin() === false, '漏了');
}

console.log('');
console.log('=== B. 模拟真实症状：连点 7 次 ===');
{
  const S = makeEnv();
  let medsCreated = 0;
  for (let i = 0; i < 7; i++) if (S.sheetSaveBegin()) medsCreated++;
  eq('★ 连点 7 次只新增 1 条药（修前是 7 条）', medsCreated, 1);

  /* 真实症状复核：修前的判据若是「正在执行」(saving 标志会立刻复位)，
   * 排队派发的点击会全部放行 —— 这正是 7 条的来源。这里用对照实现说明差异。 */
  let reentrant = 0, running = false;
  for (let i = 0; i < 7; i++) {
    if (running) continue;
    running = true;
    reentrant++;          // "正在执行"式守卫
    running = false;      // 处理完立刻复位 → 排队的点击全部放行
  }
  eq('对照：「正在执行」式守卫挡不住排队点击（会放行全部 7 次）', reentrant, 7);
}

console.log('');
console.log('=== C. 新会话要能重新保存 ===');
{
  const S = makeEnv();
  t('第一次保存放行', S.sheetSaveBegin() === true, '');
  t('同会话第二次挡掉', S.sheetSaveBegin() === false, '');
  S.sheetSaveReset();                       // = 重新打开浮层
  t('★ 重开浮层后可再次保存（不会把保存功能锁死）', S.sheetSaveBegin() === true, '锁死了');
  t('新会话的第二次同样挡掉', S.sheetSaveBegin() === false, '');
}

console.log('');
console.log('=== D. 源码级接线（防改回去）===');
{
  t('守卫生效在「保存」处理函数的最前面',
    /saveMed'\)\.onclick = function \(\) \{[\s\S]{0,260}?if \(!sheetSaveBegin\(\)\) return;/.test(APP),
    '守位不对或缺失');
  t('打开浮层会重置标记（openSheet 里）',
    /function openSheet\(medId\) \{[\s\S]{0,160}?sheetSaveReset\(\);/.test(APP), '没重置');
  t('标记与两个函数都存在',
    APP.indexOf('var sheetSaveDone = false;') >= 0
    && APP.indexOf('function sheetSaveReset()') >= 0, '缺');
  t('守卫加在「读名称」之前（连点时不会先弹"请填写药品名称"）',
    APP.indexOf('if (!sheetSaveBegin()) return;') < APP.indexOf("var name = $('#medName').value.trim();"),
    '顺序不对');
  /* 回归：防重入不能把「编辑」也锁住 —— 编辑走的是同一处理函数，
   * 所以重置必须挂在 openSheet（打开浮层）而不是 closeSheet。 */
  t('重置挂在 openSheet，而不是 closeSheet',
    /function openSheet\(medId\) \{[\s\S]{0,160}?sheetSaveReset\(\);/.test(APP), '挂错地方');
}

console.log('');
console.log('==========================================');
console.log('通过 %d / 共 %d', pass, pass + fail);
if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
process.exit(fail ? 1 : 0);
