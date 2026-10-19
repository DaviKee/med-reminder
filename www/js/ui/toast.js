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
import { minimizeApp } from '../platform/lifecycle.js';

/* ---------------- toast ---------------- */
export var toastTimer = null;

export function toast(msg) {
  var el = $('#toast');
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(function () { el.classList.remove('show'); }, 2000);
}

/* ---------------- 打卡成功反馈（2026-10-18；2026-10-20 改「模糊 + 圆扩散 + 缩后台」） ----------------
 * 与 toast 刻意分开的两个理由：
 *   ① toast 是"信息"，位置在底部、2 秒、可被下一条覆盖；
 *      打卡反馈是"确认"，必须居中、盖住整屏、只出现一次 —— 两者语义不同；
 *   ② 老人打卡后最常见的动作是**再点一次**确认到底记上没有。
 *      一个居中的大勾能在 0.3 秒内回答这个问题，而底部小字做不到。
 *
 * 2026-10-20 秦老师要求：背景直接模糊；点完主色圆扩散到全屏，顺带把 App 缩到后台。
 * 时序（与 components.css 的 cpPop / cpBurst 严格对齐，改一边必须改另一边）：
 *   0ms     .show        背景 10px 模糊 + 大勾弹入（300ms）
 *   380ms   .burst       主色圆放大到盖满全屏（420ms），勾号先淡出
 *   800ms   minimizeApp  扩散刚好铺满时缩到后台 —— 用户看到的是"圆把界面收走了"
 *   1600ms  清 class     回前台时干干净净，不会残留一层遮罩
 *
 * ⚠️ 五个调用点全在 photoGate() 回调里（拍照流程**结束之后**）才触发，
 *    所以最小化不会打断相机 Activity；将来若有调用点会在相机前触发，必须传 {minimize:false}。
 * ⚠️ 浏览器 / 无 App 插件时 minimizeApp 返回 false —— 走完整动画后自行淡出，不报错。
 * ⚠️ pointer-events:none 是硬要求：这层绝不能挡住任何东西。 */
export var popTimer = null;
var burstTimer = null, miniTimer = null;

function reducedMotion() {
  try {
    return !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  } catch (e) { return false; }
}

export function takenPop(text, opts) {
  var el = $('#checkpop');
  if (!el) return;
  var o = opts || {};
  var t = $('#checkpopTxt');
  if (t) t.textContent = text || '已记录';

  clearTimeout(popTimer); clearTimeout(burstTimer); clearTimeout(miniTimer);
  el.classList.remove('show', 'burst');
  void el.offsetWidth;          // 强制回流 → 保证动画能重播
  el.classList.add('show');

  var wantMin = o.minimize !== false;

  /* 关掉动效：不扩散（CSS 也把 animation 关了），只留 260ms 让大勾露个面就缩后台 */
  if (reducedMotion()) {
    if (wantMin) miniTimer = setTimeout(function () { minimizeApp(); }, 260);
    popTimer = setTimeout(function () { el.classList.remove('show', 'burst'); }, 700);
    return;
  }

  burstTimer = setTimeout(function () { el.classList.add('burst'); }, 380);
  if (wantMin) miniTimer = setTimeout(function () { minimizeApp(); }, 800);
  popTimer = setTimeout(function () { el.classList.remove('show', 'burst'); }, 1600);
}
