/* ui/overlay.js —— 浮层与返回键的 DOM 原语 + 判定
 *
 * 2026-09-24 架构重构：从 app.js 抽出。函数体一字未改，只去掉一层缩进并加 export。
 *
 * 依赖方向：**只依赖 core/util**（`$` / `$$`）—— 不依赖 store / schedule / app。
 * 这是第一个 `ui/` 模块：它管「浮层怎么开合、焦点还给谁」，不认识任何业务。
 *
 * ⚠️ 4 个状态（openCount / lastFocus / confirmCb / sheetSaveDone）**只由本模块写**。
 *    import 的绑定是只读的，外部不能赋值 —— 要改状态请走文件末尾那组小 API
 *    （isOverlayOpen / enterOverlay / leaveOverlay / rememberFocus / restoreFocus /
 *      clearConfirmCb / takeConfirmCb）。这样上层拿到的是语义，而不是裸变量。
 *
 * ⚠️ 留在 app.js 的三个「适配器」：openSheet / closeSheet（要写编辑态 + 调 renderPreview）、
 *    closeTopDialog（要清 pendingShot）、handleBackButton（要读 currentTab + 调 setTab）。
 *    它们是「DOM 状态 ↔ app 状态」的桥，本来就该在上层；纯判定 backAction 已在本模块。
 */

import { $, $$ } from '../core/util.js';

/* ---------------- overlays ---------------- */
export var openCount = 0;

export function setScrim(on) { $('#scrim').classList.toggle('show', on); }

/* ---------------- 浮层「保存」防重入 ----------------
 * 慢设备（或开了双击缩放）上连点「保存」会派发多次 click，而**每一次点击都是一次完整的
 * 「新增药品」**（`S.meds.push`）。症状：一长串同名同刻的药品 —— 2026-09-24 真机遇到 7 条。
 * 判据用「本次浮层会话是否已经保存过」，而不是「函数当前是否在执行」：
 * 后者挡不住"排队之后才派发"的重复点击（连点的关键就在这里）。 */
export var sheetSaveDone = false;

export function sheetSaveBegin() { if (sheetSaveDone) return false; sheetSaveDone = true; return true; }

export function sheetSaveReset() { sheetSaveDone = false; }

/* ---------------- 浮层焦点管理（A-3） ----------------
 * 键盘 / 读屏 / 开关设备用户需要三件事：
 *   ① 打开浮层时焦点跟着进去（否则 Tab 还在背后的页面上跑）
 *   ② Tab 不跑出浮层（焦点陷阱）
 *   ③ 关闭后焦点回到打开它的那个元素（否则"掉"到页面开头，得重新找位置）
 * 之前只有 role="dialog" + Esc，缺这三条。 */
export var lastFocus = null;

/* ---------------- 通用二次确认（破坏性操作专用） ----------------
 * 删药品、清记录这类动作不可逆，必须先问一遍再执行。 */
export var confirmCb = null;

export function askConfirm(title, body, okText, cb) {
  confirmCb = cb || null;
  $('#confirmTitle').textContent = title;
  $('#confirmBody').textContent = body;
  $('#confirmOk').textContent = okText || '确认';
  openDlg($('#dlgConfirm'));
}

export function focusables(root) {
  var sel = 'a[href],button:not([disabled]),input:not([disabled]),select:not([disabled]),'
    + 'textarea:not([disabled]),[tabindex]:not([tabindex="-1"])';
  return $$(sel, root).filter(function (el) {
    return !el.classList.contains('hidden') && el.offsetParent !== null;
  });
}

export function openDlg(el) {
  if (!el) return;
  rememberFocus();
  el.classList.add('show');
  enterOverlay();
  /* 焦点移进浮层：优先第一个可聚焦元素；没有就把浮层本身设为可聚焦再聚焦 */
  var f = focusables(el);
  if (f.length) { try { f[0].focus(); } catch (e) { /* ignore */ } }
  else {
    el.setAttribute('tabindex', '-1');
    try { el.focus(); } catch (e) { /* ignore */ }
  }
}

export function closeDlg(el) {
  if (!el) return;
  el.classList.remove('show');
  leaveOverlay();
  restoreFocus();                   /* 焦点还给触发它的元素 */
}

/* ---------------- Android 返回键 / 侧滑返回 ----------------
 * 此前**完全没有处理返回键**（全项目搜不到一处 `App.addListener('backButton')`）。
 * 表现：侧滑返回没反应；浮层开着时更出不来，只能杀掉 App 重开（2026-09-24 反馈）。
 *
 * 优先级（按用户直觉）：关浮层 → 切回「今天」→ 退出 App。
 * ⚠️ 提醒弹窗（dlgRemind）与确认框（dlgConfirm）**不许**被返回键抹掉 ——
 *    前者要求明确选「已服用 / 稍后」，后者要用户明确回答；
 *    此时**吃掉这次返回**（既不关也不退出），否则误按一次就直接退出 App。
 *
 * 判定抽成纯函数 `backAction(s)`，便于单测（DOM 状态由调用方拍好再传进来）。 */
export function backAction(s) {
  if (s.closableDlg) return 'close-dlg';
  if (s.sheetOpen) return 'close-sheet';
  if (s.anyDlg) return 'consume';
  if (s.tab !== 'today') return 'go-today';
  return 'exit';
}

export function exitApp() {
  var A = window.Capacitor && window.Capacitor.Plugins && window.Capacitor.Plugins.App;
  if (A && A.exitApp) { A.exitApp(); return; }
  if (navigator.app && navigator.app.exitApp) { navigator.app.exitApp(); return; }   // 老 Cordova
  /* 浏览器 / PWA 没有「退出」这个概念，什么都不做 */
}

/* 可被「一键关掉」的浮层。提醒弹窗与确认框不在其中 —— 见 backAction 的注释。 */
export function closableDialogs() {
  return $$('.dlg-wrap.show').filter(function (el) {
    return el.id === 'dlgData' || el.id === 'dlgSkip' || el.id === 'dlgClean'
      || el.id === 'dlgView' || el.id === 'dlgPhoto' || el.id === 'dlgHistory';
  });
}

/* Tab 焦点陷阱。返回 true 表示已处理（调用方不用再管）。
 * 只在有浮层显示时生效 —— 没有浮层时保持浏览器默认行为。 */
export function trapTab(ev) {
  var shown = $$('.dlg-wrap.show').concat($$('.sheet.show'));
  if (!shown.length) return false;
  var top = shown[shown.length - 1];
  var f = focusables(top);
  if (!f.length) return false;
  var first = f[0], last = f[f.length - 1];
  var act = document.activeElement;
  var inside = top.contains(act);
  if (ev.shiftKey && (act === first || !inside)) { ev.preventDefault(); try { last.focus(); } catch (e) {} return true; }
  if (!ev.shiftKey && (act === last || !inside)) { ev.preventDefault(); try { first.focus(); } catch (e) {} return true; }
  return false;
}


/* ---------------- 给上层的小 API（本模块状态只由本模块写） ----------------
 * 上面那 4 个 export var 全是**活绑定**：外部 `import` 后读到的永远是最新值，
 * 但**不能赋值**（import 绑定只读，直接 `X = ...` 会 TypeError）。
 * 所以把「写」包成带语义的函数给上层用。 */
export function isOverlayOpen() { return openCount > 0; }
/* sheet 的开关自己管计数（dialog 的计数在 openDlg / closeDlg 内部） */
export function enterOverlay() { openCount++; }
export function leaveOverlay() { openCount = 0; }
export function rememberFocus() { lastFocus = document.activeElement || null; }
export function restoreFocus() {
  if (lastFocus && document.contains && document.contains(lastFocus)) {
    try { lastFocus.focus(); } catch (e) { /* ignore */ }
  }
  lastFocus = null;
}
/* 二次确认的待执行回调：取出即清（避免重复执行）；取消则只清不取 */
export function takeConfirmCb() { var cb = confirmCb; confirmCb = null; return cb; }
export function clearConfirmCb() { confirmCb = null; }
