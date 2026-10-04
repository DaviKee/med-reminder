/* S-9 桌面卡片的数据摘要（core/cardSummary.js）—— 2026-10-04
 *
 * 测两件事（**只测 cardSummary 自己的逻辑**，依赖用桩注入：
 * schedule/util 那些函数的正确性由它们各自的 spec 保证）：
 *
 *   ① `computeSummary` —— 字段装配与**字符串化**边界。
 *      卡片侧（ArkTS）对类型敏感，且要能区分「没有下一次」与「值是 0」。
 *   ② `writeCardFile` —— 无插件跳过 / 有插件**先 mkdir 再 writeFile**（顺序不能反）、
 *      路径与 directory 必须对。这条路径挂在每次 save() 上，写错会连累主流程。
 *
 * ⚠️ 项目踩过的坑：`writeFile` 不会自动建父目录 → 不 mkdir 就写会 reject。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const CARD_SRC = fs.readFileSync(path.join(ROOT, 'www/js/core/cardSummary.js'), 'utf8');

const strip = s => s
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^\s*\/\/.*$/gm, '');
const CARD_RAW = CARD_SRC;
const CARD = strip(CARD_SRC);

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

/* 从真源码里抠出这两个常量值 —— **不手写**，否则源码改了 spec 还照样"过"。 */
const CARD_DIR_VAL = (CARD.match(/var CARD_DIR = '([^']+)'/) || [])[1];
const CARD_PATH_VAL = (CARD.match(/var CARD_PATH = '([^']+)'/) || [])[1];

/* 造沙箱：真源码 + 桩依赖。data 里放本次要模拟的状态。 */
function load(data, cap) {
  const sb = { console: console };
  if (cap) sb.window = { Capacitor: { Plugins: { Filesystem: cap } } };
  else sb.window = {};                       // 纯浏览器：没有插件
  vm.createContext(sb);
  vm.runInContext([
    'var CARD_DIR = ' + JSON.stringify(CARD_DIR_VAL) + ';',
    'var CARD_PATH = ' + JSON.stringify(CARD_PATH_VAL) + ';',
    'var DONE = ' + (data.done || 0) + ';',
    'var TOTAL = ' + (data.total || 0) + ';',
    'var NEXT = ' + JSON.stringify(data.next || null) + ';',
    'var MEDNAME = ' + JSON.stringify(data.medName || '') + ';',
    'function progress() { return { done: DONE, total: TOTAL }; }',
    'function nextPending() { return NEXT; }',
    'function medById(id) { return NEXT ? { id: id, name: MEDNAME } : null; }',
    'function dueAt(d) { return d.snoozeUntil != null ? d.snoozeUntil : d.time; }',
    'function minToStr(m) { return ("0" + Math.floor(m / 60)).slice(-2) + ":" + ("0" + (m % 60)).slice(-2); }',
    'function todayKey() { return "2026-10-04"; }',
    cut(CARD, 'function computeSummary()'),
    cut(CARD, 'function writeCardFile()'),
  ].join('\n'), sb);
  return sb;
}

(async function main() {
  console.log('=== 0. 自检 ===');
  t('能抽到 computeSummary', cut(CARD, 'function computeSummary()').length > 150, '函数改名了？');
  t('能抽到 writeCardFile', cut(CARD, 'function writeCardFile()').length > 250, '函数改名了？');
  t('★ 抠到了 CARD_DIR / CARD_PATH 两个常量（没抠到说明源码改了写法）',
    !!CARD_DIR_VAL && !!CARD_PATH_VAL, CARD_DIR_VAL + ' / ' + CARD_PATH_VAL);

  console.log('');
  console.log('=== 1. computeSummary：字段装配（跑真代码）===');
  {
    const s = load({ done: 2, total: 5, next: { id: 'd1', time: 1230 }, medName: '阿莫西林' });
    const r = s.computeSummary();
    eq('date = 今天', r.date, '2026-10-04');
    eq('nextTime 按分钟转 HH:MM', r.nextTime, '20:30');
    eq('nextMed = 药名', r.nextMed, '阿莫西林');
    eq('done / total', [r.done, r.total], ['2', '5']);
    t('★ 全字段都是字符串（卡片侧对类型敏感）',
      Object.keys(r).every(k => typeof r[k] === 'string'),
      Object.keys(r).map(k => k + ':' + typeof r[k]).join(','));
  }

  console.log('');
  console.log('=== 2. computeSummary：边界 ===');
  {
    const s = load({ done: 0, total: 0, next: null });
    const r = s.computeSummary();
    eq('★ 没有下一次 → 两个字段都是空字符串（不是 null / undefined）',
      [r.nextTime, r.nextMed], ['', '']);
    eq('没有药时进度是 0/0（卡片据此显示"今天没药"）', [r.done, r.total], ['0', '0']);
  }
  {
    /* 全吃完了：total>0 但 next 为 null —— 与"完全没药"要用同一表现，
     * 卡片侧靠 done/total 自己区分。 */
    const s = load({ done: 3, total: 3, next: null });
    const r = s.computeSummary();
    eq('★ 全吃完 → nextTime 空，但进度保留 3/3', [r.nextTime, r.done, r.total], ['', '3', '3']);
  }
  {
    /* 延后过的剂量：nextTime 必须显示**延后时刻**，
     * 否则卡片会显示一个已经过去的时间（与 nextPending 的排序口径一致）。 */
    const s = load({ done: 0, total: 2, next: { id: 'd1', time: 480, snoozeUntil: 500 }, medName: '二甲双胍' });
    const r = s.computeSummary();
    eq('★ 延后过的按延后时刻显示（不是原计划时刻）', r.nextTime, '08:20');
  }
  {
    /* 午夜边界：00:00 = 0 分钟，不能被当成"没有"（falsy 陷阱）。 */
    const s = load({ done: 0, total: 1, next: { id: 'd1', time: 0 }, medName: 'X' });
    eq('★ 00:00 显示成 00:00（0 不能被当成空）', s.computeSummary().nextTime, '00:00');
  }

  console.log('');
  console.log('=== 3. writeCardFile：无插件必须**静默跳过** ===');
  {
    const s = load({ done: 1, total: 2, next: null });
    const r = await s.writeCardFile();
    eq('纯浏览器（无插件）→ 返回 null，不抛错', r, null);
  }

  console.log('');
  console.log('=== 4. writeCardFile：有插件时的调用序列 ===');
  {
    const rec = [];
    const cap = {
      mkdir: function (o) { rec.push(['mkdir', o]); return Promise.resolve(); },
      writeFile: function (o) { rec.push(['writeFile', o]); return Promise.resolve(); }
    };
    const s = load({ done: 3, total: 4, next: { id: 'd1', time: 1200 }, medName: '阿莫西林' }, cap);
    await s.writeCardFile();

    eq('★ 先 mkdir 再 writeFile（顺序不能反 —— writeFile 不建父目录）',
      rec.map(x => x[0]), ['mkdir', 'writeFile']);
    eq('mkdir 参数：card / DATA / recursive',
      [rec[0][1].path, rec[0][1].directory, rec[0][1].recursive], ['card', 'DATA', true]);
    eq('writeFile 路径：card/card_state.json，写进 DATA 沙箱',
      [rec[1][1].path, rec[1][1].directory], ['card/card_state.json', 'DATA']);
    t('writeFile 明确 utf8（中文药名不能乱码）', rec[1][1].encoding === 'utf8', rec[1][1].encoding);
    let payload = null;
    try { payload = JSON.parse(rec[1][1].data); } catch (e) { /* 下面断言会报 */ }
    t('★ 落盘内容是合法 JSON 且有五个字段',
      payload && ['date', 'nextTime', 'nextMed', 'done', 'total'].every(k => k in payload),
      JSON.stringify(payload));
    t('落盘内容与 computeSummary 一致（不是另算一份）',
      payload && payload.nextTime === '20:00' && payload.nextMed === '阿莫西林' && payload.total === '4',
      JSON.stringify(payload));
  }

  console.log('');
  console.log('=== 5. writeCardFile：mkdir 失败（目录已存在）也要继续写 ===');
  {
    const rec = [];
    const cap = {
      mkdir: function (o) { rec.push(['mkdir', o]); return Promise.reject(new Error('Directory exists')); },
      writeFile: function (o) { rec.push(['writeFile', o]); return Promise.resolve(); }
    };
    const s = load({ done: 0, total: 1, next: null }, cap);
    await s.writeCardFile();
    eq('★ mkdir 抛错（已存在）不该阻断 writeFile',
      rec.map(x => x[0]), ['mkdir', 'writeFile']);
  }

  console.log('');
  console.log('=== 6. writeCardFile：writeFile 失败必须被吞掉 ===');
  {
    const cap = {
      mkdir: function () { return Promise.resolve(); },
      writeFile: function () { return Promise.reject(new Error('disk full')); }
    };
    const s = load({ done: 0, total: 1, next: null }, cap);
    let threw = false;
    try { await s.writeCardFile(); } catch (e) { threw = true; }
    t('★ writeFile 失败不往上抛（它挂在每次 save() 上，抛了会中断用户操作）',
      !threw, '抛出来了');
  }

  console.log('');
  console.log('=== 7. 接线（源码级）===');
  const STORE = fs.readFileSync(path.join(ROOT, 'www/js/core/store.js'), 'utf8');
  const APP = fs.readFileSync(path.join(ROOT, 'www/js/app.js'), 'utf8');
  t('★ store.js 的 saveHooks 有 card 槽', /card:\s*function\s*\(\)\s*\{\}/.test(STORE), '缺');
  t('★ setSaveHooks 认得 card', /if\s*\(h\.card\)\s*saveHooks\.card\s*=\s*h\.card/.test(STORE), '缺');
  t('★ save() 末尾调了 saveHooks.card()', /saveHooks\.card\(\);\s*\n\}/.test(STORE), '缺');
  t('★ app.js 把 writeCardFile 注册进钩子', /card:\s*writeCardFile/.test(APP), '没接上');
  t('cardSummary 不 import 上层（只依赖 core）',
    !/from\s+'\.\.\//.test(CARD_RAW.replace(/from\s+'\.\.\/core\//g, '')), '反向依赖了');

  console.log('');
  console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
  if (fail) {
    console.log('  失败清单：');
    fails.forEach(f => console.log('    - ' + f));
  }
  process.exit(fail ? 1 : 0);
})();
