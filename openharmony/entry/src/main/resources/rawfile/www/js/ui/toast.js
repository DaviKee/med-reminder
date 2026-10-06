/* ui/toast.js —— 轻提示（toast）—— 被所有视图 / 业务模块共用
 *
 * 2026-10-01 架构重构（A-2）：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * ⚠️ 抽它的理由**不是"文件太长"** —— 而是 `toast()` 被几乎所有块依赖。
 *    留它在 app.js 里，任何模块化都会撞上「底层不许 import 上层」这条铁律。
 *    这类**被广泛依赖的小叶子要优先归位**；而 `render()` 正相反 ——
 *    它是**中枢**（被依赖、又依赖所有人），只能靠订阅式重绘解决，搬不动。
 */
import { $ } from '../core/util.js';

/* ---------------- toast ---------------- */
export var toastTimer = null;

export function toast(msg) {
  var el = $('#toast');
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(function () { el.classList.remove('show'); }, 2000);
}
