/* Android 返回键 / 侧滑返回（2026-09-24 反馈：没有滑动返回，只能杀进程重开）
 *
 * 背景：全项目搜不到一处 `App.addListener('backButton')` —— 返回键从来没被处理过。
 * 本轮补上，优先级：**关浮层 → 切回「今天」→ 退出 App**。
 *
 * ⚠️ 提醒弹窗（dlgRemind）与确认框（dlgConfirm）**不许**被返回键抹掉 ——
 *    前者要求明确选「已服用 / 稍后」，后者要用户明确回答；
 *    此时要"吃掉"这次返回（既不关也不退出），否则误按一次就直接退出 App。
 *
 * 判定逻辑抽成了纯函数 `backAction(s)`，所以这里能把判定矩阵整个跑一遍，
 * 而不是靠"看代码觉得对"。
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

const sandbox = { console: { log() {}, warn() {}, error() {} } };
vm.createContext(sandbox);
vm.runInContext(cut(APP_SRC, 'function backAction(s)') + '\n', sandbox);
const B = sandbox.backAction;

const S = o => Object.assign({ closableDlg: false, sheetOpen: false, anyDlg: false, tab: 'today' }, o);

console.log('=== A. 判定矩阵 ===');
{
  eq('★ 有可关浮层 → 关它', B(S({ closableDlg: true })), 'close-dlg');
  eq('★ 只有编辑浮层 → 关浮层', B(S({ sheetOpen: true })), 'close-sheet');
  eq('★ 只剩提醒弹窗 → 吃掉返回（不关、也不退出）', B(S({ anyDlg: true })), 'consume');
  eq('不在「今天」页 → 切回今天', B(S({ tab: 'meds' })), 'go-today');
  eq('记录页同理', B(S({ tab: 'records' })), 'go-today');
  eq('★ 已在「今天」页且无浮层 → 退出 App', B(S({ tab: 'today' })), 'exit');
}

console.log('');
console.log('=== B. 优先级（顺序错了会出怪事）===');
{
  eq('★ 浮层优先于「切回今天」', B(S({ sheetOpen: true, tab: 'records' })), 'close-sheet');
  eq('★ 可关弹窗优先于编辑浮层', B(S({ closableDlg: true, sheetOpen: true })), 'close-dlg');
  eq('★ 可关弹窗优先于「切回今天」', B(S({ closableDlg: true, tab: 'records' })), 'close-dlg');
  eq('★ 提醒弹窗优先于「切回今天」也不退出',
    B(S({ anyDlg: true, tab: 'records' })), 'consume');
  eq('★ 提醒弹窗在「今天」页也只是吃掉返回（不能退出 App）',
    B(S({ anyDlg: true, tab: 'today' })), 'consume');
}

console.log('');
console.log('=== C. 源码级接线（防改回去）===');
{
  /* ⚠️ 2026-10-01 架构重构（多平台）：返回键的**注册**移到了 platform/lifecycle.js。
   * 断言改为「平台层注册事件 + 业务层把 handleBackButton 接上」——
   * 原来锁死了 addListener 的字面写法（连空格都钉住），一搬就是假失败。 */
  t('★ 注册了返回键监听',
    /addListener\('backButton'/.test(APP), '没注册 —— 侧滑返回会继续没反应');
  t('返回键走 handleBackButton',
    /onBackButton\(handleBackButton\)/.test(APP), '没接上');
  t('★ Esc 与返回键共用同一套「关最上层」逻辑',
    /if \(closeTopDialog\(\)\) return;/.test(APP), '各写一套，迟早不一致');
  t('提醒弹窗不在可关清单里',
    /closableDialogs\(\)[\s\S]{0,220}?dlgData' \|\| el\.id === 'dlgSkip' \|\| el\.id === 'dlgClean'/.test(APP)
    && APP.indexOf("el.id === 'dlgRemind'") < 0, '提醒弹窗被误列进可关清单');
  t('关拍照弹窗时清掉待恢复标记',
    /dlgPhoto'\) \{ pendingShot = null; clearPendingShot\(\); \}/.test(APP), '缺');
  t('exitApp 有 App.exitApp 与 Cordova 兜底',
    /A\.exitApp/.test(APP) && /navigator\.app\.exitApp/.test(APP), '缺兜底');
}

console.log('');
console.log('=== D. 点击被吞的三个修复（同一批）===');
{
  t('★ openSheet 防重复打开（连点「+」不再堆积）',
    /function openSheet\(medId\) \{[\s\S]{0,400}?if \(\$\('#sheetMed'\)\.classList\.contains\('show'\)\) return;/.test(APP),
    '缺守卫');
  t('★ 药品页改用事件委托（[data-med]）',
    /addEventListener\('click'[\s\S]{0,400}?closest\('\[data-med\]'\)/.test(APP), '缺委托');
  t('★ renderMeds 里不再逐个绑 onclick（否则会双触发）',
    APP.indexOf("$$('.meditem').forEach") < 0
    && APP.indexOf("if (add) add.onclick = function () { openSheet(null); };") < 0,
    '旧绑定还在，会与委托双触发');
  t('示例药删除改成具名函数（供委托调用）',
    APP.indexOf('function dropLegacySamples()') >= 0, '缺');
  t('★ refreshPerm 有浮层时不重绘',
    /if \(isOverlayOpen\(\)\) return;\s*\n\s*render\(\);/.test(APP), '缺守卫');
  /* 自检：上面那条断言的名字必须真的存在，否则正则只是在验一个不存在的写法 */
  t('自检：isOverlayOpen 确实定义在模块里', APP.indexOf('function isOverlayOpen()') >= 0, '名字对不上');
}

console.log('');
console.log('==========================================');
console.log('通过 %d / 共 %d', pass, pass + fail);
if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
process.exit(fail ? 1 : 0);
