/* ui/permission.js —— 通知权限：探测 / 提示卡 / 授权申请 / 诊断
 *
 * 2026-10-04 架构重构（A-2）：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * **这一组为什么值得单独成模块**：它承载的是 2026-09-24 那条真机反馈的完整结论 ——
 * 「`requestPermissions()` 的返回值**不能**当成败判据」（SDK < 33 时它根本不弹框，
 * 直接 resolve 当前状态）。连同 `permProbe` 的"签名没变就不重绘"优化，
 * 是一整块**独立于业务**的平台交互逻辑。
 *
 * 末尾另有三个对外的「写入口」—— 本模块的状态只由本模块写。
 */
import { esc } from '../core/util.js';
import { readJSON, writeJSON } from '../core/store.js';
import { isOverlayOpen } from './overlay.js';
import { render } from './render.js';
import { toast } from './toast.js';

export var notifyPerm = 'unknown';   // granted / denied / unsupported / unknown

/* 权限卡点击：**先真正申请一次，拿不到再跳系统设置**。
 *
 * 上一版写的是「denied 就直接跳设置，不再申请」—— 那是错的。
 * Android 官方文档说用户在弹窗选「不允许」与「在系统设置里手动关闭」**效果相似**，
 * 但**再请求时是否弹窗并不等价**：手动关闭相当于 revoke，多数设备上重新申请
 * 仍会弹出授权框。上一版把这条路砍掉了，等于白白丢掉唯一一次能直接授权的机会
 * （秦老师真机反馈：「双清后第一次能授权，手动关闭后再点就拿不到权限」）。
 *
 * 现在的行为：能弹窗的设备一次点击就授权成功；系统性不再弹窗（或用户再次拒绝）时，
 * 自动落到系统设置页 —— 那是唯一 100% 有效的路径。两条路都走通。
 * 抽成具名函数而非内联，是为了这段分支能被测试直接覆盖。 */
/* 收不到提醒的具体原因 —— 显示出来，用户才知道该去关哪一层开关 */
export function permBlockReason(p) {
  if (!p) return '读取不到状态';
  if (p.display === 'denied') return '权限被拒';
  if (p.enabled === false) return 'App 通知开关已关';
  if (p.channelImportance === 0) return '「服药提醒」渠道被关闭';
  return '收不到提醒';
}

/* 点击权限卡。
 *
 * ⚠️ **不能用 requestPermissions() 的返回值判断成败** —— 这是本轮反馈的根因。
 * 插件源码（LocalNotificationsPlugin.requestPermissions）：
 *     if (SDK_INT < TIRAMISU || getPermissionState(...) == GRANTED) {
 *         resolve(当前状态);          // ← 不弹任何框
 *     } else { requestPermissionForAlias(...); }
 * 即 **Android 13 以下永远不弹授权框**；13 以上如果权限已是 granted（只是被关掉了
 * 总开关或本渠道）也不弹。上一版按返回值走：拿到 granted 就宣布「通知已开启」并
 * 撤下卡片 —— 于是用户看到的就是「点了没反应，也不跳设置」，而问题其实还在。
 *
 * 现在每一步都回到**真实状态**复检（probe + isBlocked）：
 *   本就能收到 → 直接说明，不做多余动作
 *   确实收不到 → 先申请一次（能弹框的设备在这一步就解决了）
 *   仍然收不到 → 跳系统设置（唯一 100% 有效的路径）
 *   跳不过去   → 明确写出失败原因与手动路径，绝不「点了没反应」
 */
export function onPermCardTap() {
  if (!(window.MedNotify && window.MedNotify.native)) {
    permAction = '浏览器模式：请在浏览器的网站权限里改通知';
    render();
    toast('浏览器模式无法跳系统设置，请在网站权限里把通知改为「允许」');
    askNotify();
    return;
  }
  permAction = '检测中…';
  render();

  window.MedNotify.probe().then(function (before) {
    if (!window.MedNotify.isBlocked(before)) {
      permProbe = before;
      notifyPerm = before.display === 'granted' ? 'granted' : 'unknown';
      permAction = '复检：本就能收到提醒，无需处理';
      render();
      toast('通知本来就是开着的');
      return null;
    }
    permAction = '确认收不到（' + permBlockReason(before) + '）→ 先申请授权';
    render();

    return window.MedNotify.requestPermission().then(function (p) {
      return window.MedNotify.probe().then(function (after) {
        if (!window.MedNotify.isBlocked(after)) {
          permProbe = after;
          lastProbeSig = JSON.stringify(after);
          notifyPerm = 'granted';
          permAction = '申请成功 · 已授权（' + p + '）';
          render();
          toast('通知已开启');
          return null;
        }
        permAction = '系统未弹授权框（Android 13 以下不会弹）→ 跳系统设置';
        render();
        return window.MedNotify.openSettings().then(function (r) {
          permAction = r.ok ? '已打开系统设置，请把「通知」打开' : ('跳设置失败：' + r.reason);
          render();
          toast(r.ok
            ? '请在系统设置里打开「通知」'
            : '请到「设置 → 应用 → 定时服药提醒 → 通知」手动打开');
        });
      });
    });
  }).catch(function (e) {
    permAction = '出错：' + ((e && e.message) ? e.message : e);
    render();
  });
}

export function askNotify() {
  if (window.MedNotify && window.MedNotify.native) {
    window.MedNotify.requestPermission().then(function (p) { notifyPerm = p; render(); });
    return;
  }
  // 浏览器：只有 default 时才允许弹授权框，denied 时浏览器会静默忽略，
  // 此时不刷新权限也不假报成功，让权限卡继续显示引导用户手动改。
  try {
    if ('Notification' in window && Notification.permission === 'default') {
      Notification.requestPermission().then(function () { notifyPerm = browserPerm(); render(); });
    } else {
      notifyPerm = browserPerm(); render();
    }
  } catch (e) { /* ignore */ }
}

/* ---------------- render dispatcher ---------------- */
/* ---------------- 通知权限状态 ----------------
 * notifyPerm 取值：granted / denied / prompt(未申请，浏览器) / unsupported / unknown
 * 原生：来自 LocalNotifications.checkPermissions()
 * 浏览器：来自 Notification.permission（default → prompt）
 * 只有 denied 才是「用户已经拒绝」，需要引导去系统/浏览器设置里改。 */
export function permCardHtml() {
  var isNative = !!(window.MedNotify && window.MedNotify.native);

  // 浏览器不支持通知：页面必须开着才提醒，说清楚，别让用户以为装完就万事大吉
  if (!isNative && notifyPerm === 'unsupported') {
    return '<div class="card" style="display:flex;flex-direction:column;gap:6px;border-color:#8A8F98">'
      + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:var(--sub)">当前环境不支持通知</span>'
      + '<span class="meta">提醒只在 App 页面前台时弹出，切走或锁屏不会响。装到手机上才能锁屏提醒。</span></div>';
  }

  // 未申请：不吓唬用户，只在浏览器模式提示，点了才发起授权
  if (!isNative && notifyPerm === 'prompt') {
    return '<button class="card" id="btnPerm" style="display:flex;flex-direction:column;gap:6px;width:100%;border-color:#FF7A17;cursor:pointer;-webkit-tap-highlight-color:transparent">'
      + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:#FF7A17">还没开启通知</span>'
      + '<span class="meta">开启后才能及时收到服药提醒，点这里授权。</span></button>';
  }

  // 权限不可用：原生下点卡片先重新申请，拿不到再跳系统设置页；浏览器只能让用户去浏览器设置里改
  if (notifyPerm === 'denied') {
    var hint = isNative
      ? '锁屏和息屏时收不到服药提醒。点这里重新申请；若系统不再弹授权框，会自动打开系统设置页，请在那里把「通知」打开。'
      : '浏览器已拒绝通知，需要到浏览器的网站权限设置里把通知改回「允许」，然后刷新页面。';
    return '<button class="card" id="btnPerm" style="display:flex;flex-direction:column;gap:6px;width:100%;border-color:#FF7A17;cursor:pointer;-webkit-tap-highlight-color:transparent">'
      + '<span style="font-size:calc(14px * var(--fs));line-height:calc(20px * var(--fs));color:#FF7A17">通知未开启 · 收不到提醒</span>'
      + '<span class="meta">' + hint + '</span></button>';
  }

  return '';
}

/* 浏览器模式下把 Notification.permission 映射成与原生一致的取值。
 * 统一从 window 上取，避免 "检查 window、读取全局" 的不一致。 */
export function browserPerm() {
  try {
    var N = window.Notification || (typeof Notification !== 'undefined' ? Notification : null);
    if (!N) return 'unsupported';
    var p = N.permission;
    if (p === 'granted') return 'granted';
    if (p === 'denied') return 'denied';
    return 'prompt';   // default
  } catch (e) { return 'unsupported'; }
}

/* 最近一次原生探测结果。既用于判定，也用于 DEBUG 卡自检展示 ——
 * 真机验收时「看不到权限卡」若只靠猜，会来回折腾好几轮。 */
export var permProbe = null;

/* 通知的两项计数（系统排程 / 通知栏显示）。用于诊断「一次冒出一堆通知」。 */
export var notifyStat = null;

export var lastProbeSig = '';

/* 权限卡点击后走过的步骤。权限跳转是唯一没法远程验证的环节 ——
 * 把结果直接显示在 DEBUG 卡上，比来回猜快得多。 */
export var permAction = '';

export function refreshPerm() {
  if (window.MedNotify && window.MedNotify.native) {
    window.MedNotify.probe().then(function (p) {
      /* 顺带取一次通知计数。它不进权限判定，只用于诊断显示 ——
       * 但「一次冒出来一堆通知」这种问题，没有数字就只能靠猜。 */
      var st = window.MedNotify.stat ? window.MedNotify.stat() : Promise.resolve(null);
      return st.then(function (ns) { return { p: p, ns: ns }; });
    }).then(function (r) {
      var p = r.p, ns = r.ns;
      var sig = JSON.stringify(p) + '|' + JSON.stringify(ns);
      // 只在探测结果真的变化时重绘。每 5 秒无条件 render() 会重建视图 DOM，
      // 打断用户滚动与输入（innerHTML 一换，滚动位置就回顶）。
      if (sig === lastProbeSig) return;
      lastProbeSig = sig;
      permProbe = p;
      notifyStat = ns;

      var next;
      if (window.MedNotify.isBlocked(p)) next = 'denied';
      else if (p.display === 'granted') next = 'granted';
      else next = 'unknown';

      if (next !== notifyPerm) notifyPerm = next;
      /* ⚠️ 有浮层开着时**不要**重绘：上面那句 render() 会把三个视图的 innerHTML
       * 整个重建，正在输入/正在点的元素会被换掉（"点了没反应"的主要来源之一）。
       * 代价：权限卡可能晚一点才更新 —— 而浮层盖着时本来就看不见它，
       * 之后任何一次操作都会重绘。 */
      if (isOverlayOpen()) return;
      render();
    });
    return;
  }
  var b = browserPerm();
  if (b !== notifyPerm) { notifyPerm = b; render(); }
}

/* DEBUG 卡的自检行：把原生探测到的原始值直接显示出来。
 * 「看不到权限卡」若只靠猜，会来回折腾好几轮；显示实际读数可以一次定位。 */
/* 已注册的 Capacitor 插件清单。
 * 加这一行是因为踩过一个很隐蔽的坑：项目没有打包器，插件必须靠 <script> 引入
 * 才会注册进 Capacitor.Plugins；少了这一步，判断条件能通过、调用却失败，
 * 表现成「功能没反应」而且没有任何报错。有了这行，一眼就能看出插件在不在。 */
export function plugLine() {
  var c = window.Capacitor;
  var names = (c && c.Plugins) ? Object.keys(c.Plugins).sort() : [];
  var heads = (window.MedNotify && window.MedNotify.bridgeHeaders) ? window.MedNotify.bridgeHeaders() : 0;
  var st = (window.MedNotify && window.MedNotify.settingsState) ? window.MedNotify.settingsState() : 'missing';
  var stTxt = st === 'injected' ? '已注入' : (st === 'proxy' ? '代理兜底可用' : '不可用');
  var ua = (window.navigator && window.navigator.userAgent) || '';
  var m = /Android\s+([0-9.]+)/.exec(ua);
  var sys = '未知';
  if (m) {
    var major = parseInt(m[1], 10) || 0;
    sys = 'Android ' + m[1] + (major >= 13 ? '（可弹授权框）' : '（不弹授权框，只能跳设置）');
  }
  var ph = (window.MedPhoto && window.MedPhoto.hashCount) ? window.MedPhoto.hashCount() : 0;
  return '<p class="hint">已注册插件：'
    + (names.length ? esc(names.join(' / ')) : '无 —— 插件 JS 未加载')
    + '</p>'
    + '<p class="hint">照片校验：已登记 ' + ph + ' 个指纹（用于识别重复照片）</p>'
    + '<p class="hint">原生桥接：' + (heads ? ('已注入 ' + heads + ' 个插件头') : '未注入 —— 原生调用会失败')
    + ' · 跳设置插件：' + stTxt + '</p>'
    + '<p class="hint">系统：' + esc(sys) + '</p>';
}

export function diagHtml() {
  if (!(window.MedNotify && window.MedNotify.native)) {
    return '<p class="hint">通知模式：浏览器（无系统闹钟，页面关掉就不响）</p>' + plugLine();
  }
  var p = permProbe;
  if (!p) return '<p class="hint">通知状态：读取中…</p>' + plugLine();
  return '<p class="hint">通知状态：权限 ' + esc(p.display)
    + ' · App 开关 ' + (p.enabled === null ? '读不到' : (p.enabled ? '开' : '关'))
    + ' · 渠道 ' + (p.channelFound ? ('importance ' + p.channelImportance) : '未创建')
    + '</p>'
    + (permAction ? '<p class="hint">上次点击权限卡：' + esc(permAction) + '</p>' : '')
    + (notifyStat
        ? '<p class="hint">系统排程：' + (notifyStat.pending == null ? '读不到' : notifyStat.pending + ' 条')
          + ' · 通知栏显示：' + (notifyStat.delivered == null ? '读不到' : notifyStat.delivered + ' 条')
          + (notifyStat.test ? '（含 1 条测试）' : '') + '</p>'
        : '')
    + '<button class="btn btn-ghost" id="btnPurgeNotif" style="align-self:flex-start;margin-top:2px">清除全部提醒通知</button>'
    + '<p class="hint">点它会取消系统里所有已登记的提醒，并按今天的排程重新登记一遍。'
    + '通知栏里堆了重复的、或者早就过期的提醒时用它。</p>'
    + plugLine();
}

/* ---------------- 对外的「写入口」：本模块的状态只由本模块写 ----------------
 *
 * `notifyPerm` / `lastProbeSig` / `permProbe` 是 `export var` —— 外部**读**得到最新值，
 * 但 import 绑定只读。外部那几处赋值统一改成下面的语义化动作。 */

/* 让下一次探测强制刷新。
 *
 * 为什么需要：`refreshPerm` 里有一句"探测签名没变就不重绘"的优化
 * （每 5 秒无条件 render() 会重建 DOM，打断滚动与输入）。
 * 但「清空通知」「登记测试提醒」之后，**数字明明变了、签名却可能恰好一样** ——
 * 那时诊断行会停在旧数字上。清掉签名缓存，逼它重算一次。
 *
 * ⚠️ 原调用点还有一句 `permProbe = permProbe || null;` —— 那是**恒等操作**
 *    （`x = x || null` 在 x 非假值时就是 x），已丢弃，无行为影响。 */
export function invalidatePermProbe() {
  lastProbeSig = null;
}

/* 浏览器模式：权限态只能从 `Notification.permission` 读（没有原生插件可问）。
 * 启动时调一次，让首屏的权限卡就有正确状态。 */
export function initBrowserPerm() {
  notifyPerm = browserPerm();
}

/* ---------------- 首次添加药品时引导授权（F-3） ----------------
 * 原来只有"打卡时"才引导，于是"设完药、还没到打卡点"这段空窗期里，
 * 用户并不知道自己收不到提醒 —— 对提醒类 App 这是最要命的静默失效。
 * 只引导一次：已 granted 不必、已 denied 交给权限卡（反复弹窗只会招人烦）。 */
export var GUIDE_KEY = 'medreminder.notifyGuide.v1';

export function guideNotified() { return readJSON(GUIDE_KEY) === '1'; }

export function markGuided() { writeJSON(GUIDE_KEY, '1'); }

export function guideNotifyOnce() {
  if (guideNotified()) return;
  if (notifyPerm === 'granted') return;      // 已经能收到，别打扰
  if (notifyPerm === 'denied') return;       // 已明确收不到 → 用权限卡引导（那里还能跳系统设置）
  markGuided();
  askNotify();
  toast('顺手把通知权限开一下，锁屏时才能收到提醒');
}
