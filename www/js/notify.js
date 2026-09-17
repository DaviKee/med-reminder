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

  var CHANNEL = 'doses';
  var ACTIONS = 'DOSE_ACTIONS';
  var scheduled = [];
  var inited = false;
  var handler = null;
  var timer = null;
  var lastList = [];

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
      .then(function () { return LN.createChannel({ id: CHANNEL, name: '服药提醒', description: '按你设定的间隔提醒服药', importance: 5, visibility: 1, vibration: true, sound: 'default' }); })
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

  function cancelAll() {
    if (!NATIVE || !scheduled.length) return Promise.resolve();
    var ids = scheduled;
    scheduled = [];
    return LN.cancel({ notifications: ids }).catch(function () {});
  }

  function doSync(list) {
    if (!NATIVE || !inited) return Promise.resolve();
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
    return cancelAll()
      .then(function () {
        if (!items.length) return null;
        return LN.schedule({ notifications: items });
      })
      .then(function () {
        scheduled = items.map(function (i) { return { id: i.id }; });
      })
      .catch(function (e) { console.warn('notify sync', e); });
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
    LN.cancel({ notifications: [{ id: idOf(doseId) }] }).catch(function () {});
  }

  /* 测试提醒：delayMs 毫秒后响一次，用于真机验收，不写入任何服药数据。
   * 原生：直接登记一条带锁屏按钮的系统通知；浏览器：降级为延时页面弹窗。 */
  function test(delayMs) {
    delayMs = delayMs || 10000;
    if (!NATIVE || !inited) {
      if (handler) setTimeout(function () { handler('open', 'test'); }, delayMs);
      return Promise.resolve(!NATIVE);
    }
    var nid = 1 + Math.floor(Math.random() * 2000000000); // 测试通知用随机 id，不写入持久化映射表
    return LN.schedule({
      notifications: [{
        title: '测试提醒',
        body: '这是一条测试通知，确认提醒能正常响。',
        id: nid,
        schedule: { at: new Date(Date.now() + delayMs), allowWhileIdle: true },
        channelId: CHANNEL,
        actionTypeId: ACTIONS,
        extra: { doseId: 'test' }
      }]
    }).then(function () { return true; }).catch(function (e) { console.warn('notify test', e); return false; });
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
