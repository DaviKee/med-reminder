/* 药品删除 + 禁止相册（禁止从相册选照片）
 *
 * 两条需求：
 *   1. 「拍照只允许现场拍，不允许选择照片」→ 不弹「从相册选择 / 拍照」选择框
 *   2. 「设置里要能增删改查药品」→ 删除要安全（二次确认）、要正确（撤掉系统通知）
 *
 * ⚠️ 查源码一律先剥离注释：我们习惯在注释里引用旧写法做对比说明，
 *    不剥离就会把「说明文字」当成「代码」，报假阳性。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = fs.readFileSync(path.join(ROOT, 'www/js/app.js'), 'utf8');
const PHOTO_SRC = fs.readFileSync(path.join(ROOT, 'www/js/photo.js'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');

const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = stripJs(APP_SRC);
const PHOTO = stripJs(PHOTO_SRC);

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

const BLOCKS = [
  cut(APP_SRC, 'function fmtDate(d)'),
  cut(APP_SRC, 'function todayKey()'),
  cut(APP_SRC, 'function todayDoses()'),
  cut(APP_SRC, 'function medById(id)'),
  cut(APP_SRC, 'function deleteMed(id)')
].join('\n\n');

const TODAY = '2026-09-17';
const RealDate = Date;

function makeEnv(state) {
  const cancelled = [];
  const NOW = new RealDate('2026-09-17T12:00:00');
  function FakeDate() { return new RealDate(NOW); }
  FakeDate.now = () => NOW.getTime();

  const sandbox = {
    console: { warn() {}, log() {}, error() {} },
    Date: FakeDate, Math, JSON, Object, Array, String, Number, Boolean,
    isFinite, parseInt, parseFloat,
    pad: n => String(n).padStart(2, '0'),
    S: state
  };
  sandbox.window = sandbox;
  sandbox.window.MedNotify = { cancelOne: id => { cancelled.push(id); } };
  vm.createContext(sandbox);
  vm.runInContext(BLOCKS, sandbox);
  return { sandbox, S: state, cancelled, deleteMed: sandbox.deleteMed };
}

/* ---------------- 复刻 CameraPlugin 对 source 的处理（Java） ----------------
 *   try {
 *       settings.setSource(CameraSource.valueOf(call.getString("source", CameraSource.PROMPT.getSource())));
 *   } catch (IllegalArgumentException ex) {
 *       settings.setSource(CameraSource.PROMPT);
 *   }
 * 枚举名全大写：PROMPT / CAMERA / PHOTOS。
 * 非法值（比如小写 'camera'）会被 valueOf 拒绝 → catch → 落到 PROMPT，
 * 而 doShow() 的 default 分支正是 showPrompt()（弹「从相册选择 / 拍照」选择框）。
 */
function resolveSource(v) {
  const NAMES = ['PROMPT', 'CAMERA', 'PHOTOS'];
  const s = (v === undefined || v === null) ? 'PROMPT' : v;
  return NAMES.indexOf(s) >= 0 ? s : 'PROMPT';     // 非法值 → PROMPT → 会弹选择框
}

console.log('=== A. 复现「拍照时弹出从相册选择」（旧行为）===');
{
  t('★ 复现：source 传小写 "camera" → 被降级成 PROMPT → 弹出选择框',
    resolveSource('camera') === 'PROMPT', resolveSource('camera'));
  t('★ 复现：完全不传 source → 默认 PROMPT → 同样弹选择框',
    resolveSource(undefined) === 'PROMPT', resolveSource(undefined));
  t('★ 复现：传 "PHOTOS" 会直接进相册（这正是要禁掉的能力）',
    resolveSource('PHOTOS') === 'PHOTOS', resolveSource('PHOTOS'));
}

console.log('');
console.log('=== B. 修复后：直接进相机，不弹选择框 ===');
{
  // 从真实源码里取出实际传的值
  const m = /source:\s*'([^']+)'/.exec(PHOTO);
  t('photo.js 里取到了 source 参数', !!m, m && m[0]);
  const passed = m ? m[1] : '(取不到)';
  t('★ 传的是全大写 CAMERA', passed === 'CAMERA', passed);
  t('★ 插件会直接走 showCamera（不弹选择框）', resolveSource(passed) === 'CAMERA', resolveSource(passed));
  t('★ 源码里已无小写的 source: \'camera\'', PHOTO.indexOf("source: 'camera'") < 0, '仍是小写');
  t('没有传 promptLabel* 参数（那些只在 PROMPT 模式用）',
    !/promptLabel/.test(PHOTO), '传了 promptLabel');
  t('仍不允许存进系统相册（saveToGallery: false）',
    /saveToGallery:\s*false/.test(PHOTO), 'saveToGallery 不是 false');
}

console.log('');
console.log('=== C. 删除药品：撤掉系统通知 + 保留历史 ===');
{
  const state = {
    meds: [{ id: 'm1', name: '药A', interval: 8 }, { id: 'm2', name: '药B', interval: 12 }],
    doses: {
      '2026-09-16': [
        { id: 'h1', medId: 'm1', time: 480, status: 'taken' },     // 历史：必须保留
        { id: 'h2', medId: 'm1', time: 960, status: 'skipped' }    // 历史：必须保留
      ],
      [TODAY]: [
        { id: 'd1', medId: 'm1', time: 480, status: 'taken' },
        { id: 'd2', medId: 'm1', time: 960, status: 'pending' },
        { id: 'd3', medId: 'm2', time: 600, status: 'pending' }    // 别的药：不能动
      ]
    }
  };
  const env = makeEnv(state);

  const r = env.deleteMed('m1');
  t('返回被删药品信息', r && r.name === '药A' && r.doses === 2, JSON.stringify(r));
  t('★ 撤掉了今天该药**每一条**剂量的系统通知',
    env.cancelled.indexOf('d1') >= 0 && env.cancelled.indexOf('d2') >= 0,
    env.cancelled.join(','));
  t('★ 没有误撤别的药的通知', env.cancelled.indexOf('d3') < 0, env.cancelled.join(','));
  t('★ 药品已从列表移除', state.meds.length === 1 && state.meds[0].id === 'm2',
    state.meds.map(m => m.id).join(','));
  t('★ 今天该药的剂量已移除', state.doses[TODAY].filter(d => d.medId === 'm1').length === 0,
    JSON.stringify(state.doses[TODAY].map(d => d.id)));
  t('★ 别的药的剂量原样保留', state.doses[TODAY].some(d => d.id === 'd3'), 'd3 丢了');
  t('★ **历史记录必须保留**（吃过药的事实不该因删药而消失）',
    state.doses['2026-09-16'].length === 2, JSON.stringify(state.doses['2026-09-16']));
  t('medById 已查不到它', env.sandbox.medById('m1') === null, '还能查到');
}

console.log('');
console.log('=== D. 删除不存在的药品：不能误伤任何数据 ===');
{
  const state = {
    meds: [{ id: 'm1', name: '药A', interval: 8 }],
    doses: { [TODAY]: [{ id: 'd1', medId: 'm1', time: 480, status: 'pending' }] }
  };
  const env = makeEnv(state);
  const r = env.deleteMed('nope');
  t('返回 null', r === null, JSON.stringify(r));
  t('★ 什么都没动', env.cancelled.length === 0 && state.meds.length === 1 && state.doses[TODAY].length === 1,
    JSON.stringify({ c: env.cancelled, m: state.meds.length, d: state.doses[TODAY].length }));
}

console.log('');
console.log('=== E. 删除必须二次确认（源码级接线）===');
{
  const i = APP.indexOf("$('#deleteMed').onclick");
  const seg = APP.slice(i, i + 900);
  t('★ 走二次确认（askConfirm）', seg.indexOf('askConfirm(') >= 0, '没调用 askConfirm');
  t('★ 不再直接改 S.meds（旧写法）', seg.indexOf('S.meds = S.meds.filter') < 0, '仍在直接删');
  t('★ 确认回调里才真正调用 deleteMed', seg.indexOf('deleteMed(id)') >= 0, '没调用 deleteMed');
  t('确认文案说明了「今天的提醒会一并取消」', seg.indexOf('一并取消') >= 0, '文案缺说明');
  t('确认文案说明了「历史记录会保留」', seg.indexOf('历史服药记录会保留') >= 0, '文案缺说明');
  t('确认框的确定 / 取消都接了线',
    APP.indexOf("$('#confirmOk').onclick") >= 0 && APP.indexOf("$('#confirmCancel').onclick") >= 0,
    '按钮未绑定');
  t('取消时清掉回调，避免残留', /confirmCancel'\)\.onclick[\s\S]{0,80}confirmCb = null/.test(APP),
    '取消未清回调');
  t('index.html 有确认框骨架',
    HTML_SRC.indexOf('id="dlgConfirm"') >= 0 && HTML_SRC.indexOf('id="confirmOk"') >= 0
    && HTML_SRC.indexOf('id="confirmBody"') >= 0, '骨架缺元素');
  t('通用 askConfirm 已定义且可复用', APP.indexOf('function askConfirm(') >= 0, '没定义');
  t('药品列表提示已说明可以删', APP.indexOf('或者删掉它') >= 0, '提示未更新');
  // 无障碍：确认框有正确的 dialog 语义
  t('确认框有 role/aria', /id="dlgConfirm"[\s\S]{0,220}role="dialog"[\s\S]{0,120}aria-modal/.test(HTML_SRC),
    '缺 aria');
}

console.log('');
console.log('=== F. 回归：拍照链路其它约束没被破坏 ===');
{
  t('仍落到 DATA/photos 子目录', PHOTO.indexOf('ensureDir') >= 0, 'ensureDir 没了');
  t('仍有 L1 质量校验', PHOTO.indexOf('function qualityIssue') >= 0, '没了');
  t('仍有 L2 防重复', PHOTO.indexOf('function isDuplicate') >= 0, '没了');
  t('仍保留「两条路都带原因」', PHOTO.indexOf("'copy: ' + (firstErr") >= 0, '没了');
  t('仍有跳过通道（拍不了直接打卡）', APP.indexOf('photoSkip') >= 0, '没了');
  t('resultType 仍是 uri（大图不走 base64）', /resultType:\s*'uri'/.test(PHOTO), '变了');
}

console.log('');
console.log('==========================================');
console.log('通过 %d / 共 %d', pass, pass + fail);
if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
process.exit(fail ? 1 : 0);
