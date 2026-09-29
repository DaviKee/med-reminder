/* 拍照打卡流程 —— 复现「拍完了还让我再拍一次」（真机反馈 2026-09-29）
 *
 * 现象（秦老师原话）：
 *   「我点击拍照记录了以后，它又给我提示我还需要点击拍照，这次我放弃了，
 *     但是它拍照已经成功了。」
 *
 * 场景分析（已核实 Capacitor 官方文档）：
 *   相机是**独立 Activity**。低端机 / 内存紧张时，系统会在相机 Activity 运行期间
 *   **杀掉本 App** 来省内存；Capacitor 把结果存起来，重启后通过
 *   `appRestoredResult` 事件回放。这条路径确实会**照片落盘 + 打卡完成**，
 *   但用户当时看到的是**重启后的「未打卡」界面** —— 于是又去点了「打卡」，
 *   弹出第二个拍照框（"又提示要拍照"）。
 *
 * 本用例把整条流程的**关键时序**钉住：
 *   A. 正常路径：take 成功 → 关框 + 恰好一次 onDone
 *   B. 恢复路径：appRestoredResult → 照片落盘 + 打卡完成（且**不弹框**）
 *   C. ★ 恢复完成后再点「打卡」：必须被拦下（今天已打卡），**不许再弹拍照框**
 *   D. ★ 恢复时若今天的剂量已存在：不许重复生成
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
function cutConst(src, name) {
  const re = new RegExp('var\\s+' + name + '\\s*=\\s*[^;]+;');
  const m = src.match(re);
  if (!m) throw new Error('抽不到常量: ' + name);
  return m[0];
}

/* ---------------- 用真实源码搭一个可驱动的环境 ----------------
 * 抽函数块而非手抄实现 —— 手抄只能测到"我以为的逻辑"。
 * 这里抽的是 app.js 里拍照这条链上的真实函数 + core/schedule 的 checkIn/checkInAll。 */
const SRC = [
  'var restoringShot = false;',
  cutConst(APP, 'PENDING_KEY'),
  'var pendingShot = null;',
  cut(APP, 'function savePendingShot(kind, doseId)'),
  cut(APP, 'function loadPendingShot()'),
  cut(APP, 'function clearPendingShot()'),
  cut(APP, 'function photoGate(opts, onDone)'),
  cut(APP, 'function finishShot(rel, skipped)'),
  cut(APP, 'function stampPhoto(doses, rel, skipped)'),
  cut(APP, 'function resumeShot(photoPath)'),
  cut(APP, 'function checkIn(medId)'),
  cut(APP, 'function checkInAll()'),
  cut(APP, 'function todayDoses()'),
  cut(APP, 'function medById(id)'),
  cut(APP, 'function medMode(m)'),
  cut(APP, 'function ensureFixedDoses()'),
  cut(APP, 'function rollForward(dose, takenMs)'),
  cut(APP, 'function takenToast(med, takenMs, r)')
].join('\n\n');

/* ---------------- 桩 ----------------
 * 只造流程需要的那几样：$ / openDlg / closeDlg / checkInAll 依赖的 S / save / render。 */
const store = new Map();
let dialogsOpened = 0, dialogsClosed = 0;
let rendered = 0, saved = 0;
let toasts = [];

function el(id) {
  return {
    id: id, textContent: '', disabled: false, classList: {
      add() {}, remove() {}, toggle() {}, contains() { return false; }
    }
  };
}
const ELS = {};
['#photoBody', '#photoTake', '#photoSkip', '#dlgPhoto'].forEach(k => { ELS[k] = el(k); });

const sandbox = {
  console: { warn() {}, log() {}, error() {} },
  JSON, Object, Array, String, Number, Boolean, Math, Date, isFinite, parseInt, parseFloat,
  localStorage: {
    getItem: k => (store.has(k) ? store.get(k) : null),
    setItem: (k, v) => { store.set(k, String(v)); },
    removeItem: k => { store.delete(k); }
  },
  $: sel => ELS[sel] || null,
  $$: () => [],
  openDlg: () => { dialogsOpened++; },
  closeDlg: () => { dialogsClosed++; },
  toast: msg => { toasts.push(String(msg)); },
  render: () => { rendered++; },
  save: () => { saved++; },
  window: null,
  S: { meds: [], doses: {}, notified: {} },
  // core/schedule 里 checkIn 用到的工具
  uid: (() => { let n = 0; return () => 'd' + (++n); })(),
  todayKey: () => '2026-09-29',
  minToStr: m => String(m),
  nowMin: () => 600,
  pad: n => String(n).padStart(2, '0'),
  fmtDate: d => d.getFullYear() + '-0' + (d.getMonth() + 1) + '-0' + d.getDate(),
  lastDay: '2026-09-29',
  syncNotifications: () => {},
  askNotify: () => {}
};
sandbox.window = sandbox;
sandbox.window.MedNotify = { cancelOne: () => {} };
/* MedPhoto 桩：take / fromRestored / ready 由各用例按需替换 */
sandbox.window.MedPhoto = {
  ready: () => true,
  take: () => Promise.resolve({ ok: false, reason: 'cancelled' }),
  fromRestored: () => Promise.resolve({ ok: false }),
  src: () => Promise.resolve(''),
  dirStats: () => Promise.resolve({ files: 0, bytes: 0 }),
  gc: () => Promise.resolve(0)
};
vm.createContext(sandbox);
vm.runInContext(SRC, sandbox);
const api = {
  photoGate: sandbox.photoGate,
  finishShot: sandbox.finishShot,
  resumeShot: sandbox.resumeShot,
  savePendingShot: sandbox.savePendingShot,
  loadPendingShot: sandbox.loadPendingShot,
  clearPendingShot: sandbox.clearPendingShot,
  checkInAll: sandbox.checkInAll,
  todayDoses: sandbox.todayDoses,
  isRestoring: () => sandbox.restoringShot,
  setRestoring: v => { sandbox.restoringShot = v; },
  S: sandbox.S,
  ELS: ELS
};
const setPhoto = o => Object.assign(sandbox.window.MedPhoto, o);

(async function main() {
  console.log('=== A. 正常路径：take 成功 → 关框 + 恰好一次 onDone ===');
  {
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    dialogsOpened = dialogsClosed = 0;
    let doneCount = 0, doneRel = 'X';
    api.photoGate({ desc: 'x', kind: 'all' }, (rel, sk) => { doneCount++; doneRel = rel; });
    t('photoGate 弹出了拍照框', dialogsOpened === 1, dialogsOpened);
    t('pendingShot 已留痕到 localStorage', !!api.loadPendingShot(), '没留痕');

    /* 模拟用户点了「拍照打卡」，相机会话（独立 Activity）返回 */
    setPhoto({ take: () => Promise.resolve({ ok: true, rel: 'photos/p1.jpg' }) });
    await new Promise(r => setTimeout(r, 10));
    // 模拟 photoTake.onclick 的行为（真实代码里它调 take 然后 finishShot）
    api.finishShot('photos/p1.jpg', false);
    t('onDone 恰好调用一次', doneCount === 1, doneCount);
    t('★ onDone 拿到照片相对路径', doneRel === 'photos/p1.jpg', doneRel);
    t('拍照框已关闭', dialogsClosed === 1, dialogsClosed);
    t('★ 留痕标记已清除', !api.loadPendingShot(), '还留着');
  }

  console.log('');
  console.log('=== B. 恢复路径：App 被系统杀掉 → appRestoredResult → 照片落盘 + 打卡完成 ===');
  {
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    dialogsOpened = dialogsClosed = 0; toasts = [];
    /* 用户在按下拍照前留了痕（photoGate 已跑过），然后 App 被杀 →
     * 重启后 localStorage 里仍有该标记，JS 变量 pendingShot 归 null（新上下文） */
    api.savePendingShot('all', null);
    let savedRel = '';
    setPhoto({ fromRestored: (p, id) => { savedRel = 'photos/restored.jpg'; return Promise.resolve({ ok: true, rel: savedRel }); } });

    const beforeDoses = api.todayDoses().length;
    await api.resumeShot('/tmp/cache/123.jpg');
    await new Promise(r => setTimeout(r, 20));

    t('★ 照片落盘了', savedRel === 'photos/restored.jpg', savedRel);
    t('★ 打卡完成（今天有剂量了）', api.todayDoses().length > 0, beforeDoses + ' -> ' + api.todayDoses().length);
    t('★ 恢复路径**不弹**拍照框（用户不该被再问一次）', dialogsOpened === 0, dialogsOpened);
    t('★ 恢复后提示用户"拍照已完成"', toasts.some(x => /拍照已完成|打卡已记上/.test(x)), JSON.stringify(toasts));
  }

  console.log('');
  console.log('=== C. ★ 核心 bug：恢复完成后再点「打卡」，不许再弹拍照框 ===');
  {
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    dialogsOpened = dialogsClosed = 0; toasts = [];
    api.savePendingShot('all', null);
    setPhoto({ fromRestored: () => Promise.resolve({ ok: true, rel: 'photos/restored.jpg' }) });
    await api.resumeShot('/tmp/cache/123.jpg');
    await new Promise(r => setTimeout(r, 20));

    /* 此刻今天已有剂量。用户（以为没成功）又点了「打卡」按钮。
     * #btnCheckin 的第一道闸是：if (todayDoses().length) { toast('今天已经打过卡了'); return; } */
    dialogsOpened = 0; dialogsClosed = 0;
    const hasToday = api.todayDoses().length > 0;
    t('★ 今日已有剂量（打卡确实是完成了的）', hasToday, api.todayDoses().length);
    /* 这条闸门必须存在，否则会再弹一次拍照框 —— 正是真机现象 */
    const btnCheckinGuard = /if \(todayDoses\(\)\.length\) \{ toast\('今天已经打过卡了'\); return; \}/.test(APP);
    t('★ #btnCheckin 有「今天已打卡」闸门（挡住第二次弹框）', btnCheckinGuard, '闸门不在或写法变了');

    /* 即便绕过了闸门走到 photoGate，onDone 里也要再挡一道：
     * checkInAll() 返回空数组时应提示"已经打过卡"，而不是静默成功 */
    const onDoneGuard = /checkInAll\(\);[\s\S]{0,120}created\.length \?/.test(APP);
    t('★ onDone 里对「checkInAll 返回空」也做了提示', onDoneGuard, '没兜底');
  }

  console.log('');
  console.log('=== D. ★ 恢复时若今天已有剂量：不许重复生成 ===');
  {
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    /* 先正常打一次卡，产生剂量 */
    api.checkInAll();
    const n1 = api.todayDoses().length;
    /* 再来一次恢复（模拟重复事件 / 重放） */
    api.savePendingShot('all', null);
    setPhoto({ fromRestored: () => Promise.resolve({ ok: true, rel: 'photos/r2.jpg' }) });
    await api.resumeShot('/tmp/cache/456.jpg');
    await new Promise(r => setTimeout(r, 20));
    t('★ 剂量数没有翻倍（幂等）', api.todayDoses().length === n1, n1 + ' -> ' + api.todayDoses().length);
  }

  console.log('');
  console.log('=== E. 源码级：恢复路径的接线完整性 ===');
  {
    t('★ 监听的是 appRestoredResult（不是别的名字）',
      /addListener\('appRestoredResult'/.test(APP), '没监听');
    t('★ 只处理 Camera 插件的结果',
      /pluginId !== 'Camera'/.test(APP), '没按 pluginId 过滤');
    t('★ 有 photo.path 才走恢复（防空结果）',
      /photo\.path\)\s*resumeShot/.test(APP), '没判空');
    t('★ resumeShot 开头就清留痕（防止重复触发）',
      /function resumeShot\(photoPath\) \{[\s\S]{0,200}clearPendingShot\(\)/.test(APP), '没清');
    t('★ 拍照框打开时按钮文案复位（重拍场景）',
      /take\.textContent = '拍照打卡'/.test(APP), '没复位');
  }

  console.log('');
  console.log('=== F. ★ 新守卫：恢复中不许再开拍照会话（挡「又让我拍一次」） ===');
  {
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    dialogsOpened = dialogsClosed = 0; toasts = [];

    api.setRestoring(true);
    let done2 = 0;
    api.photoGate({ desc: 'x', kind: 'all' }, () => { done2++; });
    t('★ 恢复中 photoGate **不弹框**', dialogsOpened === 0, dialogsOpened);
    t('★ 恢复中 photoGate 不调用 onDone（不静默成功）', done2 === 0, done2);
    t('★ 恢复中 photoGate 给出了提示', toasts.some(x => /还在处理|稍等/.test(x)), JSON.stringify(toasts));
    t('★ 恢复中 photoGate 不留新痕（不覆盖正要恢复的那次）', !api.loadPendingShot(), '被覆盖了');

    /* pendingShot 已在跑（用户已点了拍照、相机 Activity 正在前台）时，再点打卡也要拦 */
    api.setRestoring(false);
    api.photoGate({ desc: 'x', kind: 'all' }, () => {});
    dialogsOpened = 0; toasts = [];
    api.photoGate({ desc: 'x', kind: 'all' }, () => {});
    t('★ 已有 pendingShot 时第二次 photoGate 被拦（不弹第二个框）', dialogsOpened === 0, dialogsOpened);
    t('★ 第二次调用有提示', toasts.some(x => /正在进行中|稍等/.test(x)), JSON.stringify(toasts));
    api.clearPendingShot(); sandbox.pendingShot = null;

    /* 源码级：两个闸门确实写在 photoGate 里、且在 savePendingShot **之前** */
    const gateBody = cut(APP, 'function photoGate(opts, onDone)');
    const iRestore = gateBody.indexOf('if (restoringShot)');
    const iPending = gateBody.indexOf('if (pendingShot)');
    const iSave = gateBody.indexOf('savePendingShot(');
    t('★ restoringShot 闸门存在且在留痕之前', iRestore >= 0 && iRestore < iSave, iRestore + '<' + iSave);
    t('★ pendingShot 闸门存在且在留痕之前', iPending >= 0 && iPending < iSave, iPending + '<' + iSave);
  }

  console.log('');
  console.log('=== G. ★ boot 接线：重启时若有留痕 → 进恢复态 + 30 秒兜底 ===');
  {
    t('★ boot 里检查 loadPendingShot() 并置 restoringShot',
      /if \(loadPendingShot\(\)\) restoringShot = true;/.test(APP), '没接线');
    t('★ 有 30 秒兜底解锁（不让用户永久点不了打卡）',
      /restoringShot[\s\S]{0,400}setTimeout\(function \(\) \{[\s\S]{0,200}30000/.test(APP), '没兜底');
    t('★ 兜底里清了留痕（否则下次启动又进恢复态）',
      /restoringShot = false;[\s\S]{0,120}clearPendingShot\(\);[\s\S]{0,120}\}, 30000\)/.test(APP), '没清留痕');
    t('★ renderToday 有「正在恢复上次拍照…」诚实态',
      /正在恢复上次拍照/.test(APP), '没有恢复态文案');
    t('★ 恢复态文案在「今天还没打卡」之前（优先显示）',
      APP.indexOf('正在恢复上次拍照') < APP.indexOf('今天还没打卡'), '顺序反了');
  }

  console.log('');
  console.log('=== H. ★ kind:\'one\' 恢复：medId/doseId 错配不再静默丢打卡 ===');
  {
    /* 接线级：resumeShot 的 one 分支必须走 checkIn(p.doseId)（与 photoGate 调用方约定一致），
     * 而不是拿 med.id 去 todayDoses() 里找 dose.id（旧代码 → 永远找不到）。 */
    const resumeBody = cut(APP, 'function resumeShot(photoPath)');
    t('★ one 分支走 checkIn(p.doseId)', /checkIn\(p\.doseId\)/.test(resumeBody), '没走 checkIn');
    t('★ 旧错配写法（l[i].id === p.doseId）已移除',
      !/l\[i\]\.id === p\.doseId/.test(resumeBody), '旧写法还在');
    t('★ 回退分支按 medId 匹配补挂照片',
      /l\[i\]\.medId === p\.doseId/.test(resumeBody), '没按 medId 回退');

    /* 行为级：以 med.id 走一次恢复，今天必须真的有剂量（不静默丢） */
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    dialogsOpened = dialogsClosed = 0; toasts = [];
    api.savePendingShot('one', 'm1');            // ← 关键：这里是 **药品 id**（真实调用方如此）
    setPhoto({ fromRestored: () => Promise.resolve({ ok: true, rel: 'photos/one.jpg' }) });
    const before = api.todayDoses().length;
    await api.resumeShot('/tmp/cache/789.jpg');
    await new Promise(r => setTimeout(r, 20));
    t('★ med.id 走的恢复**确实产生了剂量**（旧代码这里会静默丢）',
      api.todayDoses().length > before, before + ' -> ' + api.todayDoses().length);
    t('★ 剂量挂在正确的药上', api.todayDoses().some(d => d.medId === 'm1'), '挂错了药');
    t('★ 恢复结束后解锁（restoringShot 复位）', api.isRestoring() === false, '还锁着');
    t('★ 留痕已清', !api.loadPendingShot(), '没清');
  }

  console.log('');
  console.log('=== I. ★ restoringShot 生命周期（置位 → 处理 → 复位） ===');
  {
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    let sawRestoringDuringFly = null;
    api.savePendingShot('all', null);
    setPhoto({
      fromRestored: () => { sawRestoringDuringFly = api.isRestoring(); return Promise.resolve({ ok: true, rel: 'photos/lc.jpg' }); }
    });
    t('★ 调用前是解锁态', api.isRestoring() === false, '一开始就锁着');
    await api.resumeShot('/tmp/cache/aaa.jpg');
    t('★ 照片落盘那一刻处于恢复态（界面能显示「正在恢复」）', sawRestoringDuringFly === true, sawRestoringDuringFly);
    await new Promise(r => setTimeout(r, 20));
    t('★ 处理完自动复位', api.isRestoring() === false, '没复位');

    /* 照片保存失败也要复位（否则用户被永久锁住） */
    store.clear(); sandbox.S.meds = [{ id: 'm1', name: '阿司匹林', interval: 8 }]; sandbox.S.doses = {};
    toasts = [];
    api.savePendingShot('all', null);
    setPhoto({ fromRestored: () => Promise.resolve({ ok: false }) });
    await api.resumeShot('/tmp/cache/bbb.jpg');
    await new Promise(r => setTimeout(r, 20));
    t('★ 恢复失败后也复位（不永久锁死）', api.isRestoring() === false, '锁死了');
    t('★ 恢复失败给用户提示重来', toasts.some(x => /没保存下来|重新点/.test(x)), JSON.stringify(toasts));
  }

  console.log('');
  console.log('==========================================');
  console.log('通过 %d / 共 %d', pass, pass + fail);
  if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
  process.exit(fail ? 1 : 0);
})().catch(e => { console.error('用例本身抛错:', e); process.exit(2); });
