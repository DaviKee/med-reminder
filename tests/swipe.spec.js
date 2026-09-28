/* 左右滑动切换选项卡（2026-09-28 新增功能）
 *
 * 判定抽成纯函数 `swipeTarget(cur, dx, dy)` —— 与 backAction 同一套路：
 * DOM 事件在调用方拍好再传进来，所以判定矩阵可以直接跑，不必造触摸事件。
 *
 * 三条否决（都单独立了用例）：
 *   ① 位移太短 → 手抖/误触
 *   ② 竖向位移不小于横向 → 用户是在滚页面，不是在切页
 *   ③ 已经到了两端 → 停住，不绕回（绕回会让人晕）
 *
 * ⚠️ 还盯住两件容易在改动中被破坏的事：
 *   · 监听必须 passive，且**不能设 touch-action** —— 设了会把双指缩放一起废掉，
 *     而 capacitor.config 里的 zoomEnabled:true 是刻意开的。
 *   · 浮层开着时不切页；双指（缩放）不参与。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const HTML = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const CSS = fs.readFileSync(path.join(ROOT, 'www/css/app.css'), 'utf8');

const strip = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = strip(APP_SRC);

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

console.log('=== 0. 自检 ===');
t('聚合源码非空', APP.length > 50000, APP.length);
t('自检：能抽到 swipeTarget', cut(APP, 'function swipeTarget(').length > 100, '函数改名了？');

console.log('');
console.log('=== 1. 判定矩阵（跑真代码）===');
const ctx = vm.createContext({});
vm.runInContext([
  cut(APP, 'var TAB_ORDER ='),
  cut(APP, 'var SWIPE_MIN_PX ='),
  cut(APP, 'function swipeTarget('),
  'globalThis.swipe = swipeTarget;',
].join('\n'), ctx);
const S = ctx.swipe;

eq('左滑 80px（横向为主）→ 下一个', S('today', -80, 5), 'meds');
eq('右滑 80px → 上一个', S('records', 80, 5), 'meds');
eq('从「药品」左滑 → 记录', S('meds', -80, 5), 'records');
eq('从「药品」右滑 → 今天', S('meds', 80, 5), 'today');

eq('★ 太短（30px）→ 不切（手抖不算手势）', S('today', -30, 0), null);
eq('★ 竖向为主（dx80 dy100）→ 不切（让滚动赢）', S('today', -80, -100), null);
eq('竖向为主时 dx 再大也不切', S('today', -150, 160), null);
eq('★ 左端再右滑 → 停住不绕回', S('today', 80, 0), null);
eq('★ 右端再左滑 → 停住不绕回', S('records', -80, 0), null);
eq('未知当前页 → 不切（防越界）', S('', -80, 0), null);
eq('斜向但横向略占优（dx80 dy70）→ 切页', S('today', -80, 70), 'meds');

console.log('');
console.log('=== 2. 接线（源码级）===');
t('boot 里挂了滑动', /bindSwipe\(\);/.test(APP), '没接上');
t('监听挂在 #main（不会被重建，绑一次就够）', /var main = \$\('#main'\)/.test(APP), '绑错容器');
t('★ 两条监听都是 passive（不阻断滚动）',
  (APP.match(/\{ passive: true \}/g) || []).length >= 2, '不是 passive');
t('★ 没有设 touch-action（设了会把双指缩放一起废掉）',
  !/touch-action/.test(CSS) && !/touch-action/.test(APP), '别加 touch-action');
t('浮层开着时不切页', /isOverlayOpen\(\)\) return;/.test(APP), '缺守卫');
t('双指（缩放）不参与', /ev\.touches\.length !== 1/.test(APP), '缺守卫');
t('落在浮层/输入框上的触摸不参与',
  /closest\('\.dlg-wrap, \.sheet, input, textarea, \.dlg-scroll'\)/.test(APP), '缺守卫');
t('切页复用 setTab（不另写一套）', /if \(next\) setTab\(next\);/.test(APP), '没复用');

console.log('');
console.log('=== 3. 三个选项卡的顺序与判定表一致 ===');
{
  const order = (APP.match(/var TAB_ORDER = \[([^\]]*)\]/) || [])[1] || '';
  const htmlOrder = (HTML.match(/data-tab="([a-z]+)"/g) || []).map(s => s.slice(10, -1));
  eq('TAB_ORDER 与底部导航顺序一致', order.replace(/['\s]/g, '').split(','), htmlOrder);
}

console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('  失败清单：');
  fails.forEach(f => console.log('    - ' + f));
}
process.exit(fail ? 1 : 0);
