/* 布局守卫：长 token 不许顶出容器（2026-09-28 真机回归）
 *
 * 现场：「记录」页的 AUTO · 自动备份卡里，
 * 「位置：Android/data/com.medreminder.app/files/MedReminder」这一行**顶出了卡片右边**，
 * 末尾被屏幕裁掉。
 *
 * 根因：那是一整串**没有空白的 token**。按 UAX #14 的断行规则，`/`(SY) 与 `.`(IS)
 * 都**不是断行点** —— 浏览器只在空白与 CJK 字符处断行，所以整串不可断。
 *
 * 修法两层：
 *   ① 备份目录路径：每个 `/` 后插 `<wbr>`（零宽断行机会）→ 断在目录分隔处，最整洁；
 *   ② CSS 给 `main` 加 `overflow-wrap:anywhere` 兜底 —— 别处再冒出长 token
 *      （插件版本号、插件报错里的路径、UA）也不会顶出去。靠**继承**覆盖全部正文。
 *
 * ⚠️ 但 `anywhere` 不能用错地方：`.dose-n`（药名）是**刻意**用
 *    `nowrap + overflow:hidden + text-overflow:ellipsis` 截断的，别被"顺手改成可断行"。
 *    本套件把它一起盯住。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const CSS_SRC = fs.readFileSync(path.join(ROOT, 'www/css/app.css'), 'utf8');

/* ⚠️ CSS 也要剥注释：否则断言可能命中注释里的字样，而不是真的声明 */
const strip = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = strip(APP_SRC);
const CSS = strip(CSS_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}

/* 从真实源码里按花括号配对抽块 */
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
/* 取一条 CSS 规则的整段（用于断言"这条规则里有没有某声明"） */
function rule(sel) {
  const i = CSS.indexOf(sel + '{');
  if (i < 0) return null;
  const j = CSS.indexOf('}', i);
  return j < 0 ? null : CSS.slice(i, j + 1);
}

console.log('=== 0. 自检：文件真的读到了 ===');
t('CSS 与源码都非空', CSS.length > 5000 && APP.length > 50000, CSS.length + ' / ' + APP.length);
t('自检：能定位到 main 规则', rule('main') !== null, '规则名变了？');
t('自检：能定位到 .dose-n 规则', rule('.dose-n') !== null, '规则名变了？');

console.log('');
console.log('=== 1. 兜底：正文允许长 token 断行 ===');
const mainRule = rule('main') || '';
t('★ main 规则里声明了 overflow-wrap:anywhere',
  mainRule.indexOf('overflow-wrap:anywhere') >= 0, mainRule.slice(0, 80));
t('用的是 anywhere 而不是 break-all（break-all 见缝就断，会把正常单词也切碎）',
  mainRule.indexOf('break-all') < 0, mainRule.slice(0, 80));

console.log('');
console.log('=== 2. 定点：备份目录路径插了断行机会 ===');
t('源码里走「先 esc 再插 <wbr>」', APP.indexOf("replace(/\\//g, '/<wbr>')") >= 0, '没接上');
t('AUTO 卡那句位置说明仍在（没被顺手删掉）',
  APP.indexOf('Android/data/com.medreminder.app/files/') >= 0
  && APP.indexOf('（用电脑 USB 能看到）') >= 0, '缺');

console.log('');
console.log('=== 3. ★ 行为：插标记不许改变显示出来的文字 ===');
{
  const ctx = vm.createContext({});
  vm.runInContext(cut(APP, 'function esc(') + ';\nglobalThis.esc = esc;', ctx);
  const raw = 'Android/data/com.medreminder.app/files/MedReminder';

  /* 与源码同一条表达式：esc(...) 之后把每个 / 换成 /<wbr> */
  const marked = ctx.esc(raw).replace(/\//g, '/<wbr>');

  t('每个 / 后都插了断行标记（4 个 / → 4 个 <wbr>）',
    (marked.match(/<wbr>/g) || []).length === 4, marked);
  t('★ 去掉标记后文本一字不差（修法只加断行机会，不改内容）',
    marked.replace(/<wbr>/g, '') === raw, marked.replace(/<wbr>/g, ''));
  t('★ 每个 <wbr> 都紧跟在一个 / 之后（断在目录分隔处，不在词中间）',
    (marked.match(/[^/]<wbr>/g) || []).length === 0, marked);
  t('每段都非空（没把 / 变成孤立的分隔符）',
    marked.split('<wbr>').every(function (seg) { return seg.replace(/\//g, '').length > 0; }), marked);

  /* 反例：不做任何处理时，整串确实不含任何断行机会 —— 这就是当初溢出的原因 */
  t('对照：原样输出的字符串里没有任何断行机会（所以当初会顶出去）',
    (ctx.esc(raw).match(/<wbr>|\s|[\u3000-\u9fff]/g) || []).length === 0, '居然有断点？');
}

console.log('');
console.log('=== 4. 别把「刻意的省略号截断」也改掉 ===');
const doseRule = rule('.dose-n') || '';
t('.dose-n（药名）仍是 nowrap + ellipsis 截断',
  doseRule.indexOf('white-space:nowrap') >= 0 && doseRule.indexOf('text-overflow:ellipsis') >= 0,
  doseRule.slice(0, 90));
t('.dose-l 仍允许收缩（min-width:0，否则省略号不会生效）',
  (rule('.dose-l') || '').indexOf('min-width:0') >= 0, rule('.dose-l'));

console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('  失败清单：');
  fails.forEach(f => console.log('    - ' + f));
}
process.exit(fail ? 1 : 0);
