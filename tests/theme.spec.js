/* S-5 明暗主题（ui/theme.js + css/app.css）—— 2026-10-04
 *
 * 测三件事：
 *   ① `resolveTheme` 判定矩阵 —— 「跟随系统」要真的跟，手动档要压过系统。
 *   ② `applyTheme` 的 DOM / localStorage 副作用 —— 深色**不写属性**（它是 :root 的默认值），
 *      只有浅色才写 data-theme。写错的话"切回深色"会留下旧属性。
 *   ③ ⭐ **CSS 两套主题的变量必须对称** —— 亮色块漏一个变量，那个变量就沿用暗色值
 *      （漏了 --pressed → 亮色下按下还是深灰块），而这种错**不报任何错**，只能靠断言盯。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const THEME_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/theme.js'), 'utf8');
const CSS_SRC = fs.readFileSync(path.join(ROOT, 'www/css/app.css'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const REC_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/records.js'), 'utf8');
const APP_SRC = fs.readFileSync(path.join(ROOT, 'www/js/app.js'), 'utf8');

const strip = s => s
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^\s*\/\/.*$/gm, '');
const CSS = strip(CSS_SRC.replace(/\r\n/g, '\n'));
const THEME = strip(THEME_SRC);
const REC = strip(REC_SRC);
const APP = strip(APP_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}
const eq = (name, a, b) => t(name, JSON.stringify(a) === JSON.stringify(b),
  JSON.stringify(a) + ' !== ' + JSON.stringify(b));

/* 按花括号配对抽一段块（函数体或 CSS 规则体） */
function block(src, header) {
  const i = src.indexOf(header);
  if (i < 0) throw new Error('抽不到: ' + header);
  const b = src.indexOf('{', i);
  let d = 0, j = b;
  while (j < src.length) {
    if (src[j] === '{') d++;
    else if (src[j] === '}') { d--; if (d === 0) break; }
    j++;
  }
  return src.slice(b + 1, j);
}

/* 从一段 CSS 里抠出所有自定义属性 */
function vars(blk) {
  const out = {};
  (blk || '').split(';').forEach(function (decl) {
    const m = decl.match(/(--[a-z0-9-]+)\s*:\s*([^;]+)/i);
    if (m) out[m[1]] = m[2].trim();
  });
  return out;
}
const isColor = v => /^#[0-9a-f]{3,8}$/i.test(v) || /^rgba?\(/i.test(v);

const ROOT_VARS = vars(block(CSS, ':root{'));
const LIGHT_VARS = vars(block(CSS, 'html[data-theme="light"]{'));

/* 从真源码抽 key —— **不手写**，否则源码改了 key，spec 还照样"过" */
const KEY_VAL = (THEME.match(/THEME_KEY = '([^']+)'/) || [])[1];

/* ---------------- 沙箱：真源码 + 桩 DOM ---------------- */
/* import 行也要剥：vm Script 跑不了 ES module 语法（2026-10-09 起 theme.js
 * import 了 platform/lifecycle 的 setStatusBarIcons）。剥掉后该名字在沙箱里
 * 是 undefined —— applyTheme 里的调用包在 try/catch，安全跳过。 */
const NOEXPORT = THEME_SRC.replace(/^export\s+/gm, '').replace(/^import[^\n]*\n/gm, '');

function env(opts) {
  opts = opts || {};
  const log = { set: [], remove: [], ls: Object.assign({}, opts.stored || {}), wrote: [], mq: null };
  const sb = { console: console };
  sb.window = {};
  if (opts.matchMedia !== 'missing') {
    sb.window.matchMedia = function () {
      if (opts.throws === 'call') throw new Error('boom');
      return {
        matches: !!opts.sysDark,
        addEventListener: opts.legacyOnly ? undefined : function (type, h) { log.mq = { via: 'addEventListener', h: h }; },
        addListener: function (h) { log.mq = { via: 'addListener', h: h }; }
      };
    };
  }
  sb.document = {
    documentElement: {
      setAttribute: function (k, v) { log.set.push([k, v]); },
      removeAttribute: function (k) { log.remove.push(k); }
    },
    querySelector: function () { return null; }
  };
  sb.localStorage = {
    getItem: function (k) { return Object.prototype.hasOwnProperty.call(log.ls, k) ? log.ls[k] : null; },
    setItem: function (k, v) { log.ls[k] = String(v); log.wrote.push([k, String(v)]); }
  };
  vm.createContext(sb);
  vm.runInContext(NOEXPORT, sb);
  return { sb: sb, log: log };
}

console.log('=== 0. 自检 ===');
t('theme.js / app.css 都读到了', THEME.length > 800 && CSS.length > 5000,
  THEME.length + ' / ' + CSS.length);
t('能抽到 THEME_KEY', typeof KEY_VAL === 'string' && KEY_VAL.length > 8, String(KEY_VAL));
t('能定到 :root 块', Object.keys(ROOT_VARS).length > 15, String(Object.keys(ROOT_VARS).length));
t('能定到亮色块', Object.keys(LIGHT_VARS).length > 15, String(Object.keys(LIGHT_VARS).length));

console.log('');
console.log('=== 1. resolveTheme 判定矩阵（纯函数跑真代码）===');
const e0 = env({});
const R = e0.sb.resolveTheme;
t('手动「深色」压过系统浅色', R('dark', false) === 'dark', R('dark', false));
t('手动「深色」压过系统深色', R('dark', true) === 'dark', R('dark', true));
t('手动「浅色」压过系统深色', R('light', true) === 'light', R('light', true));
t('手动「浅色」在系统浅色下仍是浅色', R('light', false) === 'light', R('light', false));
t('★「跟随系统」在系统深色下 → dark', R('sys', true) === 'dark', R('sys', true));
t('★「跟随系统」在系统浅色下 → light', R('sys', false) === 'light', R('sys', false));

console.log('');
console.log('=== 2. loadThemeMode（默认与非法值）===');
eq('无存储 → 默认 dark（与改造前一致，零行为变化）', env({}).sb.loadThemeMode(), 'dark');
eq('存了 light → light', env({ stored: {} }).sb.loadThemeMode(), 'dark');
{
  const e = env({}); e.log.ls[KEY_VAL] = 'light';
  eq('存了 light → light', e.sb.loadThemeMode(), 'light');
}
{
  const e = env({}); e.log.ls[KEY_VAL] = 'sys';
  eq('存了 sys → sys', e.sb.loadThemeMode(), 'sys');
}
{
  const e = env({}); e.log.ls[KEY_VAL] = 'garbage';
  eq('非法值 → 回退 dark', e.sb.loadThemeMode(), 'dark');
}
{
  const e = env({}); e.log.ls[KEY_VAL] = 'DARK';
  eq('大小写不符也算非法 → dark', e.sb.loadThemeMode(), 'dark');
}

console.log('');
console.log('=== 3. applyTheme 的副作用 ===');
{
  const e = env({ sysDark: false });
  e.sb.applyTheme('dark', false);
  t('★ 深色**不写**属性（它是 :root 默认值）', e.log.set.length === 0, JSON.stringify(e.log.set));
  t('★ 深色会 removeAttribute（清掉可能残留的 light）',
    e.log.remove.indexOf('data-theme') >= 0, JSON.stringify(e.log.remove));
}
{
  const e = env({ sysDark: false });
  e.sb.applyTheme('light', false);
  eq('★ 浅色写 data-theme="light"', e.log.set, [['data-theme', 'light']]);
}
{
  const e = env({ sysDark: false });
  e.sb.applyTheme('sys', false);
  eq('跟随系统 + 系统浅色 → 写 light', e.log.set, [['data-theme', 'light']]);
}
{
  const e = env({ sysDark: true });
  e.sb.applyTheme('sys', false);
  eq('跟随系统 + 系统深色 → 不写属性', e.log.set, []);
}
{
  const e = env({});
  e.sb.applyTheme('light', true);
  eq('persist=true 会写 localStorage', e.log.wrote, [[KEY_VAL, 'light']]);
}
{
  const e = env({});
  e.sb.applyTheme('light', false);
  eq('persist=false 不写 localStorage', e.log.wrote, []);
}
{
  const e = env({});
  e.sb.applyTheme('light', true);
  eq('applied 后 themeMode 跟着更新', e.sb.themeMode, 'light');
  eq('themeLabel 反映当前档位', e.sb.themeLabel(), '浅色');
}
{
  const e = env({ matchMedia: 'missing' });
  t('★ 没有 matchMedia 也不炸（部分 WebView 缺）', (function () {
    try { e.sb.applyTheme('sys', false); return true; } catch (err) { return false; }
  })(), 'threw');
  eq('缺 matchMedia 时 systemPrefersDark 兜底 false', e.sb.systemPrefersDark(), false);
}
{
  const e = env({ throws: 'call' });
  eq('matchMedia 抛异常时兜底 false（返回空对象则 return false）', e.sb.systemPrefersDark(), false);
}

console.log('');
console.log('=== 4. 默认档位与「零行为变化」 ===');
{
  const e = env({});
  eq('源码里的 themeMode 初值是 dark', e.sb.themeMode, 'dark');
}
eq('THEME_MODES 共 3 档', (THEME.match(/THEME_MODES = \[[\s\S]*?\];/) || [''])[0].split('{ id:').length - 1, 3);
t('三档 id 是 dark / light / sys',
  /id: 'dark'/.test(THEME) && /id: 'light'/.test(THEME) && /id: 'sys'/.test(THEME), '缺档位');

console.log('');
console.log('=== 5. ⭐ CSS 两套主题的变量对称性 ===');
const ROOT_COLORS = Object.keys(ROOT_VARS).filter(k => isColor(ROOT_VARS[k]));
const missInLight = ROOT_COLORS.filter(k => !(k in LIGHT_VARS));
t('★ :root 里每个**颜色**变量，亮色块都重新定义了 ' + ROOT_COLORS.length + ' 个',
  missInLight.length === 0, '亮色块漏了: ' + missInLight.join(', '));
const extraInLight = Object.keys(LIGHT_VARS).filter(k => !(k in ROOT_VARS));
t('★ 亮色块没引入 :root 里不存在的变量（防笔误）',
  extraInLight.length === 0, '多余: ' + extraInLight.join(', '));
{
  /* 所有 var(--x) 的引用都必须有定义 —— 名字打错就是静默失效（回落到 nothing）。
   * ⚠️ 例外：`var(--x, 兜底值)` 是**显式允许未定义**的写法，不算漏。
   *    典型是 `--i`（页签指示器位置）：由 tabs.js 运行时 setProperty 注入，
   *    CSS 里本就查不到它的定义。 */
  const all = new Set(), bare = new Set();
  (CSS.match(/var\(\s*(--[a-z0-9-]+)/gi) || []).forEach(s => all.add(s.replace(/var\(\s*/i, '')));
  (CSS.match(/var\(\s*(--[a-z0-9-]+)\s*\)/gi) || []).forEach(s => bare.add(
    s.replace(/var\(\s*/i, '').replace(/\s*\)$/, '')));
  const undef = Array.from(bare).filter(k => !(k in ROOT_VARS));
  t('★ 所有**无兜底**的 var() 引用都有定义（共 ' + all.size + ' 处引用，'
    + bare.size + ' 处无兜底）', undef.length === 0, '未定义: ' + undef.join(', '));
  t('引用的变量数 > 20（说明抽取生效）', all.size > 20, String(all.size));
  t('★ 运行时注入的 --i 必须带兜底值（否则 CSS 里查不到定义 = 静默失效）',
    !bare.has('--i'), '--i 没兜底');
  t('CSS 里确实没有 --i 的定义（它由 tabs.js 注入，符合预期）',
    !('--i' in ROOT_VARS), '--i 被静态定义了？');
}
t('★ 反色块在亮色下翻转（--contrast-bg 变深）',
  ROOT_VARS['--contrast-bg'] !== LIGHT_VARS['--contrast-bg']
  && /^#[0-9a-f]{6}$/i.test(LIGHT_VARS['--contrast-bg'])
  && parseInt(LIGHT_VARS['--contrast-bg'].slice(1), 16) < 0x404040,
  ROOT_VARS['--contrast-bg'] + ' → ' + LIGHT_VARS['--contrast-bg']);
t('★ accent 在亮色下加深（浅底上原亮橙对比度只有 2.5:1）',
  ROOT_VARS['--accent'] !== LIGHT_VARS['--accent'], LIGHT_VARS['--accent']);
t('亮色下 --on-accent 变白（深橙底上的字）',
  LIGHT_VARS['--on-accent'].toLowerCase() === '#ffffff', LIGHT_VARS['--on-accent']);
t('亮色下日期选择器不再反色', LIGHT_VARS['--cal-filter'] === 'none', LIGHT_VARS['--cal-filter']);
t('两套都有 color-scheme 声明',
  /color-scheme\s*:\s*dark/.test(CSS) && /color-scheme\s*:\s*light/.test(CSS), '缺 color-scheme');

console.log('');
console.log('=== 6. CSS 不再有硬编码颜色（防回归）===');
{
  const hard = [];
  CSS.split('\n').forEach(function (L, i) {
    if (/^\s*--[a-z0-9-]+\s*:/.test(L)) return;     // :root / 主题块里的定义，允许
    if (/(#[0-9a-f]{3,8}\b|rgba?\()/i.test(L)) hard.push((i + 1) + ': ' + L.trim().slice(0, 60));
  });
  t('★ 除变量定义外，CSS 里没有裸颜色（否则亮色主题会漏改）',
    hard.length === 0, hard.slice(0, 4).join(' | '));
}
t('★ 没有用 @media (prefers-color-scheme) 判主题（判断只在 JS 一处）',
  !/prefers-color-scheme/.test(CSS), 'CSS 里混进了系统判断');

console.log('');
console.log('=== 6b. ⭐ JS 里也不许有硬编码的 SVG 颜色 ===');
{
  /* ⚠️ 2026-10-04 补：第一版只查了 `index.html` 与 CSS，**漏掉 JS 字符串里动态生成的 SVG**
   * —— sheet.js 的时刻删除图标、meds.js 的「+」、cards.js 的 check/chev 都写死了颜色。
   * 亮色主题下 `stroke="#fff"` 的图标在浅底上**直接看不见**（实际渲染时才发现）。
   * 这条断言就是为此加的：**颜色不分住在哪个文件，一律走变量或 currentColor。** */
  const jsFiles = [];
  (function walk(dir) {
    fs.readdirSync(dir).forEach(function (f) {
      const p = path.join(dir, f);
      if (fs.statSync(p).isDirectory()) return walk(p);
      if (f.endsWith('.js')) jsFiles.push(p);
    });
  })(path.join(ROOT, 'www/js'));
  const bad = [];
  jsFiles.forEach(function (p) {
    const src = strip(fs.readFileSync(p, 'utf8'));      // 剥注释：注释里提到色值不算违规
    const m = src.match(/(?:stroke|fill)\s*=\s*"(#[0-9a-f]{3,8}|rgba?\()/gi);
    if (m) bad.push(path.basename(p) + ': ' + m.join(' '));
  });
  t('★ JS 里没有硬编码的 stroke/fill 颜色（扫描 ' + jsFiles.length + ' 个文件）',
    bad.length === 0, bad.slice(0, 3).join(' | '));
  t('index.html 里也没有硬编码的 stroke/fill',
    !/(?:stroke|fill)="#[0-9a-f]{3,8}"/i.test(HTML_SRC), 'HTML 里有');
  t('扫描确实覆盖到文件了（防"空集通过"）', jsFiles.length >= 20, String(jsFiles.length));
}

console.log('');
console.log('=== 7. 首屏防闪内联脚本与 theme.js 一致 ===');
{
  const m = HTML_SRC.match(/localStorage\.getItem\('([^']+)'\)/);
  t('★ 内联脚本读的 key 与 theme.js 的 THEME_KEY 一致',
    m && m[1] === KEY_VAL, (m && m[1]) + ' vs ' + KEY_VAL);
}
t('内联脚本在 <link rel="stylesheet"> 之前（否则会先按旧主题画一遍）',
  HTML_SRC.indexOf('medreminder.theme.v1') < HTML_SRC.indexOf('css/app.css'), '顺序反了');
t('内联脚本认 light / sys 两个档位字样',
  /'light'/.test(HTML_SRC.slice(HTML_SRC.indexOf('medreminder.theme.v1') - 400,
    HTML_SRC.indexOf('css/app.css'))) &&
  /'sys'/.test(HTML_SRC.slice(HTML_SRC.indexOf('medreminder.theme.v1') - 400,
    HTML_SRC.indexOf('css/app.css'))), '档位名对不上');
t('index.html 声明了 theme.js 的 modulepreload',
  /modulepreload" href="js\/ui\/theme\.js"/.test(HTML_SRC), '少了 modulepreload');

console.log('');
console.log('=== 8. 接线 ===');
t('records.js 引入了 theme.js', /from '\.\/theme\.js'/.test(REC), '没接');
t('records.js 渲染了三档按钮', /THEME_MODES\.map/.test(REC), '没渲染');
t('★ 按钮属性用 data-theme-mode（不能占用 data-theme）',
  /data-theme-mode/.test(REC), '属性名不对');
t('records.js 绑定了 data-theme-mode 的点击',
  /\$\$\('\[data-theme-mode\]'\)/.test(REC), '没绑定');
t('切换后调 applyTheme(..., true) 落盘', /applyTheme\(m, true\)/.test(REC), '没落盘');
t('app.js 在 boot 里调了 initTheme()', /initTheme\(\)/.test(APP), '没初始化');
t('app.js 引入了 initTheme', /initTheme/.test(APP) && /from '\.\/ui\/theme\.js'/.test(APP_SRC), '没引入');
t('sw.js 预缓存了 theme.js',
  fs.readFileSync(path.join(ROOT, 'www/sw.js'), 'utf8').indexOf('./js/ui/theme.js') >= 0, 'SW 漏了');
t('tests/sources.js 登记了 theme.js',
  fs.readFileSync(path.join(ROOT, 'tests/sources.js'), 'utf8').indexOf('ui/theme.js') >= 0, '漏登记');
t('verify-apk.py 登记了 theme.js',
  fs.readFileSync(path.join(ROOT, 'tools/verify-apk.py'), 'utf8').indexOf('js/ui/theme.js') >= 0, '漏登记');

console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('  失败清单：');
  fails.forEach(f => console.log('    - ' + f));
}
process.exit(fail ? 1 : 0);
