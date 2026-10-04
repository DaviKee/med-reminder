/* S-6 数据可视化 —— ui/viz.js 的 spec（2026-10-04）
 *
 * 测法：cut() 把单函数抽进 vm 沙箱跑真代码（与 history.spec 同一套路）。
 * 依赖注入：S（数据）、todayKey / nowMin / dueAt（可控时间）、MISS_GRACE_MIN。
 *
 * ⚠️ 口径断言是这里的重点：statsRange 与记录页 adherenceStats 必须**完全同口径**
 * （补记不算分子、漏服只算过宽限期的、未来 pending 不算漏）——
 * 两处口径一漂移，用户就不知道该信哪个数字。
 */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const APP = require('./sources').all();   // 全模块拼接（已剥 import/export）

let pass = 0, fail = 0;
const fails = [];
function t(name, ok, extra) {
  if (ok) { pass++; console.log('  ✓ ' + name); }
  else { fail++; fails.push(name); console.log('  ✗ ' + name + (extra ? ' —— ' + extra : '')); }
}
function eq(name, got, want) {
  t(name, JSON.stringify(got) === JSON.stringify(want),
    'got ' + JSON.stringify(got) + ' want ' + JSON.stringify(want));
}

function cut(src, header) {
  const i = src.indexOf(header);
  if (i < 0) return '';
  let depth = 0, j = src.indexOf('{', i);
  for (let k = j; k < src.length; k++) {
    if (src[k] === '{') depth++;
    else if (src[k] === '}') { depth--; if (!depth) return src.slice(i, k + 1); }
  }
  return '';
}

/* 沙箱：时间可控。NOW = nowMin() 的返回；TODAY = todayKey()；
 * DUE(x) = dueAt(x) 的返回（默认 0 = 早已过宽限期）。 */
function loadSb(opts) {
  const o = opts || {};
  const sb = {
    S: { doses: o.doses || {} },
    MISS_GRACE_MIN: 30,
    TODAY: o.today || '2026-10-05',
    NOW: o.now != null ? o.now : 800,               // 800 = 13:20
    console: console
  };
  vm.createContext(sb);
  vm.runInContext([
    'function todayKey() { return TODAY; }',
    'function nowMin() { return NOW; }',
    'function dueAt(x) { return (x && typeof x.due === "number") ? x.due : 0; }',
    /* todayParts 是 viz.js 的模块私有函数，dayStatus 间接依赖它 —— 沙箱要一并抽
     * （跨模块的 import 已注入；同模块私有函数只能靠 cut 一起带走）。 */
    cut(APP, 'function todayParts'),
    cut(APP, 'export function statsRange') || cut(APP, 'function statsRange'),
    cut(APP, 'export function dayStatus') || cut(APP, 'function dayStatus'),
    cut(APP, 'export function missedBuckets') || cut(APP, 'function missedBuckets'),
    cut(APP, 'export function devStats') || cut(APP, 'function devStats')
  ].join('\n'), sb);
  return sb;
}

console.log('=== 0. 自检 ===');
t('能抽到 statsRange', cut(APP, 'function statsRange(').length > 200, '改名了？');
t('能抽到 missedBuckets', cut(APP, 'function missedBuckets(').length > 200, '');
t('能抽到 devStats', cut(APP, 'function devStats(').length > 200, '');
t('能抽到 dayStatus', cut(APP, 'function dayStatus(').length > 100, '');
t('★ keyToMs 已内联进 devStats（不再有模块级私有依赖 —— 工程纪律第 5 条）',
  !/^function keyToMs/m.test(fs.readFileSync(path.join(__dirname, '../www/js/ui/viz.js'), 'utf8')), '还在');

console.log('=== 1. statsRange：口径 ===');
(function () {
  const sb = loadSb({
    today: '2026-10-05', now: 800,
    doses: {
      '2026-10-05': [                                    // 今天
        { medId: 'a', status: 'taken', takenAt: 1 },                        // 真实
        { medId: 'a', status: 'taken', makeup: 1, takenAt: 1 },             // 补记
        { medId: 'a', status: 'skipped' },                                  // 跳过
        { medId: 'a', status: 'pending', due: 700 },                        // 今天、晚 100 分钟 > 宽限 30 → 漏
        { medId: 'a', status: 'pending', due: 900 }                         // 未来的 → 不算漏
      ],
      '2026-09-30': [{ medId: 'a', status: 'pending' }]  // 上月（前缀过滤掉）
    }
  });
  const st = vm.runInContext('statsRange("2026-10", null)', sb);
  eq('真实打卡 = 1', st.real, 1);
  eq('补记 = 1', st.makeup, 1);
  eq('跳过 = 1', st.skipped, 1);
  eq('漏服 = 1（过宽限的今天 + 未来不算）', st.missed, 1);
  eq('分母 = 4（漏服计入分母）', st.total, 4);
  eq('依从率 = 25%（分子只有真实打卡）', st.rate, 25);
  t('按天细分存在且今天 total=4', st.days['2026-10-05'] && st.days['2026-10-05'].total === 4, '');
  t('★ 上月数据被前缀过滤', !st.days['2026-09-30'], '');
})();

(function () {
  const sb = loadSb({ doses: {} });
  const st = vm.runInContext('statsRange("2026-10", null)', sb);
  t('空月：rate 为 null（不是 0 —— 没数据 ≠ 全漏）', st.rate === null, String(st.rate));
})();

console.log('=== 2. dayStatus：状态分支 ===');
(function () {
  const sb = loadSb({ today: '2026-10-05' });
  const D = vm.runInContext('dayStatus', sb);
  const FUT = '2026-10-31';
  eq('未来天 → future', D({ total: 0 }, FUT, false), 'future');
  eq('没排程 → none', D({ total: 0 }, '2026-10-03', false), 'none');
  eq('全跳过（无漏）→ good', D({ total: 2, skipped: 2 }, '2026-10-03', false), 'good');
  eq('全服 → good', D({ total: 3, real: 3 }, '2026-10-03', false), 'good');
  eq('有吃有漏 → partial', D({ total: 3, real: 2, missed: 1 }, '2026-10-03', false), 'partial');
  eq('全漏 → bad', D({ total: 2, missed: 2 }, '2026-10-03', false), 'bad');
  eq('★ 今天还有 pending → part（进行中，不和已完成一个样）',
    D({ total: 1, real: 1, pending: 1 }, '2026-10-05', true), 'part');
})();

console.log('=== 3. missedBuckets：分桶边界 ===');
(function () {
  const sb = loadSb({
    today: '2026-10-05', now: 1440,
    doses: {
      '2026-10-03': [
        { medId: 'a', status: 'pending', time: 539 },    // 清晨（<540）
        { medId: 'a', status: 'pending', time: 540 },    // 上午（边界含 540）
        { medId: 'a', status: 'pending', time: 719 },    // 上午
        { medId: 'a', status: 'pending', time: 720 },    // 下午
        { medId: 'a', status: 'pending', time: 1080 },   // 晚上
        { medId: 'a', status: 'taken', time: 600 }       // 打卡的不算漏
      ]
    }
  });
  const b = vm.runInContext('missedBuckets("2026-10", null)', sb);
  eq('清晨 = 1', b[0].count, 1);
  eq('上午 = 2（边界 540/720 归下一段）', b[1].count, 2);
  eq('下午 = 1', b[2].count, 1);
  eq('晚上 = 1', b[3].count, 1);
})();

console.log('=== 4. devStats：偏差 ===');
(function () {
  // 2026-10-03 00:00 本地 = ?ms：用沙箱里的同款公式算期望值
  const sb = loadSb({
    today: '2026-10-05', now: 800,
    doses: {
      '2026-10-03': [
        { medId: 'a', status: 'taken', time: 480, takenAt: null },     // 无时间戳 → 跳过
        { medId: 'a', status: 'taken', time: 480, takenAt: new Date(2026, 9, 3).getTime() + 480 * 60000 + 5 * 60000 },   // +5 分 → 按时
        { medId: 'a', status: 'taken', time: 720, takenAt: new Date(2026, 9, 3).getTime() + 720 * 60000 + 30 * 60000 },  // +30 → 迟到
        { medId: 'a', status: 'taken', time: 900, takenAt: new Date(2026, 9, 3).getTime() + 900 * 60000 + 95 * 60000 },  // +95 → 晚很多
        { medId: 'a', status: 'taken', time: 1200, makeup: 1, takenAt: new Date(2026, 9, 3).getTime() + 1300 * 60000 },  // 补记 → 不算
        { medId: 'a', status: 'taken', time: 1300, takenAt: new Date(2026, 9, 3).getTime() + 1300 * 60000 - 40 * 60000 } // 提前 40 分 → 按 0 算，按时
      ]
    }
  });
  const dv = vm.runInContext('devStats("2026-10", null)', sb);
  eq('count = 4（无时间戳与补记都排除）', dv.count, 4);
  eq('按时 = 2（+5 分的 + 提前 40 分按 0 算的）', dv.ontime, 2);
  eq('迟到 = 1', dv.late, 1);
  eq('晚很多 = 1', dv.verylate, 1);
  eq('平均晚 = 63 分钟（(30+95)/2 四舍五入）', dv.lateAvg, 63);
})();

(function () {
  const sb = loadSb({ doses: { '2026-10-03': [{ medId: 'a', status: 'missed', time: 480 }] } });
  const dv = vm.runInContext('devStats("2026-10", null)', sb);
  eq('没有真实打卡 → count=0', dv.count, 0);
  eq('lateAvg 为 null（不是 0）', dv.lateAvg, null);
})();

console.log('=== 5. vizHtml：结构（源码级）===');
(function () {
  const src = fs.readFileSync(path.join(__dirname, '../www/js/ui/viz.js'), 'utf8');
  t('有月历网格 .cal 与星期头（周一起始）',
    /cal-row cal-head/.test(src) && src.indexOf("'一'") < src.indexOf("'日'"), '');
  t('有图例（颜色语义当场说清）',
    src.indexOf('cal-legend') > 0 && /全服/.test(src) && /有漏/.test(src) && /全漏/.test(src), '');
  t('有翻月按钮 data-viz-nav', /data-viz-nav="-1"/.test(src) && /data-viz-nav="1"/.test(src), '');
  t('★ 下月按钮可禁用（不许翻到未来）', /disabled/.test(src) && /不许|未来/.test(src), '');
  t('漏服时段按计划时刻分桶', /计划时刻/.test(src) && /清晨|上午|下午|晚上/.test(src), '');
  t('偏差文案解释「按时」的口径', /±10 分钟/.test(src), '');
  t('晚很多时给出可行动的提示', /间隔设得太紧/.test(src), '');
})();

console.log('=== 6. 接线 ===');
(function () {
  const rec = fs.readFileSync(path.join(__dirname, '../www/js/ui/records.js'), 'utf8');
  const app = fs.readFileSync(path.join(__dirname, '../www/js/app.js'), 'utf8');
  const idx = fs.readFileSync(path.join(__dirname, '../www/index.html'), 'utf8');
  const sw = fs.readFileSync(path.join(__dirname, '../www/sw.js'), 'utf8');
  const vap = fs.readFileSync(path.join(__dirname, '../tools/verify-apk.py'), 'utf8');
  t('records.js 引入并嵌入 vizHtml', /import \{ statsRange, vizHtml \} from '\.\/viz\.js'/.test(rec) && /vizHtml\(\)/.test(rec), '');
  t('★ adherenceStats 已委托 statsRange（口径唯一实现）',
    /return statsRange\(prefix, medId\);/.test(rec), '');
  t('app.js 注册 bindViz（document 委托只注册一次）',
    /import \{ bindViz \} from '\.\/ui\/viz\.js'/.test(app) && /bindViz\(\)/.test(app), '');
  t('index.html 声明 modulepreload', /modulepreload" href="js\/ui\/viz\.js"/.test(idx), '');
  t('sw.js 预缓存', /'\.\/js\/ui\/viz\.js'/.test(sw), '');
  t('verify-apk.py 清单含 viz.js', /js\/ui\/viz\.js/.test(vap), '');
})();

console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) { console.log('失败清单:\n  - ' + fails.join('\n  - ')); process.exit(1); }
