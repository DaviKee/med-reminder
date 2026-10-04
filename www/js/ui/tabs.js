/* ui/tabs.js —— 选项卡切换：setTab + currentTab + 切换动效
 *
 * 架构重构（A-2）：从 app.js 抽出。
 * 2026-10-04：加切换动效（页面沿方向滑入 + 白色胶囊滑动）——
 *   原来 `.view` 是 display 从 none 跳 block 的**硬切**，真机反馈"太硬"。
 *
 * `currentTab` 用 **`export var`（活绑定）**：app.js 有两处**只读**它
 * （backAction 的状态参数、swipeTarget 的入参），活绑定让那两处一行都不用改。
 * 写点只在 `setTab` 内部 —— 这也是它能整体搬走的原因
 * （对比 `lastDay`：那边有外部写点，得配 `markToday()`）。
 */
import { $, $$ } from '../core/util.js';

/* ---------------- tabs ---------------- */

/* 方向判定抽成**纯函数**（与 swipeTarget / backAction 同一套路）：
 * 不碰 DOM，判定矩阵可以直接单测，不必造触摸事件。
 *
 * 返回 null = 这一次切换不播动画。三种情况：
 *   · 首屏（from < 0：没有"上一页"可比）—— 打开 App 时滑一下是多余的
 *   · 同页（to === from：点了当前页签）
 *   · 找不到目标（to < 0：名字写错之类）
 *
 * 方向语义：`to > from` 是往右走（今日→药品），新页从**右侧**滑入，
 * 观感是内容被向左推走 —— 与「左滑切到下一个」的手势直觉一致。 */
export function slideClass(from, to) {
  if (from < 0 || to < 0 || to === from) return null;
  return to > from ? 'enter-from-right' : 'enter-from-left';
}

export function setTab(name) {
  var tabs = $$('.tab');
  var to = -1, from = -1;

  /* 顺序取自 DOM（`.tab` 的先后 = index.html 里的先后）——
   * **不在这里另写一份 ['today','meds','records']**。写两份的话，
   * 将来加第 4 个页签必然有一天对不上，而"对不上的表现"是动画方向反了，很难查。
   * （app.js 的 swipeTarget 里另有一份硬编码 TAB_ORDER —— 它是纯函数、要能单独
   *   单测，所以留着；那一份由 tests/swipe.spec.js 钉住与 DOM 一致。） */
  tabs.forEach(function (t, i) {
    var key = t.getAttribute('data-tab');
    var active = key === name;
    t.classList.toggle('active', active);
    if (active) to = i;
    if (key === currentTab) from = i;
  });

  /* 白胶囊滑到目标位置。指示器是独立的一层，这里只写 `--i` 一个数字，
   * 具体位置交给 CSS 的 calc（见 app.css 的 .tab-ind）。 */
  var ind = $('.tab-ind');
  if (ind && to >= 0) ind.style.setProperty('--i', String(to));

  $$('.view').forEach(function (v) {
    var active = v.getAttribute('data-view') === name;
    v.classList.toggle('active', active);
    /* 先摘掉上一次的入场 class —— 不摘的话，同一页再次成为目标时**不会重播**
     * （class 没变化，浏览器认为动画没变）。 */
    v.classList.remove('enter-from-right', 'enter-from-left');
    if (!active) return;
    var cls = slideClass(from, to);
    if (cls) v.classList.add(cls);
  });

  /* 切页后回到顶部。放在动画 class 之后 —— 两者互不影响（动画只改 transform/opacity）。 */
  $('#main').scrollTop = 0;
  currentTab = name;
}

export var currentTab = 'today';
