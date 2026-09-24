/* 通用工具层 —— 纯函数，不碰业务状态、不碰 DOM 之外的副作用
 *
 * 2026-09-24 架构重构第一步：从 app.js 抽出的「原子能力」。
 * 抽取原则是「无依赖」—— 这一层不 import 任何其他模块，处在依赖图最底部。
 * 之所以独立成模块：它们被 app.js 里几十个函数引用（fmtDate 5 处、minToStr 14 处、
 * todayKey 14 处），放在底层才不会形成循环依赖。
 */

export var $ = function (s, r) { return (r || document).querySelector(s); };

export var $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };

export var pad = function (n) { return String(n).padStart(2, '0'); };

export var uid = function () { return 'd' + Date.now().toString(36) + Math.random().toString(36).slice(2, 7); };

/* ---------------- date / time helpers ---------------- */
export function fmtDate(d) { return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()); }

export function nowMin() { var d = new Date(); return d.getHours() * 60 + d.getMinutes(); }

export function minToStr(m) { m = ((m % 1440) + 1440) % 1440; return pad(Math.floor(m / 60)) + ':' + pad(m % 60); }

export function minOfDay(ms) { var d = new Date(ms); return d.getHours() * 60 + d.getMinutes(); }

export function todayKey() { return fmtDate(new Date()); }

/* ---------------- 后台通知登记 ---------------- */
export function dateAt(min) { var d = new Date(); d.setHours(Math.floor(min / 60), min % 60, 0, 0); return d; }

export function copyText(text) {
  if (navigator.clipboard && navigator.clipboard.writeText) {
    return navigator.clipboard.writeText(text).then(
      function () { return true; },
      function () { return legacyCopy(); }
    );
  }
  return Promise.resolve(legacyCopy());
}

/* 剪贴板权限在各 WebView 上差异很大，兜底用「全选 + execCommand」 */
export function legacyCopy() {
  var ta = $('#dataArea');
  if (!ta) return false;
  var ro = ta.readOnly;
  ta.readOnly = false;
  ta.focus();
  ta.select();
  var ok = false;
  try { ok = document.execCommand('copy'); } catch (e) { ok = false; }
  ta.readOnly = ro;
  try { ta.setSelectionRange(0, 0); } catch (e) { /* ignore */ }
  return ok;
}

/* ---------------- esc ---------------- */
export function esc(s) {
  return String(s).replace(/[&<>"']/g, function (c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
  });
}
