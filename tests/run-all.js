/* 一键跑完全部用例：`npm test`（等价于 `node tests/run-all.js`）
 *
 * 为什么需要它：4 个套件此前只能手工逐个 `node tests/xxx.spec.js`，
 * 实际结果就是「改完不跑」或「只跑最近动过的那一个」—— 回归防护形同虚设。
 *
 * 设计取舍：
 *   · 用 `process.execPath` 起子进程 —— 不依赖 PATH 里有没有 node（本机 PATH 常是坏的）；
 *   · 顺序跑、不并行 —— 输出可读，失败时一眼定位是哪个套件；
 *   · 断言总数由各套件自己打印的「通过 N / 共 M」汇总，**不另维护一份总数**
 *     （同一个数字写在两处，迟早会不同步）；
 *   · 退出码：全部通过 = 0；有断言失败 = 1；套件/环境出错 = 2。三者含义不同，别混。
 *   · `--selftest` 自检汇总逻辑 —— 因为**汇总算错比用例失败更危险**（会谎报"全绿"）。
 *
 * ⚠️ 已知环境限制：某些受限沙箱（如 WorkBuddy 内置 shell）会**禁止 Node 创建任何子进程**，
 *    `spawnSync` 一律返回 `EBUSY`。那种情况下本脚本会打印明确提示并退出 2，
 *    不会伪装成"用例失败"。此时请在**普通终端**里跑，或逐个执行 `node tests/xxx.spec.js`。
 */
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const DIR = __dirname;
const SUMMARY = /通过\s*(\d+)\s*\/\s*共\s*(\d+)/;

/* 从套件输出里抠出「通过 N / 共 M」。抠不到 → null（视为套件自身出错）。 */
function parseSummary(out) {
  const m = SUMMARY.exec(out || '');
  return m ? { pass: +m[1], total: +m[2] } : null;
}

/* 汇总：返回每套件的状态 + 总计 + 退出码。纯函数，便于自检。 */
function aggregate(rows) {
  let pass = 0, total = 0, crashed = 0;
  const detail = rows.map(r => {
    if (!r.sum) { crashed++; return Object.assign({}, r, { state: 'CRASH', fail: 0 }); }
    pass += r.sum.pass;
    total += r.sum.total;
    const fail = r.sum.total - r.sum.pass;
    return Object.assign({}, r, { state: fail ? 'FAIL' : 'ok', fail });
  });
  const fail = total - pass;
  return { detail, pass, total, fail, crashed, code: crashed ? 2 : (fail ? 1 : 0) };
}

/* ---------------- 自检：不依赖子进程也能验证汇总逻辑 ---------------- */
function selftest() {
  const cases = [
    ['全绿', [{ file: 'a', sum: { pass: 10, total: 10 } }, { file: 'b', sum: { pass: 5, total: 5 } }], 0, 15, 0],
    ['有失败', [{ file: 'a', sum: { pass: 9, total: 10 } }], 1, 9, 1],
    ['套件出错优先于失败', [{ file: 'a', sum: { pass: 9, total: 10 } }, { file: 'b', sum: null }], 2, 9, 1],
    ['全零也不炸', [{ file: 'a', sum: { pass: 0, total: 0 } }], 0, 0, 0],
    ['汇总行乱序/无空格', [{ file: 'a', sum: parseSummary('通过1/共2') }], 1, 1, 1],
    ['中文与空格混杂', [{ file: 'a', sum: parseSummary('  通过  7  /  共  8  \n') }], 1, 7, 1],
    ['抠不到汇总', [{ file: 'a', sum: parseSummary('完全无关的输出') }], 2, 0, 0],
    ['空输出', [{ file: 'a', sum: parseSummary('') }], 2, 0, 0],
  ];
  let bad = 0;
  console.log('自检：汇总逻辑（%d 个用例）', cases.length);
  for (const [name, rows, wantCode, wantPass, wantFail] of cases) {
    const r = aggregate(rows);
    const ok = r.code === wantCode && r.pass === wantPass && r.fail === wantFail;
    if (!ok) bad++;
    console.log('  %s %s  code=%d(期望%d) pass=%d(期望%d) fail=%d(期望%d)',
      ok ? '✓' : '✗', name, r.code, wantCode, r.pass, wantPass, r.fail, wantFail);
  }
  console.log(bad ? '\n自检失败 ' + bad + ' 个 ❌' : '\n自检全部通过 ✅');
  process.exit(bad ? 1 : 0);
}

if (process.argv.indexOf('--selftest') >= 0) selftest();

/* ---------------- 主流程 ---------------- */
const files = fs.readdirSync(DIR).filter(f => f.endsWith('.spec.js')).sort();
if (!files.length) {
  console.error('没找到任何 tests/*.spec.js');
  process.exit(2);
}

const rows = [];
let spawnBroken = null;

for (const f of files) {
  console.log('\n' + '─'.repeat(64));
  console.log('▶ ' + f);
  console.log('─'.repeat(64));

  const r = spawnSync(process.execPath, [path.join(DIR, f)], { encoding: 'utf8' });
  if (r.error) spawnBroken = r.error;
  if (r.stdout) process.stdout.write(r.stdout);
  if (r.stderr) process.stderr.write(r.stderr);

  const sum = spawnBroken ? null : parseSummary(r.stdout);
  if (!sum && !spawnBroken) {
    console.log('!! ' + f + ' 未打印汇总行（退出码 ' + r.status + '）—— 视为套件自身出错');
  }
  rows.push({ file: f, sum, rc: r.status });
}

const res = aggregate(rows);

console.log('\n' + '='.repeat(64));
console.log('汇总');
console.log('='.repeat(64));

if (spawnBroken) {
  console.log('  ⚠️ 无法创建子进程：' + spawnBroken.code + ' — ' + spawnBroken.message.split('\n')[0]);
  console.log('     这是**运行环境限制**，不是用例失败（未跑任何断言）。');
  console.log('     请在**普通终端**重新执行：npm test');
  console.log('     或逐个执行：');
  for (const f of files) console.log('        node tests/' + f);
  process.exit(2);
}

for (const r of res.detail) {
  const mark = r.state === 'ok' ? '✓' : (r.state === 'FAIL' ? '✗' : '‼');
  const tail = r.state === 'CRASH' ? '套件出错（rc=' + r.rc + '）'
                                   : (r.sum.pass + ' / ' + r.sum.total);
  console.log('  ' + mark + '  ' + r.file.padEnd(22) + ' ' + tail);
}
console.log('  ' + '-'.repeat(52));
console.log('  套件 ' + files.length + ' 个 ｜ 断言 ' + res.pass + ' / ' + res.total +
            (res.crashed ? '  ｜ ' + res.crashed + ' 个套件出错' : ''));

console.log(res.code === 0 ? '\n全部通过 ✅' : '\n有失败 ❌（退出码 ' + res.code + '）');
process.exit(res.code);
