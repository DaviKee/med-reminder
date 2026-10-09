/* platform/lifecycle.js —— 平台生命周期与系统事件
 *
 * 2026-10-01 架构重构（多平台准备）：从 `app.js` 与 `ui/overlay.js` 抽出。
 *
 * 为什么要单独一层：这些是**纯平台语义** ——
 *   · Android 有物理返回键 / 侧滑返回
 *   · 相机是**独立 Activity**，被系统回收后由 Capacitor 回放结果
 *   · 前后台切换由系统广播
 * 它们原本住在业务层（`app.js` 的 boot / `ui/overlay.js`），
 * **加一个平台就要在业务代码里动刀** —— 正是工程纪律 §4.1 要避免的事。
 *
 * 契约（见同目录 README.md）：
 *   ① 本文件是 `window.Capacitor` 的取用点之一（另一处是各能力模块）
 *   ② **不 import `ui/` 或 `core/`** —— 需要业务动作时走**回调参数**（由上层注入）
 *   ③ 插件不存在时**安全返回**（浏览器 / PWA 下不能抛错）
 *
 * ⚠️ 返回键的**判定逻辑**（关浮层 → 回今天 → 退出）**不在本文件** ——
 *    它是纯函数 `backAction()`，仍住在 `ui/overlay.js`（与 DOM 无关、可单测）。
 *    本文件只负责「把系统事件接上」。
 */

/* 取原生 App 插件。
 * ⚠️ 用「插件对象是否存在」判可用性 —— **不要**用 `Capacitor.isNativePlatform()`
 *    （它恒为 true，已踩过这个坑，见 README 契约 ②）。 */
function capApp() {
  var C = window.Capacitor;
  return (C && C.Plugins && C.Plugins.App) || null;
}

/* 退出 App —— 返回键在「今天」页再按一次时走这里。 */
export function exitApp() {
  var A = capApp();
  if (A && A.exitApp) { A.exitApp(); return; }
  if (navigator.app && navigator.app.exitApp) { navigator.app.exitApp(); return; }   // 老 Cordova
  /* 浏览器 / PWA 没有「退出」这个概念，什么都不做 */
}

/* 注册「返回键 / 侧滑返回」回调。返回 true 表示确实注册上了。
 * 判定逻辑由上层提供（`handleBackButton`，内部调 `ui/overlay.js` 的 `backAction`）。 */
export function onBackButton(cb) {
  var A = capApp();
  if (!A || !A.addListener) return false;
  A.addListener('backButton', function () { cb(); });
  return true;
}

/* 相机 Activity 被系统杀掉后，Capacitor 会把结果**回放**给本 App。
 *
 * ⚠️ 官方明确要求监听这个事件 —— 不处理会**同时丢照片和打卡**（2026-09-29 真机反馈）。
 * 只关心 Camera 插件的成功结果；回调收到照片路径。 */
export function onCameraRestored(cb) {
  var A = capApp();
  if (!A || !A.addListener) return false;
  A.addListener('appRestoredResult', function (d) {
    if (!d || d.pluginId !== 'Camera' || !d.success) return;
    var photo = d.data || {};
    if (photo.path) cb(photo.path);
  });
  return true;
}

/* 前后台切换。回调收到 `isActive`（true = 回到前台）。
 *
 * ⚠️ 上层在「回到前台」分支里**必须补一次跨天处理**（`rollDayIfNeeded`）——
 *    后台时 JS 定时器被系统暂停，过夜后回到前台是第一现场，
 *    不补就会出现"今天的药被顺延成间隔逻辑"（2026-09-28 真机 bug）。 */
export function onAppStateChange(cb) {
  var A = capApp();
  if (!A || !A.addListener) return false;
  A.addListener('appStateChange', function (st) { cb(!!(st && st.isActive)); });
  return true;
}

/* ---- 纯净状态栏（2026-10-09 小米 13） ----
 * App 背景铺到状态栏底下（edge-to-edge）之后，状态栏图标必须与背景对比：
 * 浅色主题 → 深色图标（dark=true），深色主题 → 白色图标（dark=false）。
 * 原生侧由本项目 AppSettingsPlugin.setStatusBarIcons 提供（官方没有这能力）。
 *
 * ⚠️ 调用方（ui/theme.js）负责只在「真铺进去了」的设备上调 —— 判据是
 *    MainActivity 注入的 --sat > 0；旧设备状态栏是黑底白字，不能跟着切。
 * 浏览器 / PWA 无此插件，安全返回 false。 */
export function setStatusBarIcons(dark) {
  var C = window.Capacitor;
  var P = C && C.Plugins && C.Plugins.AppSettings;
  if (P && typeof P.setStatusBarIcons === 'function') {
    try { P.setStatusBarIcons({ dark: !!dark }); return true; } catch (e) { return false; }
  }
  return false;
}
