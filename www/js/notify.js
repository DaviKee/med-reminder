/* 通知层
 * 原生壳（Capacitor）：用 LocalNotifications 注册系统闹钟，App 关掉 / 锁屏 / 息屏都能响，
 *   锁屏通知上直接带「已服用」「10 分钟后」两个按钮。
 * 浏览器：没有系统闹钟能力，降级为页面内提醒 + Web Notification（页面必须开着）。
 */
(function () {
  'use strict';

  var C = window.Capacitor;
  var LN = (C && C.Plugins && C.Plugins.LocalNotifications) || null;
  var App = (C && C.Plugins && C.Plugins.App) || null;
  var NATIVE = !!LN;

  /* ⚠️ 关于这个渠道的声音（2026-09-21 修）：
   *
   * 创建渠道时**绝对不能传 sound**。插件的实现是：
   *     Uri.parse("android.resource://" + 包名 + "/raw/" + sound)
   * 也就是把 sound 当成「res/raw 下的资源名」。而我们这个 APK 里**根本没有 res/raw 目录**，
   * 于是 'default' 被拼成一个**永远解析不了**的 URI → 渠道看起来"有声音"（设置里显示「默认」），
   * 实际播放时找不到那个文件 → **全程静音**。震动是独立的 enableVibration，所以表现为
   * 「只有震动、没有声音」。
   *
   * 不传 sound → 插件不调 setSound() → 渠道使用**真正的系统默认通知音**。
   *
   * ⚠️ 注意 Android 8+ 的**渠道属性不可变**：已经创建过的渠道，改代码是改不动它的。
   * 所以对「已经装了旧版、渠道已经建坏」的设备，只有两条路：
   *   ① 用户手动到「设置 → 通知 → 服药提醒 → 声音」里指定一个铃声（**秦老师就是这么修好的**）；
   *   ② 卸载重装（渠道会重建）。
   * 我们**故意不换渠道 id** —— 换了会重建渠道，把用户手动设好的声音丢掉；
   * 而现在这个改法能保证**今后所有新建的渠道**（新装机、重装、清数据）从一开始就是对的。 */
  var CHANNEL = 'doses';
  var ACTIONS = 'DOSE_ACTIONS';
  var scheduled = [];
  var inited = false;
  var handler = null;
  var timer = null;
  var lastList = [];
  var syncBusy = false;      // doSync 串行化，见下面的说明
  var syncQueued = null;
  var bootPurge = null;      // 启动清场的 Promise；排程要等它完成

  /* 测试通知用**固定 id**。
   * 旧实现是 1 + Math.floor(Math.random() * 2000000000) —— 点几次就在系统里留几条，
   * 而且随机 id 谁也认不出来，事后没有任何办法取消它们。 */
  var TEST_ID = 990000001;

  /* 已排通知 id 的**落盘**台账。这是兜底：首选是直接问插件 getPending() 要权威清单，
   * 万一插件没这个方法（或调用失败），还得有东西可取消。 */
  var PENDING_KEY = 'medreminder.notifPending.v1';
  var pendingIds = {};
  function loadPendingIds() {
    try {
      var r = localStorage.getItem(PENDING_KEY);
      var o = r ? JSON.parse(r) : null;
      pendingIds = (o && typeof o === 'object') ? o : {};
    } catch (e) { pendingIds = {}; }
  }
  function savePendingIds() {
    try { localStorage.setItem(PENDING_KEY, JSON.stringify(pendingIds)); } catch (e) { /* ignore */ }
  }
  function markPending(ids) {
    for (var i = 0; i < ids.length; i++) pendingIds[ids[i]] = 1;
    savePendingIds();
  }
  function pendingList() {
    var out = [];
    for (var k in pendingIds) if (Object.prototype.hasOwnProperty.call(pendingIds, k)) out.push(parseInt(k, 10));
    return out;
  }
  loadPendingIds();
  function asRefs(ids) { return ids.map(function (n) { return { id: n }; }); }

  /* 字符串 doseId → Android 通知 id（必须是 32 位 int）。
   * 用持久化映射表分配自增整数 id，彻底避免哈希碰撞导致不同剂量覆盖同一条通知。 */
  var IDMAP_KEY = 'medreminder.notifIds.v1';
  var idMap = {};
  function loadIdMap() {
    try { var raw = localStorage.getItem(IDMAP_KEY); if (raw) idMap = JSON.parse(raw) || {}; }
    catch (e) { idMap = {}; }
  }
  function saveIdMap() {
    try { localStorage.setItem(IDMAP_KEY, JSON.stringify(idMap)); } catch (e) { /* ignore */ }
  }
  loadIdMap();
  function idOf(doseId) {
    if (idMap[doseId] != null) return idMap[doseId];
    var used = {}, next = 1;
    for (var k in idMap) if (Object.prototype.hasOwnProperty.call(idMap, k)) used[idMap[k]] = true;
    while (used[next]) next++;
    idMap[doseId] = next;
    saveIdMap();
    return next;
  }

  function init(cb) {
    handler = cb || handler;
    if (inited || !NATIVE) return Promise.resolve(false);
    inited = true;

    return Promise.resolve()
      .then(function () {
        /* ⚠️ **故意不传 sound**（原因见文件上方 CHANNEL 处的长注释）。
         * importance 用 5 = IMPORTANCE_HIGH：有声音 + 横幅；
         * 3 = LOW 会被系统判为静音。 */
        return LN.createChannel({
          id: CHANNEL,
          name: '服药提醒',
          description: '按你设定的间隔提醒服药',
          importance: 5,
          visibility: 1,
          vibration: true
        });
      })
      .catch(function () { /* 渠道已存在会抛错，忽略 */ })
      .then(function () {
        return LN.registerActionTypes({
          types: [{
            id: ACTIONS,
            actions: [
              { id: 'taken', title: '已服用', foreground: false },
              { id: 'snooze', title: '10 分钟后', foreground: false }
            ]
          }]
        });
      })
      .catch(function () { /* 已注册过会抛错，忽略 */ })
      .then(function () {
        LN.addListener('localNotificationActionPerformed', function (ev) {
          var doseId = ev.notification && ev.notification.extra && ev.notification.extra.doseId;
          if (!handler || !doseId) return;
          if (ev.actionId === 'taken') handler('taken', doseId);
          else if (ev.actionId === 'snooze') handler('snooze', doseId);
          else handler('open', doseId);
        });
        if (App) {
          App.addListener('appStateChange', function (st) {
            if (st.isActive && handler) handler('resume', null);
          });
          App.addListener('appUrlOpen', function () { if (handler) handler('resume', null); });
        }
        return true;
      })
      .catch(function (e) { console.warn('notify init', e); return false; });
  }

  function checkPermissions() {
    if (!NATIVE) return Promise.resolve('unsupported');
    return LN.checkPermissions().then(function (r) { return r.display; }).catch(function () { return 'unknown'; });
  }

  function requestPermission() {
    if (!NATIVE) {
      try { if ('Notification' in window && Notification.permission === 'default') Notification.requestPermission(); } catch (e) {}
      return Promise.resolve('unsupported');
    }
    return LN.requestPermissions().then(function (r) { return r.display; }).catch(function () { return 'unknown'; });
  }

  /* 取消本 App 排着的**全部**通知。
   *
   * v1.2.1 最重要的一处修复，起因是反馈「点一次测试提醒，冒出来 6 个通知」。
   *
   * 旧实现是 if (!NATIVE || !scheduled.length) return; —— scheduled 是个**内存变量**，
   * App 被系统杀掉或用户上划关闭就归零。于是**插件持久化存储里的那些通知，
   * 再也没有任何代码去清**，全成了僵尸。僵尸的下场见插件源码
   * LocalNotificationRestoreReceiver.onReceive：
   *
   *     if (at != null && at.before(new Date())) {
   *         // modify the scheduled date in order to show notifications that would
   *         // have been delivered while device was off.
   *         long newDateTime = new Date().getTime() + 15 * 1000;
   *         schedule.setAt(new Date(newDateTime));
   *     }
   *
   * 也就是说，**手机每次开机（BOOT_COMPLETED / QUICKBOOT_POWERON），插件都会把
   * 存储里所有「关机期间本该响」的通知改成「开机后 15 秒」再响一遍**。
   * 攒了几条，开机 15 秒后就一起弹几条 —— 这就是「一次冒出一堆提醒」的完整成因。
   *
   * 所以现在：**先问插件要权威清单（getPending），把存储里的全清掉**；
   * 拿不到清单时退回自建台账。两个来源都清，才算真的干净。 */
  function cancelAll() {
    if (!NATIVE) return Promise.resolve();
    scheduled = [];

    function viaStorage() {
      if (typeof LN.getPending !== 'function') return Promise.resolve(false);
      return LN.getPending()
        .then(function (r) {
          var list = (r && r.notifications) || [];
          if (!list.length) return true;
          return LN.cancel({ notifications: list.map(function (n) { return { id: n.id }; }) })
            .then(function () { return true; })
            .catch(function () { return false; });
        })
        .catch(function () { return false; });
    }

    return viaStorage().then(function (ok) {
      if (ok) { pendingIds = {}; savePendingIds(); return; }
      /* 插件没提供 getPending（或调用失败）→ 退回自建台账 */
      var ids = pendingList();
      pendingIds = {}; savePendingIds();
      if (!ids.length) return;
      return LN.cancel({ notifications: asRefs(ids) }).catch(function () {});
    });
  }

  function doSync(list) {
    if (!NATIVE || !inited) return Promise.resolve();
    /* 串行化。两次 doSync 交错时，后一次会在前一次 schedule 落地之前就发出自己的
     * cancel，两边各排一份，先落地的那份就成了没人认领的僵尸。
     * 中间状态本来也没人关心，排队只处理最后一次即可。 */
    if (syncBusy) { syncQueued = list; return Promise.resolve(); }
    syncBusy = true;

    var now = Date.now();
    var items = list.filter(function (x) { return x.at.getTime() > now + 5000; }).map(function (x) {
      return {
        title: '该服药了',
        body: x.medName + ' · ' + x.timeStr,
        id: idOf(x.id),
        schedule: { at: x.at, allowWhileIdle: true },
        channelId: CHANNEL,
        actionTypeId: ACTIONS,
        extra: { doseId: x.id }
      };
    });
    /* 等启动清场跑完再排 —— 否则可能自己把自己刚排的取消掉 */
    return (bootPurge || Promise.resolve())
      .then(function () { return cancelAll(); })
      .then(function () {
        if (!items.length) return null;
        return LN.schedule({ notifications: items });
      })
      .then(function () {
        var ids = items.map(function (i) { return i.id; });
        scheduled = ids.map(function (n) { return { id: n }; });
        markPending(ids);
      })
      .catch(function (e) { console.warn('notify sync', e); })
      .then(function () {
        syncBusy = false;
        var q = syncQueued;
        syncQueued = null;
        if (q) doSync(q);
      });
  }

  /* 合并抖动：一次改动会触发多次 save，避免反复 cancel/schedule */
  function sync(list) {
    lastList = list || lastList;
    if (!NATIVE) return;
    clearTimeout(timer);
    timer = setTimeout(function () { doSync(lastList); }, 250);
  }

  function cancelOne(doseId) {
    if (!NATIVE) return;
    var n = idOf(doseId);
    if (pendingIds[n]) { delete pendingIds[n]; savePendingIds(); }
    LN.cancel({ notifications: [{ id: n }] }).catch(function () {});
  }

  /* 测试提醒：delayMs 毫秒后响一次，用于真机验收，不写入任何服药数据。
   * 原生：直接登记一条带锁屏按钮的系统通知；浏览器：降级为延时页面弹窗。 */
  function test(delayMs) {
    delayMs = delayMs || 10000;
    if (!NATIVE || !inited) {
      if (handler) setTimeout(function () { handler('open', 'test'); }, delayMs);
      return Promise.resolve(!NATIVE);
    }
    /* 固定 id + **先取消再登记** —— 点多少次，系统里都只有一条测试通知。
     * （旧实现的随机 id 会让测试通知越点越多，而且它们谁也取消不掉。） */
    return LN.cancel({ notifications: [{ id: TEST_ID }] })
      .catch(function () { /* 本来就不存在，正常 */ })
      .then(function () {
        return LN.schedule({
          notifications: [{
            title: '测试提醒',
            body: '这是一条测试通知，确认提醒能正常响。',
            id: TEST_ID,
            schedule: { at: new Date(Date.now() + delayMs), allowWhileIdle: true },
            channelId: CHANNEL,
            actionTypeId: ACTIONS,
            extra: { doseId: 'test' }
          }]
        });
      })
      .then(function () { return true; })
      .catch(function (e) { console.warn('notify test', e); return false; });
  }

  /* 会话开始的彻底清场：取消插件存储里的全部遗留 + 清空通知栏。
   * 目的是让 App **每次打开都从一个干净的通知状态开始** —— 只有这样，
   * 手机重启时 RestoreReceiver 才没有东西可以「复活」。
   * 返回一个 Promise，doSync 会等它完成。 */
  function purge() {
    if (!NATIVE) return Promise.resolve(0);
    var p = cancelAll().then(function () {
      if (typeof LN.removeAllDeliveredNotifications !== 'function') return 0;
      return LN.removeAllDeliveredNotifications().then(function () { return 1; }).catch(function () { return 0; });
    }).catch(function () { return 0; });
    bootPurge = p;
    return p;
  }

  /* 只清通知栏里**已经显示出来**的，不动已排的闹钟。
   * 用在「回到前台」：用户已经在看 App 了，通知栏里那些提醒已经没有意义；
   * 而且它们很可能是刚刚被系统一次性投递出来的（休眠期间攒下的），
   * 留着只会让人以为「怎么又冒出来一堆」。 */
  function clearDelivered() {
    if (!NATIVE || typeof LN.removeAllDeliveredNotifications !== 'function') return Promise.resolve(0);
    return LN.removeAllDeliveredNotifications().catch(function () {});
  }

  /* 诊断：系统里到底排着几条、通知栏里显示着几条。
   * 「冒出一堆通知」这类问题，光看代码猜不出来，得有数字。 */
  function stat() {
    if (!NATIVE) return Promise.resolve({ native: false, pending: null, delivered: null, test: false });
    var r = { native: true, pending: null, delivered: null, test: false };
    return Promise.resolve()
      .then(function () {
        if (typeof LN.getPending !== 'function') return null;
        return LN.getPending().then(function (x) {
          var list = (x && x.notifications) || [];
          r.pending = list.length;
          for (var i = 0; i < list.length; i++) if (list[i] && Number(list[i].id) === TEST_ID) r.test = true;
        });
      })
      .catch(function () { /* 保留 null */ })
      .then(function () {
        if (typeof LN.getDeliveredNotifications !== 'function') return null;
        return LN.getDeliveredNotifications().then(function (x) {
          r.delivered = ((x && x.notifications) || []).length;
        });
      })
      .catch(function () { /* 保留 null */ })
      .then(function () { return r; });
  }

  /* 通知可用性综合探测。
   * 只看 checkPermissions() 不够：用户可能只关掉「本 App 的这一个通知渠道」，
   * 此时 App 级开关仍开着、checkPermissions() 报 granted，但提醒实际收不到。
   * 所以三路一起看：权限状态 / App 级开关 / 本渠道重要性。
   * 任一信号为「收不到」即视为不可用 —— 提醒类 App 宁可与误报多提示，也不能静默失效。 */
  function probe() {
    if (!NATIVE) return Promise.resolve({ native: false });
    var r = { native: true, display: 'unknown', enabled: null, channelFound: false, channelImportance: null };

    return Promise.resolve()
      .then(function () {
        return LN.checkPermissions().then(function (x) { r.display = (x && x.display) || 'unknown'; });
      })
      .catch(function () { /* 保留 unknown */ })
      .then(function () {
        if (typeof LN.areEnabled !== 'function') return null;
        return LN.areEnabled().then(function (x) { r.enabled = !!(x && x.value); });
      })
      .catch(function () { /* 保留 null */ })
      .then(function () {
        if (typeof LN.listChannels !== 'function') return null;
        return LN.listChannels().then(function (x) {
          var list = (x && (x.channels || x.notificationChannels)) || (Array.isArray(x) ? x : []);
          for (var i = 0; i < list.length; i++) {
            var ch = list[i];
            if (ch && ch.id === CHANNEL) {
              r.channelFound = true;
              r.channelImportance = (typeof ch.importance === 'number') ? ch.importance : null;
              break;
            }
          }
        });
      })
      .catch(function () { /* 渠道查询失败不算致命 */ })
      .then(function () { return r; });
  }

  /* 综合判定：是否真的收不到提醒 */
  function isBlocked(p) {
    if (!p || !p.native) return false;
    if (p.display === 'denied') return true;
    if (p.enabled === false) return true;
    // importance 0 = IMPORTANCE_NONE，用户把这条渠道单独关掉了
    if (p.channelImportance === 0) return true;
    return false;
  }

  /* 跳到系统通知设置页。原生侧由本项目的 AppSettingsPlugin 提供 ——
   * Capacitor 官方没有这个 API（@capacitor/app 只有 exitApp/getInfo/minimizeApp 等）。
   * 权限被拒后系统不再弹窗，只能用户手动去设置里开，所以这个跳转是必要的。
   * 浏览器/PWA 没有等价能力，返回 false 让调用方降级成文字指引。 */
  /* 原生桥接是否真的注入过 —— 用 PluginHeaders 判断（它在 globalJS/bridgeJS 之后由原生注入）。
   * 这个信号能区分两种「插件看起来在、实际调不通」的情况：
   * 只有 vendor 里的 JS 生效时，Plugins 有名字但 nativePromise 并不存在。 */
  function bridgeHeaders() {
    var c = window.Capacitor;
    var hs = c && c.PluginHeaders;
    return (hs && hs.length) ? hs.length : 0;
  }
  function hasHeader(name) {
    var c = window.Capacitor;
    var hs = (c && c.PluginHeaders) || [];
    for (var i = 0; i < hs.length; i++) if (hs[i] && hs[i].name === name) return true;
    return false;
  }

  /* 取本项目的跳设置插件。
   * 它**没有 JS 包装**，正常情况下由原生侧生成的 JS（JSExport.getPluginJS）
   * 注入成 Capacitor.Plugins.AppSettings；万一那份注入没生效，再用 core 的
   * registerPlugin 建一个代理兜底（只要有 PluginHeaders 就能调通原生）。 */
  function settingsPlugin() {
    var c = window.Capacitor;
    var P = c && c.Plugins && c.Plugins.AppSettings;
    if (P && typeof P.openNotificationSettings === 'function') return P;
    if (c && typeof c.registerPlugin === 'function') {
      try {
        if (!hasHeader('AppSettings')) return null;
        var g = c.registerPlugin('AppSettings');
        if (g && typeof g.openNotificationSettings === 'function') return g;
      } catch (e) { /* 插件不存在时会抛，忽略 */ }
    }
    return null;
  }
  /* 'injected'（原生已注入） / 'proxy'（靠 registerPlugin 兜底） / 'missing' */
  function settingsState() {
    var c = window.Capacitor;
    var P = c && c.Plugins && c.Plugins.AppSettings;
    if (P && typeof P.openNotificationSettings === 'function') return 'injected';
    if (hasHeader('AppSettings') && c && typeof c.registerPlugin === 'function') return 'proxy';
    return 'missing';
  }

  /* 跳系统通知设置。
   * 返回 {ok, reason} —— **必须带原因**：跳不过去时用户看到的那行字，
   * 决定他是能自己找到设置，还是只能干等（这正是本轮反馈的问题）。 */
  function openSettings() {
    var P = settingsPlugin();
    if (!P) return Promise.resolve({ ok: false, reason: 'AppSettings 插件未注册' });
    return P.openNotificationSettings()
      .then(function () { return { ok: true, reason: '' }; })
      .catch(function (e) { return { ok: false, reason: (e && e.message) || '系统拒绝打开设置页' }; });
  }

  window.MedNotify = {
    native: NATIVE,
    init: init,
    sync: sync,
    test: test,
    purge: purge,
    clearDelivered: clearDelivered,
    stat: stat,
    cancelOne: cancelOne,
    checkPermissions: checkPermissions,
    requestPermission: requestPermission,
    probe: probe,
    isBlocked: isBlocked,
    openSettings: openSettings,
    settingsState: settingsState,
    bridgeHeaders: bridgeHeaders,
    plugins: function () {
      var c = window.Capacitor;
      return (c && c.Plugins) ? Object.keys(c.Plugins) : [];
    }
  };
})();
