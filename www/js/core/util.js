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

/* ---------------- 日期工具（S-3 服用周期用） ---------------- */

/* 'YYYY-MM-DD' → 0(周一) .. 6(周日)。
 * 约定与「记录」页的 week dots 一致（那边写 `(d.getDay()+6)%7`）—— 0 是周一，不是周日。
 * ⚠️ 这个偏移量很反直觉，改动时两个地方要一起改。 */
export function dowOf(key) {
  var p = String(key || '').split('-');
  var d = new Date(Number(p[0]), Number(p[1]) - 1, Number(p[2]));
  if (isNaN(d.getTime())) return -1;          // 非法输入 → -1，调用方据此判为"不匹配"
  return (d.getDay() + 6) % 7;
}

/* 两个日期键相差几天（b - a）。可负。
 *
 * ⚠️ 必须用 `new Date(y, m-1, d)`（**本地时区**）构造。
 *    直接 `new Date('2026-10-04')` 是按 **UTC** 解析的 —— 东八区会整体差 8 小时，
 *    跨月/跨年时就会算出「差一天」，而这种错**只在部分日期出现**，极难查。
 *
 * 用 round 而不是 floor：夏令时切换那天是 23 或 25 小时，
 * floor 会把它算成"差 0 天"。round 对 1 小时级的偏差免疫。 */
export function dayDiff(a, b) {
  var pa = String(a || '').split('-'), pb = String(b || '').split('-');
  var da = new Date(Number(pa[0]), Number(pa[1]) - 1, Number(pa[2]));
  var db = new Date(Number(pb[0]), Number(pb[1]) - 1, Number(pb[2]));
  if (isNaN(da.getTime()) || isNaN(db.getTime())) return NaN;
  return Math.round((db.getTime() - da.getTime()) / 86400000);
}

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
