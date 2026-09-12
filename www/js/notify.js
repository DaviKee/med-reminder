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

  window.MedNotify = {
    native: NATIVE,
    init: init,
    sync: sync,
    test: test,
    cancelOne: cancelOne,
    checkPermissions: checkPermissions,
    requestPermission: requestPermission
  };
})();
