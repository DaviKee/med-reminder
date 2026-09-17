/* 固定时刻排程（F-4）
 *
 * 需求：「每天固定几点吃」—— 如早 8 点、晚 8 点，**到点就提醒，不随打卡时间顺延**。
 * 这是第二种排程模型，与原有的「打卡后按间隔滚动」并存：
 *   · med.mode = 'interval'（默认，字段缺省即此）：打卡后生成，会顺延
 *   · med.mode = 'fixed'：**不等打卡**就预生成当天时刻，不顺延
 *
 * 桩一律照项目自己的工具函数语义来（todayKey / todayDoses / minOfDay 等抽真实源码）。
 * 查源码先剥离注释 —— 注释里常引用旧写法做对比说明。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = fs.readFileSync(path.join(ROOT, 'www/js/app.js'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const CSS_SRC = fs.readFileSync(path.join(ROOT, 'www/css/app.css'), 'utf8');

const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = stripJs(APP_SRC);
const CSS = stripJs(CSS_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}
const eq = (name, a, b) => t(name, JSON.stringify(a) === JSON.stringify(b), JSON.stringify(a) + ' !== ' + JSON.stringify(b));

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

const HEADERS = [
  'var uid = function ()',
  'function fmtDate(d)', 'function todayKey()', 'function todayDoses()', 'function medById(id)',
  'function minOfDay(ms)', 'function intervalLabel(v)', 'function minToStr(m)',
  'function medMode(m)', 'function normTimes(arr)', 'function timeInputToMin(v)',
  'function medScheduleLabel(m)', 'function reindexMed(medId)',
  'function ensureFixedDoses()', 'function rebuildTodayDoses(m)',
  'function rollForward(dose, takenMs)', 'function markTaken(dose, takenMs)'
];
function extract(header) {
  if (header.indexOf('var uid') === 0) {
    /* `var uid = function () { ... };` —— 不能用「找第一个分号」截断：
     * 函数体里就有分号，会把结尾的 `};` 切掉，沙箱直接语法报错。
     * （这就是我第一版犯的错。）改用花括号配对。 */
    const i = APP_SRC.indexOf(header);
    const b = APP_SRC.indexOf('{', i);
    let d = 0, j = b;
    while (j < APP_SRC.length) {
      if (APP_SRC[j] === '{') d++;
      else if (APP_SRC[j] === '}') { d--; if (d === 0) break; }
      j++;
    }
    return APP_SRC.slice(i, j + 1) + ';';
  }
  return cut(APP_SRC, header);
}
const BLOCKS = HEADERS.map(extract).join('\n\n') + '\n\nvar MAX_TIMES = 12;';

const TODAY = '2026-09-17';
const RealDate = Date;

function makeEnv(state) {
  const cancelled = [];
  const dropped = [];
  let saveCalls = 0;
  const NOW = new RealDate('2026-09-17T12:00:00');
  function FakeDate() {
    /* 无参 → 固定到 NOW（让 todayKey 可预期）；
     * 有参 → 真的按参数构造（minOfDay(ms) 之类要按传入的毫秒算）。
     * 注意别写成 `new RealDate.apply(...)` —— 那会被解析成
     * `new (RealDate.apply)(...)`，而 apply 不是构造函数，直接 TypeError。 */
    if (arguments.length === 0) return new RealDate(NOW);
    return Reflect.construct(RealDate, arguments);
  }
  FakeDate.now = () => NOW.getTime();

  state.notified = state.notified || {};
  if (!state.doses) state.doses = {};

  const sandbox = {
    console: { warn() {}, log() {}, error() {} },
    Date: FakeDate, Math, JSON, Object, Array, String, Number, Boolean,
    isFinite, parseInt, parseFloat,
    pad: n => String(n).padStart(2, '0'),
    S: state
  };
  sandbox.window = sandbox;
  sandbox.window.MedNotify = { cancelOne: id => { cancelled.push(id); } };
  // noteDropped 不在被测范围内，用桩并记录调用 —— 固定模式**不该**产生跨天丢弃
  sandbox.noteDropped = n => { dropped.push(n); };
  // save() 是持久化入口，这里只需要它存在（ensureFixedDoses 里会调）
  sandbox.save = () => { saveCalls++; };
  vm.createContext(sandbox);
  vm.runInContext(BLOCKS, sandbox);
  return { sandbox, S: state, cancelled, dropped, saveCalls: () => saveCalls };
}
const at = (h, mi, day) => new RealDate(day || '2026-09-17T00:00:00').setHours(h, mi || 0, 0, 0);

console.log('=== A. 工具函数 ===');
{
  const env = makeEnv({ meds: [], doses: {} });
  const S = env.sandbox;

  console.log('  -- medMode --');
  t('缺省（旧数据）视为 interval', S.medMode({ id: 'x' }) === 'interval', S.medMode({ id: 'x' }));
  t('显式 fixed', S.medMode({ mode: 'fixed' }) === 'fixed', S.medMode({ mode: 'fixed' }));
  t('显式 interval', S.medMode({ mode: 'interval' }) === 'interval', S.medMode({ mode: 'interval' }));
  t('大小写不对当 interval（不猜）', S.medMode({ mode: 'FIXED' }) === 'interval', S.medMode({ mode: 'FIXED' }));
  t('null / undefined 安全', S.medMode(null) === 'interval' && S.medMode(undefined) === 'interval', '崩了');

  console.log('  -- normTimes --');
  eq('升序', S.normTimes([1200, 480]), [480, 1200]);
  eq('去重', S.normTimes([480, 480, 480]), [480]);
  eq('钳到 0..1439', S.normTimes([-10, 2000]), [0, 1439]);
  eq('取整（480.4 → 480）', S.normTimes(['480', 480.4]), [480]);
  eq('非数字一律丢弃', S.normTimes([NaN, 'x', 480]), [480]);
  // ★ null / undefined / 空串**不能**变成 0 点：Number(null) === 0，会把「没填」
  //   静默当成「半夜提醒」。这一条是跑测试时发现的真实缺陷，已修源码。
  eq('★ null / undefined / 空串不会被当成 0 点', S.normTimes([null, undefined, '', 480]), [480]);
  eq('空 / 非法输入 → 空数组', S.normTimes([]), []);
  eq('null 安全', S.normTimes(null), []);
  eq('个数上限 12', S.normTimes([0, 60, 120, 180, 240, 300, 360, 420, 480, 540, 600, 660, 720, 780]).length, 12);

  console.log('  -- timeInputToMin --');
  t('08:00 → 480', S.timeInputToMin('08:00') === 480, S.timeInputToMin('08:00'));
  t('8:05 → 485（允许不补零）', S.timeInputToMin('8:05') === 485, S.timeInputToMin('8:05'));
  t('23:59 → 1439', S.timeInputToMin('23:59') === 1439, S.timeInputToMin('23:59'));
  t('24:00 → null', S.timeInputToMin('24:00') === null, S.timeInputToMin('24:00'));
  t('08:60 → null', S.timeInputToMin('08:60') === null, S.timeInputToMin('08:60'));
  t('空 / null / 乱码 → null（不当成 0 点）',
    S.timeInputToMin('') === null && S.timeInputToMin(null) === null && S.timeInputToMin('abc') === null, '有返回值');
  t('带空格也认', S.timeInputToMin(' 08:00 ') === 480, S.timeInputToMin(' 08:00 '));

  console.log('  -- medScheduleLabel --');
  t('interval → 每 N 小时', S.medScheduleLabel({ interval: 8 }) === '每 8 小时', S.medScheduleLabel({ interval: 8 }));
  t('interval 0.5 → 30 分钟', S.medScheduleLabel({ interval: 0.5 }) === '每 30 分钟', S.medScheduleLabel({ interval: 0.5 }));
  t('fixed → 每天 HH:MM / HH:MM',
    S.medScheduleLabel({ mode: 'fixed', times: [480, 1200], interval: 8 }) === '每天 08:00 / 20:00',
    S.medScheduleLabel({ mode: 'fixed', times: [480, 1200], interval: 8 }));
  t('fixed 但没时刻 → 未设时刻', S.medScheduleLabel({ mode: 'fixed', times: [], interval: 8 }) === '未设时刻',
    S.medScheduleLabel({ mode: 'fixed', times: [], interval: 8 }));
}

console.log('');
console.log('=== B. 固定时刻预生成（不等打卡）===');
{
  const env = makeEnv({
    meds: [{ id: 'm1', name: '降压药', interval: 8, mode: 'fixed', times: [480, 1200] }],
    doses: {}
  });
  const S = env.sandbox;
  env.sandbox.ensureFixedDoses();
  const list = env.S.doses[TODAY] || [];
  t('★ 生成了 2 条（08:00 / 20:00）', list.length === 2, JSON.stringify(list.map(d => d.time)));
  eq('时刻正确', list.map(d => d.time).sort((a, b) => a - b), [480, 1200]);
  t('状态都是 pending（还没吃）', list.every(d => d.status === 'pending'), JSON.stringify(list.map(d => d.status)));
  t('序号已排好', list.slice().sort((a, b) => a.time - b.time).every((d, i) => d.idx === i && d.total === 2),
    JSON.stringify(list.map(d => [d.idx, d.total])));

  console.log('  -- 幂等 --');
  env.sandbox.ensureFixedDoses();
  env.sandbox.ensureFixedDoses();
  t('★ 反复调用不会重复生成', (env.S.doses[TODAY] || []).length === 2, (env.S.doses[TODAY] || []).length);
}

console.log('');
console.log('=== C. 与其他情况共存 ===');
{
  // C-1 间隔模式的药不受影响（不该被预生成）
  const e1 = makeEnv({
    meds: [
      { id: 'f1', name: 'F', interval: 8, mode: 'fixed', times: [480] },
      { id: 'i1', name: 'I', interval: 8 }
    ],
    doses: {}
  });
  e1.sandbox.ensureFixedDoses();
  const l1 = e1.S.doses[TODAY] || [];
  t('★ 只给固定模式排，间隔模式不排', l1.length === 1 && l1[0].medId === 'f1',
    JSON.stringify(l1.map(d => d.medId)));

  // C-2 没设时刻 → 不排（药品卡会提示去设置）
  const e2 = makeEnv({ meds: [{ id: 'f2', mode: 'fixed', times: [], interval: 8 }], doses: {} });
  e2.sandbox.ensureFixedDoses();
  t('★ 没设时刻就不排', (e2.S.doses[TODAY] || []).length === 0, (e2.S.doses[TODAY] || []).length);

  // C-3 已有该药的剂量（比如用户已打过卡）→ 不重复生成，只理顺序号
  const e3 = makeEnv({
    meds: [{ id: 'f3', mode: 'fixed', times: [480, 1200], interval: 8 }],
    doses: { [TODAY]: [{ id: 'k1', medId: 'f3', time: 480, status: 'taken', takenAt: 1, idx: 9, total: 9 }] }
  });
  e3.sandbox.ensureFixedDoses();
  const l3 = e3.S.doses[TODAY];
  t('★ 已有记录时不重复生成', l3.length === 1 && l3[0].id === 'k1', JSON.stringify(l3.map(d => d.id)));
  t('已打卡的记录原样保留', l3[0].status === 'taken', l3[0].status);
  t('序号被理顺（idx 9 → 0）', l3[0].idx === 0 && l3[0].total === 1, l3[0].idx + '/' + l3[0].total);

  // C-4 不影响别的药的剂量
  const e4 = makeEnv({
    meds: [{ id: 'f4', mode: 'fixed', times: [600], interval: 8 }],
    doses: { [TODAY]: [{ id: 'other', medId: 'mX', time: 300, status: 'pending', idx: 0, total: 1 }] }
  });
  e4.sandbox.ensureFixedDoses();
  const l4 = e4.S.doses[TODAY];
  t('别的药的剂量没被动', l4.length === 2 && l4.some(d => d.id === 'other'), JSON.stringify(l4.map(d => d.id)));
}

console.log('');
console.log('=== D. 打卡：固定模式不顺延（核心语义）===');
{
  const e = makeEnv({
    meds: [{ id: 'm1', name: 'A', interval: 8, mode: 'fixed', times: [480, 1200] }],
    doses: { [TODAY]: [
      { id: 'a', medId: 'm1', time: 480, status: 'pending', idx: 0, total: 2 },
      { id: 'b', medId: 'm1', time: 1200, status: 'pending', idx: 1, total: 2 }
    ] }
  });
  const list = e.S.doses[TODAY];
  // 9:30 才打卡（比计划的 08:00 晚 1.5 小时）
  const r = e.sandbox.markTaken(list[0], at(9, 30));
  t('返回 shifted 0 / dropped 0', r.shifted === 0 && r.dropped === 0, JSON.stringify(r));
  t('这次被标记为已服用', list[0].status === 'taken', list[0].status);
  t('★ 20:00 那条**没有**被顺延（仍是 1200）', list[1].time === 1200, list[1].time);
  t('★ 没有产生跨天丢弃记录', e.dropped.length === 0, JSON.stringify(e.dropped));
  eq('序号不变', list.map(d => d.idx), [0, 1]);
}

console.log('');
console.log('=== E. 回归：间隔模式仍然顺延（没被改坏）===');
{
  const e = makeEnv({
    meds: [{ id: 'm1', name: 'A', interval: 8 }],       // 没有 mode → interval
    doses: { [TODAY]: [
      { id: 'a', medId: 'm1', time: 480, status: 'pending', idx: 0, total: 2 },
      { id: 'b', medId: 'm1', time: 960, status: 'pending', idx: 1, total: 2 }
    ] }
  });
  const list = e.S.doses[TODAY];
  const r = e.sandbox.markTaken(list[0], at(9, 30));
  t('间隔模式仍会顺延', r.shifted === 1, JSON.stringify(r));
  t('★ 第二次从 16:00 顺延到 17:30（9:30 + 8h）', list[1].time === 1050, list[1].time);
}

console.log('');
console.log('=== F. 改设置后重排今天（rebuildTodayDoses）===');
{
  // F-1 固定模式改时刻表
  const e = makeEnv({
    meds: [{ id: 'm1', name: 'A', interval: 8, mode: 'fixed', times: [420, 1260] }],   // 07:00 / 21:00
    doses: { [TODAY]: [
      { id: 'a', medId: 'm1', time: 480, status: 'taken', takenAt: 1, idx: 0, total: 2 },
      { id: 'b', medId: 'm1', time: 1200, status: 'pending', idx: 1, total: 2 }
    ] }
  });
  e.sandbox.rebuildTodayDoses(e.S.meds[0]);
  const l = e.S.doses[TODAY].slice().sort((x, y) => x.time - y.time);
  eq('时刻按新表 + 保留打卡记录', l.map(d => d.time), [420, 480, 1260]);
  t('★ 已打卡的记录保留了（480 taken）', l.some(d => d.id === 'a' && d.status === 'taken'), JSON.stringify(l.map(d => d.status)));
  t('新的两条是 pending', l.filter(d => d.status === 'pending').length === 2, '');
  t('旧的 pending（1200）已移除', !l.some(d => d.id === 'b'), '还在');
  t('★ 撤掉了旧 pending 的系统通知', e.cancelled.indexOf('b') >= 0, JSON.stringify(e.cancelled));
  t('没有多撤（只撤了 1 条）', e.cancelled.length === 1, JSON.stringify(e.cancelled));
  eq('序号重排', l.map(d => d.idx), [0, 1, 2]);

  // F-2 间隔模式改间隔（今天已打卡过）
  const e2 = makeEnv({
    meds: [{ id: 'm1', name: 'A', interval: 6 }],
    doses: { [TODAY]: [
      { id: 'a', medId: 'm1', time: 480, status: 'taken', takenAt: 1, idx: 0, total: 3 },
      { id: 'b', medId: 'm1', time: 960, status: 'pending', idx: 1, total: 3 },
      { id: 'c', medId: 'm1', time: 1200, status: 'pending', idx: 2, total: 3 }
    ] }
  });
  e2.sandbox.rebuildTodayDoses(e2.S.meds[0]);
  const l2 = e2.S.doses[TODAY].slice().sort((x, y) => x.time - y.time);
  eq('★ 按新间隔 6h 从首次打卡重排', l2.map(d => d.time), [480, 840, 1200]);
  t('打卡记录仍是第一条且为 taken', l2[0].id === 'a' && l2[0].status === 'taken', l2[0].id + '/' + l2[0].status);
  t('两条旧 pending 的通知都被撤', e2.cancelled.length === 2, JSON.stringify(e2.cancelled));

  // F-3 间隔模式、今天还没打卡 → 没有排程可言
  const e3 = makeEnv({
    meds: [{ id: 'm1', name: 'A', interval: 6 }],
    doses: { [TODAY]: [{ id: 'b', medId: 'm1', time: 960, status: 'pending', idx: 0, total: 1 }] }
  });
  e3.sandbox.rebuildTodayDoses(e3.S.meds[0]);
  t('★ 没打过卡 → 今天该药清空（回到「打卡后开始计时」）',
    (e3.S.doses[TODAY] || []).filter(d => d.medId === 'm1').length === 0,
    JSON.stringify((e3.S.doses[TODAY] || []).map(d => d.time)));

  // F-4 不影响别的药
  const e4 = makeEnv({
    meds: [{ id: 'm1', name: 'A', interval: 8, mode: 'fixed', times: [600] }],
    doses: { [TODAY]: [{ id: 'other', medId: 'mX', time: 300, status: 'pending', idx: 0, total: 1 }] }
  });
  e4.sandbox.rebuildTodayDoses(e4.S.meds[0]);
  t('别的药的剂量没被动', (e4.S.doses[TODAY] || []).some(d => d.id === 'other'), '丢了');
}

console.log('');
console.log('=== G. 源码级接线（防改回去）===');
{
  t('★ 启动时预生成固定时刻', APP.indexOf('ensureFixedDoses();') >= 0, '启动没接');
  t('★ 跨天时重新预生成', /todayKey\(\) !== lastDay[\s\S]{0,220}ensureFixedDoses\(\)/.test(APP), '跨天没接');
  t('★ 保存成固定模式后立刻生成', /if \(isFixed\) ensureFixedDoses\(\);/.test(APP), '保存后没接');
  t('改设置后重排今天', /modeChanged \|\| intervalChanged\) rebuildTodayDoses\(m\)/.test(APP), '没重排');
  t('★ 保存前的校验在改数据之前（不会改一半）',
    /normTimes\(fixedTimes\)[\s\S]{0,120}请至少添加一个服药时刻[\s\S]{0,600}m\.mode = medModeDraft/.test(APP),
    '校验顺序不对');
  t('备份校验接受 mode / times',
    /m\.mode != null && m\.mode !== 'interval' && m\.mode !== 'fixed'/.test(APP)
    && /m\.mode === 'fixed'[\s\S]{0,160}Array\.isArray\(m\.times\)/.test(APP), '备份校验没放宽');
  t('展示文案统一走 medScheduleLabel', (APP.match(/medScheduleLabel\(/g) || []).length >= 4, '用的地方太少');
  t('药品卡说明走 medMetaText', APP.indexOf('medMetaText(m)') >= 0, '没接');

  console.log('  -- 界面骨架 --');
  t('有模式切换 chip', HTML_SRC.indexOf('id="modeRow"') >= 0 && HTML_SRC.indexOf('data-mode="fixed"') >= 0, '缺');
  t('有固定时刻字段块', HTML_SRC.indexOf('id="fieldFixed"') >= 0 && HTML_SRC.indexOf('id="timeList"') >= 0, '缺');
  t('有「添加时刻」按钮', HTML_SRC.indexOf('id="addTime"') >= 0, '缺');
  t('间隔字段可被隐藏（id 存在）', HTML_SRC.indexOf('id="fieldInterval"') >= 0, '缺');
  t('预览块可被隐藏（id 存在）', HTML_SRC.indexOf('id="fieldPreview"') >= 0, '缺');
  t('时刻输入框有 44px 高（触摸目标）', /\.time-input\{[^}]*height:44px/.test(CSS), '太矮');
  t('时刻行有布局样式', CSS.indexOf('.time-row') >= 0, '缺');

  console.log('  -- 无障碍 --');
  t('模式按钮有 aria-pressed', HTML_SRC.indexOf('aria-pressed="true"') >= 0, '缺');
  t('时刻输入框有 aria-label', /data-ti="'\s*\+\s*i\s*\+\s*'"[\s\S]{0,140}aria-label="第/.test(APP) ||
    APP.indexOf('个服药时刻') >= 0, '缺');
  t('删除时刻按钮有 aria-label', APP.indexOf('个时刻') >= 0, '缺');
}

console.log('');
console.log('==========================================');
console.log('通过 %d / 共 %d', pass, pass + fail);
if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
process.exit(fail ? 1 : 0);
