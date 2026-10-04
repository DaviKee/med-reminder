/* 选项卡切换动效（2026-10-04）
 *
 * 真机反馈"切换页面太硬" —— 原来是 `.view` 从 display:none 跳到 block 的硬切，
 * 全项目连一个 @keyframes 都没有。本轮加了两处动效：
 *   ① 新页沿切换方向滑入（方向感知）
 *   ② 底部白色胶囊平滑滑到目标页签
 *
 * 本文件盯三类东西：
 *   A. **判定矩阵**（跑真代码）—— 方向对不对、首屏/同页该不该播
 *   B. **动效不能反过来咬功能** —— 胶囊挡住点击（pointer-events）、
 *      白底盖住图标（z-index）、滑动挤出横向滚动条（overflow-x）、
 *      临时绝对定位把布局搞塌（.view 必须保持流式）
 *   C. **单一真源** —— 页签顺序只认 DOM；CSS 里的除数跟着页签个数走
 *
 * ⚠️ 这个项目踩过的坑：**断言别锁实现字面**（搬家一次能打哑四条）。
 *    所以下面查的是"做了什么"，不是"怎么写的"。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const HTML = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const CSS = fs.readFileSync(path.join(ROOT, 'www/css/app.css'), 'utf8');
const TABS = fs.readFileSync(path.join(ROOT, 'www/js/ui/tabs.js'), 'utf8');

const strip = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = strip(APP_SRC);
const CSS_CLEAN = strip(CSS);
const HTML_CLEAN = strip(HTML);

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
t('自检：能抽到 slideClass', cut(APP, 'function slideClass(').length > 80, '函数改名了？');
t('自检：能抽到 setTab', cut(APP, 'function setTab(').length > 200, '函数改名了？');

console.log('');
console.log('=== 1. 方向判定矩阵（跑真代码）===');
const ctx = vm.createContext({});
vm.runInContext([
  cut(APP, 'function slideClass('),
  'globalThis.slideClass = slideClass;',
].join('\n'), ctx);
const SC = ctx.slideClass;

eq('往右切（今日 0 → 药品 1）→ 新页从**右**滑入', SC(0, 1), 'enter-from-right');
eq('继续往右（药品 1 → 记录 2）→ 从右滑入', SC(1, 2), 'enter-from-right');
eq('往左切（记录 2 → 药品 1）→ 新页从**左**滑入', SC(2, 1), 'enter-from-left');
eq('一直往左（记录 2 → 今日 0）→ 从左滑入', SC(2, 0), 'enter-from-left');
eq('跨两格往右（今日 → 记录）→ 仍然从右滑入', SC(0, 2), 'enter-from-right');

eq('★ 首屏（from = -1）→ 不播（打开 App 时滑一下是多余的）', SC(-1, 0), null);
eq('★ 同页（点了当前页签）→ 不播', SC(1, 1), null);
eq('找不到目标（to = -1）→ 不播', SC(0, -1), null);
eq('来路也找不到（from = -1 且 to = -1）→ 不播', SC(-1, -1), null);

console.log('');
console.log('=== 2. 接线（源码级）===');
t('★ setTab 用 slideClass 取动画类（判定与执行分开）',
  /var cls = slideClass\(from, to\);/.test(APP), '没接上');
t('★ 每次都先摘掉两个入场 class（不摘则同一页不会重播）',
  /classList\.remove\('enter-from-right', 'enter-from-left'\)/.test(APP), '没清理');
t('只有目标页加动画类（非目标页直接 return）',
  /if \(!active\) return;/.test(APP), '会给所有页加');
t('★ 白胶囊的 --i 由 setTab 写（位置不写死在 CSS 里）',
  /setProperty\('--i'/.test(APP), '没接线');
t('★ 页签顺序取自 DOM，本模块不再另写一份名单',
  !/TAB_ORDER\s*=/.test(strip(TABS)), '又写了一份顺序（两份必然有一天对不上）');
t('顺序确实来自 .tab 的遍历下标',
  /tabs\.forEach\(function \(t, i\)/.test(APP), '没按 DOM 顺序算下标');
t('切页仍回顶部（老行为不能丢）', /scrollTop = 0;/.test(APP), '丢了');

console.log('');
console.log('=== 3. ⚠️ 动效不能反过来咬功能 ===');
t('★ 胶囊 pointer-events:none（否则挡住点击 → 页签点不动）',
  /\.tab-ind\{[^}]*pointer-events:none/.test(CSS_CLEAN), '会挡住点击');
t('★ .tab 抬到胶囊之上（否则白底盖住图标文字）',
  /\.tab\{[^}]*z-index:1/.test(CSS_CLEAN), '会被白底盖住');
t('★ .tabpill 是定位祖先（否则胶囊跑到别处）',
  /\.tabpill\{[^}]*position:relative/.test(CSS_CLEAN), '定位会错');
t('★ main 限制横向溢出（滑动位移期间不挤横向滚动条）',
  /main\{[^}]*overflow-x:hidden/.test(CSS_CLEAN), '会出横向滚动条');
t('main 仍可纵向滚动（别为了动画把滚动改没了）',
  /main\{[^}]*overflow-y:auto/.test(CSS_CLEAN), '纵向滚动丢了');
t('★ .view 仍是流式布局（没被改成 absolute —— 那会让内容高度塌成 0）',
  !/\.view\s*\{[^}]*position:\s*absolute/.test(CSS_CLEAN), '视图被绝对定位了');
t('★ 白底只有一处来源（.tab.active 不再自带 background）',
  !/\.tab\.active\{[^}]*background/.test(CSS_CLEAN), '两层白底会错位');

console.log('');
console.log('=== 4. 动画本身 ===');
t('有 viewInRight / viewInLeft 两个 keyframes',
  /@keyframes viewInRight/.test(CSS_CLEAN) && /@keyframes viewInLeft/.test(CSS_CLEAN), '缺 keyframes');
t('.view.enter-from-right 绑 viewInRight',
  /\.view\.enter-from-right\{animation:viewInRight/.test(CSS_CLEAN), '绑错');
t('.view.enter-from-left 绑 viewInLeft',
  /\.view\.enter-from-left\{animation:viewInLeft/.test(CSS_CLEAN), '绑错');
t('胶囊位移用 calc + --i（一格 = 自身宽 + gap）',
  /\.tab-ind\{[^}]*translateX\(calc\(var\(--i, 0\) \* \(100% \+ 4px\)\)\)/.test(CSS_CLEAN), '位移公式不对');
t('胶囊有 transform 过渡（否则还是瞬移）',
  /\.tab-ind\{[^}]*transition:transform/.test(CSS_CLEAN), '没有过渡');
t('页签文字颜色也跟着过渡（跟胶囊同步）',
  /\.tab\{[^}]*transition:color/.test(CSS_CLEAN), '颜色会瞬变，和胶囊打架');
t('★ 动画被「减少动态效果」通杀（系统设置优先）',
  /@media \(prefers-reduced-motion: reduce\)/.test(CSS_CLEAN)
  && /animation-duration:\.001ms !important/.test(CSS_CLEAN), '没有 reduced-motion 兜底');

console.log('');
console.log('=== 5. 单一真源：页签个数 ===');
{
  const htmlTabs = (HTML_CLEAN.match(/data-tab="/g) || []).length;
  const div = (CSS_CLEAN.match(/\.tab-ind\{[^}]*width:calc\(\(100% - 16px\) \/ (\d+)\)/) || [])[1];
  eq('HTML 里确实有 3 个页签', htmlTabs, 3);
  eq('★ CSS 胶囊宽度的除数 == 页签个数（加第 4 个页签时必须一起改）',
    Number(div), htmlTabs);
  t('胶囊在 .tabpill 内',
    /<div class="tabpill">[\s\S]{0,200}class="tab-ind"[\s\S]{0,200}<button class="tab/.test(HTML_CLEAN), '位置不对');
  t('胶囊在页签之前（当背景层）',
    HTML_CLEAN.indexOf('tab-ind') < HTML_CLEAN.indexOf('data-tab="today"'), '顺序不对');
  t('胶囊是装饰层（aria-hidden）',
    /class="tab-ind" aria-hidden="true"/.test(HTML), '缺 aria-hidden');
}

console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('  失败清单：');
  fails.forEach(f => console.log('    - ' + f));
}
process.exit(fail ? 1 : 0);
