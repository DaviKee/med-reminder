/* ui/render.js —— 重绘调度（订阅式）
 *
 * 为什么要它
 * ----------
 * app.js 里曾有 **55 处**直接调 `render()`，而 `render()` 是把三个视图的
 * innerHTML **整个重建一遍**。两个问题：
 *   ① 慢 —— 只改了「今天」也要连「药品」「记录」一起重建
 *   ② **不安全** —— 重建会把用户正在点的元素换掉，click 落在已脱离文档的节点上，
 *      表现为「点了没反应」（v1.4.3 用事件委托绕过了它，但根因一直在）
 *
 * 但**更要命的是第三条**：它是**拆视图的前置**。
 *   `ui/today.js` 这种模块改了数据后需要"重绘一下"，可 `render` 住在上层 app.js 里
 *   —— 下层 import 上层就破了依赖方向铁律。所以把"请重绘"这件事抽成**底层的通知**：
 *   视图模块 import 这个模块（无依赖），启动方负责把真正的渲染函数注册进来。
 *
 * 与 `core/store.js` 的 `saveHooks` 是**同一个套路**：底层定义入口 + 上层注入实现。
 *
 * ⚠️ **必须保持同步语义**：`markDirty()` 返回时 DOM 已经更新了。
 *    调用方经常紧接着取刚渲染出来的元素（`$('#btnXxx')`），
 *    一旦改成 `setTimeout` 之类的异步 flush，这些地方会拿到 null。
 */
var subs = [];           // [{ view, fn }] —— 注册进来的渲染器
var dirty = {};          // view → true，等待重绘
var registered = false;

/* 由启动方（app.js）调用一次，注入三个视图的渲染函数。
 * 在注册之前发生的 markDirty 不会丢 —— dirty 标记留着，注册完统一 flush。 */
export function setRenderers(map) {
  subs = Object.keys(map).map(function (v) { return { view: v, fn: map[v] }; });
  registered = true;
  flush();
}

/* 声明"这块变了"。不传参数 = 三个视图全部标脏（等价于旧的全量 render）。
 *
 * 现阶段 `render()` 就是这个函数的别名，所以 55 处老调用点**一行没改**；
 * 将来要细粒度化，只需把热点处改成 `markDirty('today')`，其余保持不变。 */
export function markDirty(view) {
  if (view) {
    dirty[view] = true;
  } else {
    subs.forEach(function (s) { dirty[s.view] = true; });
  }
  flush();
}

/* 重绘标脏的视图。**同步** —— 见文件头部的警告。 */
function flush() {
  if (!registered) return;
  subs.forEach(function (s) {
    if (!dirty[s.view]) return;
    dirty[s.view] = false;
    s.fn();
  });
}
