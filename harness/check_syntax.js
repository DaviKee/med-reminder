/* harness 关口 0：JS 语法自检
 *
 * 为什么不用 `node --check` 逐个文件？
 *   - www/js 下已是 ES module，`node --check foo.js` 对 .js 会按 CommonJS 解析，
 *     遇到 import/export 直接报"Unexpected token 'export'" —— 假阳性。
 *   - 所以这里用 `new vm.SourceTextModule`（Node 22 默认可用）或退回到
 *     "剥掉 import/export 后按 script 解析"，两者都能真实反映语法是否正确。
 *
 * 退出码：0 = 全部通过；1 = 有文件语法错误。绝不在出错时假装成功。
 */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');

/* 待检查的文件：www/js 下全部 .js + tests 下全部 .spec.js */
function collect() {
  const out = [];
  const jsDir = path.join(ROOT, 'www', 'js');
  (function walk(d) {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      const p = path.join(d, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name.endsWith('.js')) out.push(p);
    }
  })(jsDir);
  const tDir = path.join(ROOT, 'tests');
  for (const f of fs.readdirSync(tDir)) {
    if (f.endsWith('.js')) out.push(path.join(tDir, f));
  }
  return out.sort();
}

/* 把模块语法剥掉，得到一个可被 `new vm.Script` 解析的脚本。
 * 只做"能不能解析"的检查，不求语义正确。
 *
 * ⚠️ 这些正则必须容忍「行尾有注释」—— 例如
 *     import { fmtDate, todayKey } from './util.js';   // cleanTargets 用到
 * 早期版本要求 `from '...'` 之后立刻是行尾，于是这一行没被剥掉，
 * 报出假的 "Cannot use import statement outside a module"。 */
function strip(src) {
  return src
    // 删 import 声明（含行尾注释）—— 支持跨行写法
    .replace(/^[ \t]*import\s+[\s\S]*?from\s*['"][^'"]+['"]\s*;?[^\n]*$/gm, '')
    // 删裸 import（无 from）：import './x.js';
    .replace(/^[ \t]*import\s*['"][^'"]+['"]\s*;?[^\n]*$/gm, '')
    // 删 export ... from '...'（re-export），含行尾注释
    .replace(/^[ \t]*export\s+(?:\*|\{)[\s\S]*?from\s*['"][^'"]+['"]\s*;?[^\n]*$/gm, '')
    // 删 export 前缀，保留后面的声明本身
    .replace(/^[ \t]*export\s+(?=(?:default\s+)?(?:function|class|const|let|var|async))/gm, '')
    // 删 export { ... };（命名导出清单），含行尾注释
    .replace(/^[ \t]*export\s*\{[^}]*\}\s*;?[^\n]*$/gm, '');
}

const files = collect();
let bad = 0;
console.log('JS 语法自检：%d 个文件', files.length);

for (const f of files) {
  const rel = path.relative(ROOT, f).replace(/\\/g, '/');
  let src;
  try { src = fs.readFileSync(f, 'utf8'); }
  catch (e) { bad++; console.log('  \u2717 %s  读不到: %s', rel, e.message); continue; }

  try {
    new vm.Script(strip(src), { filename: rel });
    console.log('  \u2713 %s', rel);
  } catch (e) {
    bad++;
    console.log('  \u2717 %s  %s', rel, e.message.split('\n')[0]);
  }
}

console.log(bad ? '\n语法错误 %d 个 \u274c' : '\n语法全部通过 \u2705', bad || '');
process.exit(bad ? 1 : 0);
