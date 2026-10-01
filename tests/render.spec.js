/* ui/render.js —— 重绘调度（订阅式）（2026-10-01 · A-2 第 14 步）
 *
 * 它取代了「55 处各自直接调 render()」的散乱写法。测两件最要紧的事：
 *   ① **同步性** —— `markDirty()` 返回时回调**已经跑完**。
 *      调用方紧接着会去取刚渲染出来的元素（`$('#btnXxx')`），一旦改成异步 flush 就会拿到 null。
 *   ② **只跑标脏的那些** —— 这是"订阅式"的全部意义；不标脏就不该重绘。
 *
 * ⚠️ 直接把 ui/render.js 的真实源码丢进 vm 跑（只剥 export），不另写一份实现。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const RENDER_SRC = fs.readFileSync(path.join(ROOT, 'www/js/ui/render.js'), 'utf8');
const APP_RAW = fs.readFileSync(path.join(ROOT, 'www/js/app.js'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const SW_SRC = fs.readFileSync(path.join(ROOT, 'www/sw.js'), 'utf8');

const stripExport = s => s.replace(/^[ \t]*export\s+(?=(?:function|var|const|let))/gm, '');
/* ⚠️ **查源码一律先剥离注释**（项目纪律）。这里就踩了一次：
 * app.js 的注释里还留着「renderToday(); renderMeds(); renderRecords();」这句旧写法做对比，
 * 不剥注释就会假红 —— 明明代码已经删干净了。 */
const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP_CODE = stripJs(APP_RAW);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}

/* 每个用例一个干净 sandbox —— 模块内部有 subs/dirty/registered 状态，必须隔离 */
function makeEnv() {
  const sb = { console: { warn() {}, log() {}, error() {} } };
  vm.createContext(sb);
  vm.runInContext(stripExport(RENDER_SRC), sb);
  return sb;
}

console.log('');
console.log('=== A. 调度行为 ===');
{
  const sb = makeEnv();
  const calls = [];
  const mk = v => function () { calls.push(v); };

  t('注册前调 markDirty 不会抛错', (function () {
    try { sb.markDirty('today'); return true; } catch (e) { return false; }
  })(), '抛错了');
  t('★ 注册前标脏 → 不渲染（还没人可渲染）', calls.length === 0, '居然渲染了 ' + calls.length);

  sb.setRenderers({ today: mk('today'), meds: mk('meds'), records: mk('records') });
  t('★ 注册时把之前累积的标脏补跑掉（不丢）', calls.join(',') === 'today', '实际: ' + calls.join(','));

  calls.length = 0;
  sb.markDirty('today');
  t('★ markDirty(单个) 只渲染那一个', calls.join(',') === 'today', '实际: ' + calls.join(','));

  calls.length = 0;
  sb.markDirty('records');
  t('换个视图同样只渲染它', calls.join(',') === 'records', '实际: ' + calls.join(','));

  calls.length = 0;
  sb.markDirty();
  t('markDirty() 无参数 = 全部重绘', calls.sort().join(',') === 'meds,records,today', '实际: ' + calls.join(','));

  calls.length = 0;
  sb.markDirty('meds');
  const during = calls.slice();
  t('★ 同步：markDirty 返回时回调已跑完（不能改成 setTimeout）',
    during.join(',') === 'meds', '实际: ' + during.join(','));

  calls.length = 0;
  sb.markDirty('today'); sb.markDirty('today');
  t('同一视图标脏两次 → 各跑一次（不合并，但不会漏）', calls.join(',') === 'today,today', '实际: ' + calls.join(','));
}

console.log('');
console.log('=== B. 空视图名 / 边界 ===');
{
  const sb = makeEnv();
  const calls = [];
  sb.setRenderers({ today: () => calls.push('today') });

  sb.markDirty('不存在的视图');
  t('标脏一个没注册的视图 → 不崩也不渲染', calls.length === 0, '实际: ' + calls.join(','));

  sb.markDirty('today');
  t('之后正常视图照常渲染（脏标记没被污染）', calls.join(',') === 'today', '实际: ' + calls.join(','));
}

console.log('');
console.log('=== C. 源码级接线（防改回去）===');
{
  t('render.js 导出 setRenderers', /export function setRenderers\(/.test(RENDER_SRC), '缺');
  t('render.js 导出 markDirty', /export function markDirty\(/.test(RENDER_SRC), '缺');
  t('render.js **不依赖任何模块**（底层，谁都能 import）',
    !/^\s*import\s/.test(RENDER_SRC), '它 import 了东西 —— 会破坏依赖方向');

  t('★ app.js 已 import 调度器',
    /import\s*\{[^}]*markDirty[^}]*\}\s*from\s*'\.\/ui\/render\.js'/.test(APP_RAW), '没接上');
  t('★ app.js 注册了三个视图的渲染函数',
    /setRenderers\(\s*\{\s*today:\s*renderToday,\s*meds:\s*renderMeds,\s*records:\s*renderRecords\s*\}\s*\)/.test(APP_RAW),
    '没注册');
  t('★ render() 已变成 markDirty 的别名（旧调用点无需改）',
    /function render\(\)\s*\{\s*markDirty\(\);\s*\}/.test(APP_RAW), '还是老实现');
  t('★ 老的三连调用已移除（否则绕过调度器）',
    !/renderToday\(\);\s*renderMeds\(\);\s*renderRecords\(\);/.test(APP_CODE), '还在');

  t('index.html 声明了 render.js', HTML_SRC.indexOf('js/ui/render.js') >= 0, '缺');
  t('sw.js 预缓存含 render.js', SW_SRC.indexOf("'./js/ui/render.js'") >= 0, '缺');
  t('★ 改了 js 必须升 SW 缓存版本', /var CACHE = 'medreminder-v\d+'/.test(SW_SRC), '缺');
}

console.log('');
/* ⚠️ 汇总行必须是 `通过 N / 共 M` —— harness 按这个格式解析（见 harness/gates.py 第 11 行）。 */
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fails.length) {
  console.log('失败项：');
  fails.forEach(f => console.log('  - ' + f));
  process.exit(1);
}
