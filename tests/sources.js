/* 测试用的「源码聚合器」
 *
 * 为什么需要它
 * ------------
 * 2026-09-24 架构重构后，www/js/ 下是 **ES module**（import/export）。
 * 但测试的做法是「读源码真实文本 → 在 Node vm 里抽函数块 → 断言」，
 * 而 vm 不认 import/export 语法。
 *
 * 所以这里把所有 app 模块按**依赖顺序**拼成"同一个作用域下的一个 script"，
 * 并剥掉 import / export —— 效果等价于打包器产出的 bundle。
 * 这样测试里的 cut(src, 'function fmtDate(d)') 不论函数搬到了哪个模块都能找到，
 * 不用为每个模块各写一套加载逻辑。
 *
 * ⚠️ 加新模块时：在 MODULES 里加一行（顺序 = 依赖顺序，低在前）。
 *    顺序错了会在 vm 里报 "X is not defined" —— 这正是漏加模块的信号。
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const JS = path.join(ROOT, 'www', 'js');

/* app.js 的模块图（依赖顺序：底层在前）。
 * notify.js / photo.js / backup.js 是**独立的 IIFE script**（挂 window.MedXxx），
 * 不属于这个图 —— 它们各自被对应的 spec 单独读取。 */
const MODULES = [
  /* platform/lifecycle.js 无依赖（只用 window.Capacitor），放最前 */
  'platform/lifecycle.js',
  'core/util.js',
  'core/store.js',
  'core/schedule.js',
  'core/cardSummary.js',
  'ui/tabs.js',
  'ui/render.js',
  'ui/cards.js',
  'ui/actions.js',
  'ui/toast.js',
  'ui/overlay.js',
  'ui/photo.js',
  'ui/permission.js',
  'ui/fontsize.js',
  'ui/theme.js',
  'ui/sheet.js',
  'ui/today.js',
  'ui/data.js',
  'ui/records.js',
  'ui/viz.js',
  'ui/meds.js',
  'app.js'
];

/* 剥掉模块语法，让代码能在普通 script 作用域里跑。 */
function stripModuleSyntax(src) {
  return src
    // 删 import 声明（支持跨行写法）
    .replace(/^[ \t]*import\s+[\s\S]*?from\s+['"][^'"]+['"]\s*;?[ \t]*$/gm, '')
    // 删 export 前缀，保留后面的声明本身
    .replace(/^[ \t]*export\s+(?=(?:default\s+)?(?:function|class|const|let|var))/gm, '');
}

function read(rel) {
  return fs.readFileSync(path.join(JS, rel), 'utf8');
}

let _cache = null;

/** 聚合后的源码文本（等价于 bundle）。 */
function all() {
  if (_cache === null) {
    _cache = MODULES.map(function (m) {
      return '/* ==== ' + m + ' ==== */\n' + stripModuleSyntax(read(m));
    }).join('\n\n');
  }
  return _cache;
}

/** 读单个独立 script（notify.js / photo.js / backup.js）。 */
function standalone(rel) {
  return read(rel);
}

/* ---- CSS（2026-10-17 P1 四拆分后）----
 * index.html 按 tokens → base → components → views 的顺序加载四个文件，
 * 合并后与拆分前的 app.css 等价（拆分时做过规则多重集比对，零视觉变化）。
 * spec 里原先读 app.css 的地方统一改成读这里 —— 测试论文不变
 * （仍是"读真实源码文本"），只是源从一个文件变成四个文件的按序拼接。 */
const CSS_DIR = path.join(ROOT, 'www', 'css');
const CSS_FILES = ['tokens.css', 'base.css', 'components.css', 'views.css'];

/** 全部样式文本：按 index.html 的加载顺序拼接（等价于拆分前的 app.css）。 */
function cssAll() {
  return CSS_FILES.map(function (f) {
    return fs.readFileSync(path.join(CSS_DIR, f), 'utf8');
  }).join('\n');
}

module.exports = { all: all, standalone: standalone, stripModuleSyntax: stripModuleSyntax, read: read, MODULES: MODULES, cssAll: cssAll };
