/* core/store.js —— 状态与持久化层
 *
 * 2026-09-24 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * 依赖方向：本模块**只依赖 core/util**（最底层），不依赖 UI、不依赖通知层 ——
 * 只有这个方向立住了，store 才能被任何模块安全引用而不成环。
 * 需要「写完盘再通知上层」的三件事，一律走 saveHooks 注入（见文件末尾），
 * **不要**为了省事去 import app.js —— 那就成环了。
 */

import { fmtDate, todayKey } from './util.js';   // cleanTargets 用到

/* ---------------- state ---------------- */
export var KEY = 'medreminder.v1';

export var DEFAULT = { meds: [], doses: {}, notified: {} };

export function load() {
  try {
    var raw = localStorage.getItem(KEY);
    if (raw) {
      var o = JSON.parse(raw);
      return { meds: o.meds || [], doses: o.doses || {}, notified: o.notified || {} };
    }
  } catch (e) { /* ignore */ }
  return JSON.parse(JSON.stringify(DEFAULT));
}

export var S = load();

/* 各实现（Chromium / Firefox / 旧 WebView）对配额异常的命名不一致，三种都认 */
export function isQuotaError(e) {
  if (!e) return false;
  return e.name === 'QuotaExceededError'
      || e.name === 'NS_ERROR_DOM_QUOTA_REACHED'
      || e.code === 22 || e.code === 1014;
}

/* localStorage 在 Android WebView 里通常是 5 MB（部分实现 10 MB），按保守的 5 MB 估算占比 */
export var STORAGE_BUDGET = 5 * 1024 * 1024;

export function storageStats() {
  var raw = null;
  try { raw = localStorage.getItem(KEY); } catch (e) { /* ignore */ }
  var bytes = raw ? raw.length * 2 : 0;      // localStorage 按 UTF-16 计，每字符 2 字节
  var keys = Object.keys(S.doses || {});
  var doses = 0;
  keys.forEach(function (k) { doses += (S.doses[k] || []).length; });
  /* 照片不在 localStorage 里，但同样占设备空间。不并进来，这张卡显示的
   * 「已用」就是错的 —— 而它的全部意义就是让容量可见。 */
  var pBytes = photoStats ? photoStats.bytes : 0;
  var pFiles = photoStats ? photoStats.files : 0;
  bytes += pBytes;
  return {
    bytes: bytes,
    kb: Math.round(bytes / 1024),
    days: keys.length,
    doses: doses,
    photoFiles: pFiles,
    photoBytes: pBytes,
    pct: Math.min(100, Math.round(bytes / STORAGE_BUDGET * 100))
  };
}

/* 回收孤儿 notified：id 对应的剂量早已不存在 → 这条标记永远不会再被读到。
 * 纯垃圾回收，不碰任何用户可见数据，所以可以在 boot 时自动执行。 */
export function gcNotified() {
  var alive = {}, removed = 0;
  Object.keys(S.doses || {}).forEach(function (k) {
    (S.doses[k] || []).forEach(function (d) { alive[d.id] = 1; });
  });
  Object.keys(S.notified || {}).forEach(function (id) {
    if (!alive[id]) { delete S.notified[id]; removed++; }
  });
  return removed;
}

/* 「预览」与「执行」共用同一套判定 —— 保证用户看到的数字就是实际会被删的数量。
 * keepDays = 0 表示不清理。**今天的记录永不删**：它是当前排程的依据，删了 App 立刻错乱。 */
export function cleanTargets(keepDays) {
  if (!keepDays) return [];
  var cutoff = new Date();
  cutoff.setDate(cutoff.getDate() - keepDays);
  var limit = fmtDate(cutoff), today = todayKey();
  return Object.keys(S.doses || {}).filter(function (k) {
    return k < today && k < limit;      // YYYY-MM-DD 的字典序即时间序
  });
}

export function cleanPreview(keepDays) {
  var keys = cleanTargets(keepDays), n = 0;
  keys.forEach(function (k) { n += (S.doses[k] || []).length; });
  return { days: keys.length, doses: n };
}

export function readJSON(k) {
  try { var r = localStorage.getItem(k); return r ? JSON.parse(r) : null; } catch (e) { return null; }
}

export function writeJSON(k, v) {
  try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) { /* ignore */ }
}

export var photoStats = { files: 0, bytes: 0 };           // 异步刷新，供存储卡统计照片占用

/* ⚠️ photoStats 是**整体替换**（照片层写 `photoStats = s`），而 import 的绑定是只读的 ——
 * 外部必须走这个 setter，直接赋值会 TypeError。 */
export function setPhotoStats(s) { photoStats = s; }


/* ---------------- 持久化：写入 + 失败可见（D-1） ----------------
 * 2026-09-28 从 app.js 搬来。save() 是「状态→磁盘」的**唯一出口**，
 * 归 store 才符合依赖方向 —— 也因此 schedule 层才能直接调它而不反向依赖 app.js。
 *
 * 一个关键事实（决定了告警文案）：setItem 是原子的，要么全写成功要么抛异常，
 * 不会写一半。所以失败**不会损坏已存数据** —— 磁盘上仍是上一次成功写入的完整版本。
 * 失败的含义是「最近的改动没存上」，不是「数据坏了」。
 *
 * ⚠️ storageError 只由本文件**写**，外部一律**读**（UI 常驻告警卡）。
 *    所以 export var 的**活绑定**就够用了 —— 外部直接 import 这个名字即可拿到最新值，
 *    不需要 getter；但也**绝不能在外部重新赋值**（import 绑定只读，会 TypeError）。 */
export var storageError = null;        // null | { kind:'quota'|'other', msg, at }

/* save() 写完盘之后的**编排**属于上层（刷新告警 / 同步通知 / 触发自动备份）。
 * core 层不允许反向 import 上层 → 由 app.js 启动时用 setSaveHooks() 注入。
 * 默认空实现：单测里不需要任何桩就能直接调 save()。 */
export var saveHooks = {
  refresh: function () {},
  notify: function () {},
  backup: function () {},
  card: function () {},     // S-9：桌面卡片的数据摘要（每次都重写沙箱里的小文件）
  shadow: function () {}    // A-3：影子状态文件（原生进程可读的副本，喂卡片 / Agent）
};
export function setSaveHooks(h) {
  if (!h) return;
  if (h.refresh) saveHooks.refresh = h.refresh;
  if (h.notify) saveHooks.notify = h.notify;
  if (h.backup) saveHooks.backup = h.backup;
  if (h.card) saveHooks.card = h.card;
  if (h.shadow) saveHooks.shadow = h.shadow;
}

/* 每次改动都落盘。写入失败**不抛给调用方**（那会让用户的操作半途中断），
 * 而是做成持续可见的状态（storageError），由 UI 常驻展示直到恢复。 */
export function save() {
  try {
    localStorage.setItem(KEY, JSON.stringify(S));
    if (storageError) { storageError = null; saveHooks.refresh(); }  // 恢复后自动撤下告警
  } catch (e) {
    var first = !storageError;
    storageError = {
      kind: isQuotaError(e) ? 'quota' : 'other',
      msg: (e && (e.name || e.message)) || '未知错误',
      at: Date.now()
    };
    if (first) saveHooks.refresh();
  }
  saveHooks.notify();
  /* 每次改动都往本地存一份（防抖 3 秒，写失败也不影响任何功能）。
   * 传出去的是与手动导出**完全一致**的格式 —— 现有恢复流程能直接读回来。 */
  saveHooks.backup();
  /* S-9 桌面卡片：摘要重算并写进沙箱文件（卡片是锦上添花，失败静默 —— 见 cardSummary.js）。 */
  saveHooks.card();
}
