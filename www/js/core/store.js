/* core/store.js —— 状态与持久化层
 *
 * 2026-09-24 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * 依赖方向：本模块**只依赖 core/util**（最底层），不依赖 UI、不依赖通知层 ——
 * 只有这个方向立住了，store 才能被任何模块安全引用而不成环。
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
