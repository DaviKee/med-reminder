/* ui/photo.js —— 拍照打卡（G-1）：拍 / 恢复 / 补记 / 看照片
 *
 * 2026-10-04 架构重构（A-2）：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * **这一块搬出来最值钱**：它承载的是 2026-09-29 真机那个"拍完了还让我再拍一次"的
 * 完整修复（相机是独立 Activity，App 可能在它运行期间被杀 → 留痕 + 恢复态 + 30 秒兜底）。
 * 它此前和"今天页渲染""药品增删"混在同一个文件里，改任一处都要整文件通读。
 *
 * ⚠️ 两个状态变量（`pendingShot` / `restoringShot`）在 app.js 有**外部写点**，
 *    所以本模块对外提供三个语义化的写入口（文件末尾）—— 见那里的说明。
 */
import { $, minToStr, pad } from '../core/util.js';
import { S, save, photoStats, setPhotoStats } from '../core/store.js';
import { checkIn, checkInAll, findDoseById, markTaken, medById, todayDoses } from '../core/schedule.js';
import { openDlg, closeDlg } from './overlay.js';
import { render } from './render.js';
import { toast } from './toast.js';
import { queueStorageRefresh } from './actions.js';

/* 本月拍照打卡统计。跳过率是这个功能最关键的观察指标：
 * 跳过率很高说明要么场景确实拍不了，要么用户不接受这个设计 ——
 * 用它来决定该收紧还是放宽，而不是一开始就锁死。 */
export function photoTally() {
  var d = new Date();
  var prefix = d.getFullYear() + '-' + pad(d.getMonth() + 1);
  var shot = 0, skipped = 0;
  Object.keys(S.doses || {}).forEach(function (k) {
    if (k.indexOf(prefix) !== 0) return;
    (S.doses[k] || []).forEach(function (x) {
      if (x.status !== 'taken') return;
      if (x.photo) shot++;
      else if (x.photoSkipped) skipped++;
    });
  });
  return { shot: shot, skipped: skipped };
}

/* 打开一张服药照片 */
export function openPhoto(id) {
  var ds = findDoseById(id);
  if (!ds || !ds.photo) return;
  var med = medById(ds.medId);
  $('#viewMeta').textContent = minToStr(ds.time) + (med ? ' · ' + med.name : '');
  var hint = $('#viewHint');
  hint.textContent = '';
  if (ds.takenAt) {
    var t = new Date(ds.takenAt);
    hint.textContent = '打卡于 ' + t.getFullYear() + '-' + pad(t.getMonth() + 1) + '-' + pad(t.getDate())
      + ' ' + pad(t.getHours()) + ':' + pad(t.getMinutes());
  }
  var img = $('#viewImg');
  img.removeAttribute('src');
  openDlg($('#dlgView'));
  window.MedPhoto.src(ds.photo).then(function (u) {
    if (u) { img.src = u; }
    else { hint.textContent = '照片文件找不到了（可能已被系统清理）。'; }
  });
}

/* ---------------- 拍照打卡（G-1） ----------------
 * 目标不是"防遗忘"，而是**防「随手划掉提醒、假装吃过」**。
 * 现在通知上的「已服用」一秒就能点掉 —— 没吃药也能清掉提醒，App 还记一笔已服用。
 * 这叫「假依从」，比漏服更难发现：漏服至少记录是空的，假依从连数据都是假的。
 * 拍照把"清掉提醒"从零成本变成有成本。
 *
 * 逃生通道按已拍板方案 B：**拍不了可以跳过，打卡照常完成，但记录留痕并计入跳过率**。
 * 为什么不能硬性阻断：现实中拍不了的场景很多（在外面 / 开会中 / 药盒没带 / 光线太暗 /
 * 存储满 / 相机权限被误关 / 老人不会用相机）。硬拦会让这些用户只能强停 App，
 * 数据直接断掉 —— 比不强制更糟。
 *
 * 浏览器预览模式没有相机插件：不假装能拍，直接放行，也**不计跳过**
 * （那是环境不支持，不是用户偷懒，混进跳过率会让指标失真）。
 */
export var PENDING_KEY = 'medreminder.pendingPhoto.v1';   // 独立 key，不碰主状态，备份格式不用动

export var pendingShot = null;                            // { kind:'all'|'one', doseId, onDone }

/* ★ 2026-09-29 真机反馈：「拍完了还让我再拍一次」。
 *
 * 根因：相机是**独立 Activity**，低端机 / 内存紧张时系统会在它运行期间**杀掉本 App**
 * （用户体感就是"卡顿"）。Capacitor 把结果存起来，重启后经 `appRestoredResult` 回放。
 * 于是出现一个**几分钟的空窗**：
 *   ① 重启后 `todayDoses()` 还是空的 → 「今天」页显示「今天还没打卡」+ 打卡按钮；
 *   ② 用户看到"没成功"，又点了一次打卡 → 又弹出拍照框（"又提示要拍照"）；
 *   ③ 与此同时 `appRestoredResult` 到达 → 照片落盘 + 打卡完成（"拍照已经成功了"）。
 *
 * 修法：重启时**只要 localStorage 里还有未消费的拍照留痕**，就把界面切到
 * 「正在恢复上次拍照」的诚实状态，并在恢复结束前**拒绝**任何新的拍照打卡
 * —— 既不让用户白点，也不让两次 photoGate 互相覆盖 pendingShot。 */
export var restoringShot = false;                         // 恢复进行中：界面据此改文案、打卡按钮据此拒绝

export function savePendingShot(kind, doseId) {
  try { localStorage.setItem(PENDING_KEY, JSON.stringify({ kind: kind, doseId: doseId || null, at: Date.now() })); } catch (e) { /* ignore */ }
}

export function loadPendingShot() {
  try { var r = localStorage.getItem(PENDING_KEY); return r ? JSON.parse(r) : null; } catch (e) { return null; }
}

export function clearPendingShot() {
  try { localStorage.removeItem(PENDING_KEY); } catch (e) { /* ignore */ }
}

/* 打卡前先过一道拍照。opts = { desc, kind:'all'|'one', doseId }
 * onDone(photoRel, skipped)：photoRel 为 null 表示这次没有照片。 */
export function photoGate(opts, onDone) {
  if (!(window.MedPhoto && window.MedPhoto.ready())) { onDone(null, false); return; }
  /* ★ 恢复中不许再开一个新的拍照会话 —— 否则会覆盖 pendingShot，
   * 让真正要恢复的那次打卡彻底对不上号（真机"又让我拍一次"的放大器）。 */
  if (restoringShot) { toast('上次的拍照还在处理，请稍等一下'); return; }
  if (pendingShot) { toast('拍照正在进行中，请稍等一下'); return; }
  pendingShot = { kind: opts.kind, doseId: opts.doseId || null, onDone: onDone };
  savePendingShot(opts.kind, opts.doseId);   // 相机 Activity 可能把 App 挤掉，先留痕
  var body = $('#photoBody');
  if (body) body.textContent = (opts.desc ? opts.desc + ' ' : '')
    + '拍下这次的药，之后能回看确认。实在拍不了可以跳过，但记录里会标出来。';
  if (pendingShot) pendingShot.body = body ? body.textContent : '';   // 重拍时用来复位提示
  var take = $('#photoTake');
  if (take) { take.disabled = false; take.textContent = '拍照打卡'; }
  openDlg($('#dlgPhoto'));
}

export function finishShot(rel, skipped) {
  var p = pendingShot;
  pendingShot = null;
  clearPendingShot();
  closeDlg($('#dlgPhoto'));
  if (p) p.onDone(rel, skipped);
}

/* 把照片路径 / 跳过标记写到一批剂量上 */
export function stampPhoto(doses, rel, skipped) {
  (doses || []).forEach(function (d) {
    if (!d) return;
    if (rel) d.photo = rel;
    if (skipped) d.photoSkipped = 1;
  });
}

/* App 在拍照期间被系统杀掉 → 恢复时把这次打卡接上。
 * Capacitor 官方明确要求监听 appRestoredResult，不处理会**同时丢照片和打卡**。 */
export function resumeShot(photoPath) {
  var p = loadPendingShot();
  clearPendingShot();
  if (!p) return;                       // 不是在打卡流程里被杀掉的，不插手
  if (!(window.MedPhoto && window.MedPhoto.ready())) return;

  restoringShot = true;                 // 恢复期间：挡住新的拍照打卡，界面显示"正在恢复"
  render();
  window.MedPhoto.fromRestored(photoPath, p.doseId || 'restored').then(function (r) {
    restoringShot = false;
    if (!r.ok) { render(); toast('照片没保存下来，这次打卡请重新点一下'); return; }
    if (p.kind === 'all') {
      var created = checkInAll();
      stampPhoto(created, r.rel, false);
      save(); render();
      toast('拍照已完成，今天的打卡已记上');
      return;
    }
    /* ★ kind === 'one'：⚠️ **两种调用方、两种 id**（2026-10-07 修复）——
     *   · bindToday 的 data-checkin 传**药品 id**（注释原话，见下）；
     *   · remindDone / btnEarly / onNotifyAction('taken') 传的都是**剂量 id**。
     * 旧代码只按药品 id 走 checkIn —— 剂量 id 传进来时 medById 永远找不到 →
     * checkIn 返回 null → 兜底循环又拿「medId === 剂量id」去比 → 也不命中 →
     * **这次打卡被静默丢掉**：拍照期间 App 被杀的用户回来发现"没记上"，
     * 只能再点一次 —— 正是"点了没效、还得再来一次"的真机反馈之一。
     * 修法：先按剂量 id 找，找得到且还是 pending 就直接 markTaken（与死亡流程
     * 本来要做的事一致）；找不到再按老语义当药品 id 走 checkIn。 */
    var ds0 = findDoseById(p.doseId);
    if (ds0 && ds0.status === 'pending') {
      stampPhoto([ds0], r.rel, false);
      markTaken(ds0, Date.now());
      save(); render();
      toast('拍照已完成，打卡已记上');
      return;
    }
    var arr = checkIn(p.doseId);
    if (arr) {
      stampPhoto(arr, r.rel, false);
      save(); render();
      var med0 = medById(p.doseId);
      toast((med0 ? med0.name : '') + ' 已打卡 · 排了 ' + arr.length + ' 次提醒');
      return;
    }
    /* checkIn 返回 null 有两种：今天已排过（也算成功，把照片挂到今天的剂量上）、
     * 或药品已被删除。挂照片要挑**还 pending 的那条**——旧代码拿第一条（可能是
     * 已服用的），照片挂错剂量、该记的那条还是 pending。 */
    var l = todayDoses(), hit = null, hitAny = null;
    for (var i = 0; i < l.length; i++) {
      if (l[i].medId === p.doseId) {
        if (!hitAny) hitAny = l[i];
        if (l[i].status === 'pending') { hit = l[i]; break; }
      }
    }
    if (!hit) hit = hitAny;
    if (hit) { stampPhoto([hit], r.rel, false); save(); render(); toast('拍照已完成，今天的打卡已记上'); return; }
    render();
  });
}

/* 照片目录占用 —— 不清点它，D-1 的存储卡就会漏报照片空间，数字变成错的 */
export function refreshPhotoStats() {
  if (!(window.MedPhoto && window.MedPhoto.ready())) return;
  window.MedPhoto.dirStats().then(function (s) {
  if (s.files === photoStats.files && s.bytes === photoStats.bytes) return;
  setPhotoStats(s);       // 走 setter：photoStats 现在住在 core/store.js（import 绑定只读）
    queueStorageRefresh();
  });
}

/* 当前所有被记录引用的照片路径 —— 供孤儿回收判断"谁还活着" */
export function allPhotoRefs() {
  var refs = [];
  Object.keys(S.doses || {}).forEach(function (k) {
    (S.doses[k] || []).forEach(function (d) { if (d.photo) refs.push(d.photo); });
  });
  return refs;
}

/* ---------------- 对外的「写入口」：本模块的状态只由本模块写 ----------------
 *
 * `pendingShot` / `restoringShot` 是 `export var` —— 外部**读**得到最新值（活绑定），
 * 但 **import 绑定只读**：app.js 那三处 `restoringShot = ...` 直接赋值会 TypeError。
 *
 * 与其把裸变量交出去，不如按**语义**包成动作 —— 与 `ui/overlay.js` 的
 * `enterOverlay()` / `leaveOverlay()` 同一套路。外部那行读起来是「做什么」，
 * 而不是「改哪个变量」。 */

/* 放弃这次拍照会话（关掉拍照浮层 = 用户不拍了）。
 * 必须**同时**清掉「待恢复」留痕 —— 否则下次启动会被误当成
 * 「拍照途中被杀」而自动补一次打卡（把"取消"变成了"打卡成功"）。 */
export function discardShotSession() {
  pendingShot = null;
  clearPendingShot();
}

/* 进入 / 离开「恢复上次拍照」态。boot 检测到留痕时 enter；
 * 恢复事件到达、或 30 秒兜底超时时 leave。 */
export function enterRestore() { restoringShot = true; }

export function leaveRestore() {
  restoringShot = false;
  clearPendingShot();
}
