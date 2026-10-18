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

/* ---------------- 打卡成功反馈（2026-10-18） ----------------
 * 与 toast 刻意分开的两个理由：
 *   ① toast 是"信息"，位置在底部、2 秒、可被下一条覆盖；
 *      打卡反馈是"确认"，必须居中、盖住整屏、只出现一次 —— 两者语义不同；
 *   ② 老人打卡后最常见的动作是**再点一次**确认到底记上没有。
 *      一个居中的大勾能在 0.3 秒内回答这个问题，而底部小字做不到。
 *
 * 实现要点：
 *   · 只切 class，动画交给 components.css 的 @keyframes popIn；
 *   · 重播前先 remove + 强制回流（void offsetWidth）—— 连续两次打卡时，
 *     不回流的话第二次加 class 不会重启动画，看起来像"没反应"；
 *   · 计时器独立于 toastTimer，避免两条消息互相 clearTimeout。
 *   · 尊重「减少动态效果」：base.css 的 reduced-motion 会把 animation-duration
 *     压到 .001ms，此时反馈层不显示也不影响任何功能（纯装饰，pointer-events:none）。 */
export var popTimer = null;

export function takenPop(text) {
  var el = $('#checkpop');
  if (!el) return;
  var t = $('#checkpopTxt');
  if (t) t.textContent = text || '已记录';
  el.classList.remove('show');
  void el.offsetWidth;          // 强制回流 → 保证动画能重播
  el.classList.add('show');
  clearTimeout(popTimer);
  popTimer = setTimeout(function () { el.classList.remove('show'); }, 1050);
}
