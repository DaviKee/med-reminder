/* 备份导入校验（H-3）
 *
 * 需求（来自排期 §0.2 第 5 位）：
 *   「备份导入校验只覆盖 meds 一半，**doses 侧零校验**（字段/体积/防重复全裸奔）」
 *
 * 为什么这条是真问题（逐行核实过后果）：
 *   parseBackup() 原来只对 o.data.meds 逐条验，对 o.data.doses **只查
 *   typeof === 'object'**，于是下面这些都放行，并在导入后炸在别处：
 *     · doses 值是字符串/数字/对象（不是数组） → `S.doses[k].some(...)`
 *       （app.js hasTaken）/ `.forEach` 直接 TypeError —— 记录页整页白屏；
 *     · 剂量 time 超出 0~1439 → minToStr / 排程比较得到荒谬结果；
 *     · 剂量缺 id/medId → 照片回收、依从率统计按 undefined 归组；
 *     · status 是未知值 → 既不算已服也不算待服，静默丢失统计；
 *     · 体积无上限 → 一份 200 MB 的 JSON 能塞爆 localStorage 配额。
 *   本用例把上述每一条都钉住。
 *
 * 做法：从**真实源码文本**里抽 parseBackup 函数块（它自己引用
 *       BACKUP_FORMAT / BACKUP_VERSION 两个常量，一并抽出），在 vm 里跑。
 *       不重抄一份实现 —— 那只能测到"我以为的校验"。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();

const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = stripJs(APP_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}

/* ---------------- 从真实源码里抽块 ---------------- */
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

/* 常量必须从源码里抽出来 —— 否则测试自己写一份 'medreminder.backup'，
 * 源码真改了前缀这里也发现不了。 */
function cutConst(src, name) {
  const re = new RegExp('var\\s+' + name + '\\s*=\\s*[^;]+;');
  const m = src.match(re);
  if (!m) throw new Error('抽不到常量: ' + name);
  return m[0];
}

const SRC = [
  cutConst(APP, 'BACKUP_FORMAT'),
  cutConst(APP, 'BACKUP_VERSION'),
  cutConst(APP, 'BACKUP_MAX_CHARS'),
  cutConst(APP, 'BACKUP_MAX_DOSES_PER_DAY'),
  cutConst(APP, 'DOSE_STATUSES'),
  cut(APP, 'function isDateKey(k)'),
  cut(APP, 'function dosesInvalid(doses)'),
  cut(APP, 'function parseBackup(txt)')
].join('\n\n');

/* ---------------- 在 vm 里跑真实 parseBackup ---------------- */
const sandbox = {
  console: { warn() {}, log() {}, error() {} },
  JSON, Object, Array, String, Number, Boolean, isFinite, Math
};
sandbox.window = sandbox;
vm.createContext(sandbox);
vm.runInContext(SRC, sandbox);
const parseBackup = sandbox.parseBackup;
if (typeof parseBackup !== 'function') throw new Error('parseBackup 没抽出来');

/* ---------------- 构造合法备份的助手 ---------------- */
function goodMed(over) {
  return Object.assign({ id: 'm1', name: '阿司匹林', interval: 8 }, over || {});
}
/* 一天的剂量数组；doses 的键是 'YYYY-MM-DD'，值是剂量数组 */
function goodDose(over) {
  return Object.assign({ id: 'd1', medId: 'm1', time: 480, status: 'pending', takenAt: null }, over || {});
}
function backup(over) {
  const data = Object.assign(
    { meds: [goodMed()], doses: { '2026-09-01': [goodDose()] } },
    (over && over.data) || {}
  );
  const o = Object.assign({ format: 'medreminder.backup', version: 1, app: 'MedReminder', data }, over || {});
  if (over && Object.prototype.hasOwnProperty.call(over, 'data')) o.data = over.data;
  return JSON.stringify(o);
}
const parsed = str => parseBackup(str);

console.log('=== A. 合法备份必须放行（别把校验做成"一律拒绝"）===');
{
  const r = parsed(backup());
  t('★ 标准备份通过', !r.err && !!r.ok, JSON.stringify(r.err || 'ok'));
  t('  返回的是解析后的对象', r.ok && r.ok.data.meds.length === 1 && !!r.ok.data.doses['2026-09-01'],
    JSON.stringify(r.ok && Object.keys(r.ok.data.doses)));

  /* 多天、多剂量、含 taken/skipped、含旧备份（无 mode/times） */
  const rich = parsed(backup({
    data: {
      meds: [goodMed(), goodMed({ id: 'm2', name: '降压药', mode: 'fixed', times: [480, 1200] })],
      doses: {
        '2026-09-01': [goodDose(), goodDose({ id: 'd2', status: 'taken', takenAt: 1756700000000 })],
        '2026-09-02': [goodDose({ id: 'd3', status: 'skipped' })]
      }
    }
  }));
  t('多天/多状态/含 fixed 药 通过', !rich.err, JSON.stringify(rich.err || 'ok'));

  const emptyDoses = parsed(backup({ data: { meds: [], doses: {} } }));
  t('空数据（新装 App 的备份）通过', !emptyDoses.err, JSON.stringify(emptyDoses.err || 'ok'));

  /* 旧格式：meds 里有 dose-interval 但剂量没有 id（历史数据）——
   * 需求明确要求 id 必须存在才收；这里确认它**被拒**，见 E 组。 */
}

console.log('');
console.log('=== B. 顶层格式（这些本来就该拒，确认没被改坏）===');
{
  t('空内容 → 拒绝', !!parsed('').err, '');
  t('空白 → 拒绝', !!parsed('   \n ').err, '');
  t('非 JSON → 拒绝', !!parsed('{不是json').err, '');
  t('format 不对 → 拒绝', !!parsed(JSON.stringify({ format: 'other', version: 1, data: {} })).err, '');
  t('版本过高 → 拒绝', !!parsed(backup({ version: 99 })).err, '');
  t('缺 data → 拒绝', !!parsed(JSON.stringify({ format: 'medreminder.backup', version: 1 })).err, '');
}

console.log('');
console.log('=== C. ★ doses 侧：值必须是「日期键 → 剂量数组」===');
{
  const bad = mk => parsed(backup({ data: { meds: [goodMed()], doses: mk } }));

  t('★ doses 值是字符串 → 拒绝', !!bad({ '2026-09-01': 'oops' }).err, JSON.stringify(bad({ '2026-09-01': 'oops' })));
  t('★ doses 值是数字 → 拒绝', !!bad({ '2026-09-01': 123 }).err, '');
  t('★ doses 值是对象（不是数组）→ 拒绝', !!bad({ '2026-09-01': { a: 1 } }).err, '');
  t('★ doses 值是 null → 拒绝', !!bad({ '2026-09-01': null }).err, '');
  t('★ doses 是数组（整体类型错）→ 拒绝', !!bad([1, 2, 3]).err, '');
  t('★ 一天里混入一个非对象剂量 → 拒绝', !!bad({ '2026-09-01': [goodDose(), 'x'] }).err, '');
  t('  空数组是合法的（那天没记录）', !bad({ '2026-09-01': [] }).err, JSON.stringify(bad({ '2026-09-01': [] }).err || 'ok'));
}

console.log('');
console.log('=== D. ★ doses 侧：日期键必须是 YYYY-MM-DD ===');
{
  const bad = mk => parsed(backup({ data: { meds: [goodMed()], doses: mk } }));
  const okKey = k => !bad({ [k]: [goodDose()] }).err;

  t('★ "2026-9-1"（未补零）→ 拒绝', !okKey('2026-9-1'), '放行了');
  t('★ "20260901"（无分隔）→ 拒绝', !okKey('20260901'), '放行了');
  t('★ "2026-13-01"（月份越界）→ 拒绝', !okKey('2026-13-01'), '放行了');
  t('★ "2026-02-30"（不存在的日期）→ 拒绝', !okKey('2026-02-30'), '放行了');
  t('★ ""（空键）→ 拒绝', !okKey(''), '放行了');
  t('★ "not-a-date" → 拒绝', !okKey('not-a-date'), '放行了');
  t('  合法键 "2026-09-01" 通过', okKey('2026-09-01'), '被拒了');
  t('  合法键 "2024-02-29"（闰日）通过', okKey('2024-02-29'), '被拒了');
  t('  非法闰日 "2025-02-29" → 拒绝', !okKey('2025-02-29'), '放行了');
}

console.log('');
console.log('=== E. ★ doses 侧：每条剂量的字段必须合法 ===');
{
  const withDose = d => parsed(backup({ data: { meds: [goodMed()], doses: { '2026-09-01': [d] } } }));
  const okDose = d => !withDose(d).err;

  t('★ 缺 id → 拒绝', !okDose({ medId: 'm1', time: 480, status: 'pending' }), '放行了');
  t('★ id 非字符串 → 拒绝', !okDose(goodDose({ id: 123 })), '放行了');
  t('★ 缺 medId → 拒绝', !okDose({ id: 'd1', time: 480, status: 'pending' }), '放行了');
  t('★ medId 非字符串 → 拒绝', !okDose(goodDose({ medId: 5 })), '放行了');

  t('★ time 缺失 → 拒绝', !okDose({ id: 'd1', medId: 'm1', status: 'pending' }), '放行了');
  t('★ time 是字符串 → 拒绝', !okDose(goodDose({ time: '480' })), '放行了');
  t('★ time = -1 → 拒绝', !okDose(goodDose({ time: -1 })), '放行了');
  t('★ time = 1440（越界）→ 拒绝', !okDose(goodDose({ time: 1440 })), '放行了');
  t('★ time = NaN → 拒绝', !okDose(goodDose({ time: NaN })), '放行了');
  t('★ time = 0 与 1439 是合法的边界', okDose(goodDose({ time: 0 })) && okDose(goodDose({ time: 1439 })), '边界被误拒');

  t('★ status 未知值 → 拒绝', !okDose(goodDose({ status: 'flying' })), '放行了');
  t('★ status 缺失 → 拒绝', !okDose({ id: 'd1', medId: 'm1', time: 480 }), '放行了');
  t('  status=pending/taken/skipped 都通过',
    ['pending', 'taken', 'skipped'].every(s => okDose(goodDose({ status: s }))), '有被误拒的');

  t('★ takenAt 是字符串 → 拒绝', !okDose(goodDose({ status: 'taken', takenAt: 'yesterday' })), '放行了');
  t('  takenAt=null 合法（未服药）', okDose(goodDose({ takenAt: null })), '被误拒');
  t('  takenAt=数字合法（已服药）', okDose(goodDose({ status: 'taken', takenAt: 1756700000000 })), '被误拒');

  /* medId 允许"指向已删除的药"—— 历史记录里药品删了、剂量还在，
   * 这是刻意保留的"已发生的事实"（见工程纪律 §4.5）。不能因为对不上 meds 就拒。 */
  const orphan = parsed(backup({
    data: { meds: [goodMed()], doses: { '2026-09-01': [goodDose({ medId: '已删除的药id' })] } }
  }));
  t('★ 剂量指向已删除的药品 → 仍放行（保留历史）', !orphan.err, JSON.stringify(orphan.err || 'ok'));
}

console.log('');
console.log('=== F. ★ 体积上限（防一份巨型 JSON 塞爆 localStorage）===');
{
  /* 造一份超大的合法备份：把一天撑到几十万条剂量 */
  const bigDoses = {};
  const one = JSON.stringify(goodDose()).length + 1;
  const perDay = 4000;
  for (let i = 0; i < 12; i++) {
    bigDoses['2026-09-' + String(i + 1).padStart(2, '0')] = Array.from({ length: perDay }, (_, n) => goodDose({ id: 'd' + i + '_' + n, time: n % 1440 }));
  }
  const bigTxt = backup({ data: { meds: [goodMed()], doses: bigDoses } });
  const mb = (bigTxt.length / 1024 / 1024).toFixed(1);
  t('  （用例本身：造了 ' + mb + ' MB 的备份）', bigTxt.length > 0, '');
  t('★ 超过体积上限 → 拒绝并给出提示', !!parsed(bigTxt).err, '放行了 ' + mb + 'MB');

  /* 正常大小的备份不受影响 */
  const normalDoses = {};
  for (let i = 1; i <= 31; i++) {
    normalDoses['2026-08-' + String(i).padStart(2, '0')] = Array.from({ length: 12 }, (_, n) => goodDose({ id: 'n' + i + '_' + n, time: (n * 60) % 1440 }));
  }
  const normalTxt = backup({ data: { meds: [goodMed()], doses: normalDoses } });
  t('  一个月的正常记录（' + (normalTxt.length / 1024).toFixed(0) + ' KB）必须放行',
    !parsed(normalTxt).err, JSON.stringify(parsed(normalTxt).err || 'ok'));
}

console.log('');
console.log('=== G. ★ 防重复导入（幂等：同一份备份导入两次不翻倍）===');
{
  /* 这条的正确性靠 applyRestore 侧的判据，而 applyRestore 依赖 DOM / S / save，
   * 没法在 vm 里整体跑。所以这里改用**源码级断言**：
   *   · 必须存在一个"重复导入"的识别与拒绝路径；
   *   · 不能只是把 d.doses 直接赋值覆盖（那会把两次的记录叠起来——
   *     实际上现在是整体覆盖，不会叠；真正会叠的是"导入到已有数据"的语义）。
   * 我们要求实现里**有显式的重复/冲突检查**，并在命中时向用户说明。 */
  const hasDupGuard = /重复|已导入|identical|sameBackup|已经恢复过|duplicate/i.test(APP);
  t('★ 源码里存在"重复导入"的识别/提示路径', hasDupGuard, '没找到');
}

console.log('');
console.log('=== H. 与 parseBackup 的接线（源码级，防抽错块）===');
{
  t('★ parseBackup 确实校验了 doses（不只是 typeof object）',
    /data\.doses/.test(cut(APP, 'function parseBackup(txt)')) && /Array\.isArray/.test(cut(APP, 'function parseBackup(txt)')),
    '校验不完整');
  t('★ applyRestore 走 parseBackup（不绕过校验）',
    /parseBackup\(\$\('#dataArea'\)\.value\)/.test(APP), '没走校验');
  t('★ 体积上限有具体阈值（静态常量或字面量）',
    /(MAX|LIMIT|BUDGET|CAP)[A-Z_]*\s*=|安全上限|体积/.test(cut(APP, 'function parseBackup(txt)')) || /体积/.test(APP),
    '没找到阈值');
}

console.log('');
console.log('==========================================');
console.log('通过 %d / 共 %d', pass, pass + fail);
if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
process.exit(fail ? 1 : 0);
