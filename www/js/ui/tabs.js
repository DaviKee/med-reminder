/* ui/tabs.js —— 选项卡切换：setTab + currentTab（下一步把 swipe 也并进来）
 *
 * 2026-10-04 架构重构（A-2）：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * `currentTab` 用 **`export var`（活绑定）**：app.js 有两处**只读**它
 * （backAction 的状态参数、swipeTarget 的入参），活绑定让那两处一行都不用改。
 * 写点只在 `setTab` 内部 —— 这也是它能整体搬走的原因
 * （对比 `lastDay`：那边有外部写点，得配 `markToday()`）。
 */
import { $, $$ } from '../core/util.js';

/* ---------------- tabs ---------------- */
export function setTab(name) {
  $$('.tab').forEach(function (t) { t.classList.toggle('active', t.getAttribute('data-tab') === name); });
  $$('.view').forEach(function (v) { v.classList.toggle('active', v.getAttribute('data-view') === name); });
  $('#main').scrollTop = 0;
  currentTab = name;
}

export var currentTab = 'today';
