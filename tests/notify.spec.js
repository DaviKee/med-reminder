/* 通知链路用例（v1.2.1）
 *
 * 起因：用户反馈「点一次测试提醒，冒出来 6 个通知」。
 *
 * 桩严格按 @capacitor/local-notifications 的 Java 实现写，不是想当然：
 *   · schedule → dismissVisibleNotification(id) + cancelTimerForNotification(id)
 *                + buildNotification() + notificationStorage.appendNotifications()  → 按 id 覆盖
 *   · cancel   → dismissVisibleNotification + cancelTimerForNotification
 *                + storage.deleteNotification(id)
 *   · getPending → notificationStorage.getSavedNotifications()   ← 权威清单
 *   · LocalNotificationRestoreReceiver.onReceive（开机）：
 *         凡 at 已过的通知一律改成「now + 15s」，然后全部 schedule 一遍
 *     源码注释：// modify the scheduled date in order to show notifications that
 *               // would have been delivered while device was off.
 *     ★ 这条就是「一次冒出来一堆」的机制本身，所以桩里必须原样实现。
 */
const fs = require('fs');
const vm = require('vm');

const SRC = fs.readFileSync(require("path").join(__dirname, "..", "www/js/notify.js"), "utf8");
let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}
const sleep = ms => new Promise(r => setTimeout(r, ms));
const settle = (ms = 500) => sleep(ms);

/* ---------------- 系统侧：模拟插件（含插件的持久化存储） ---------------- */
function makeSystem() {
  const pending = new Map();      // NotificationStorage：已登记但还没到点的
  const delivered = new Map();    // 已经显示在通知栏里的
  let next = 1;

  const sys = {
    pending, delivered,
    scheduledCalls: 0,
    schedule({ notifications }) {
      sys.scheduledCalls++;
      for (const n of notifications) {
        // Java 在排程前会先干掉同 id 的（所以同 id 不会重复）
        delivered.delete(n.id);
        pending.set(n.id, {
          id: n.id,
          title: n.title,
          at: n.schedule && n.schedule.at ? +new Date(n.schedule.at) : 0
        });
      }
      return Promise.resolve({ notifications: notifications.map(n => ({ id: n.id })) });
    },
    cancel({ notifications }) {
      for (const n of notifications) { pending.delete(n.id); delivered.delete(n.id); }
      return Promise.resolve();
    },
    getPending() {
      return Promise.resolve({
        notifications: Array.from(pending.values()).map(p => ({
          id: p.id, title: p.title, body: '',
          schedule: { at: new Date(p.at).toISOString() }
        }))
      });
    },
    getDeliveredNotifications() {
      return Promise.resolve({ notifications: Array.from(delivered.values()) });
    },
    removeAllDeliveredNotifications() { delivered.clear(); return Promise.resolve(); },

    /* ★ 插件的 LocalNotificationRestoreReceiver：手机开机时跑一遍。
     * 返回「会被改成开机后 15 秒响」的条数 —— 也就是用户开机后一次性看到的条数。 */
    boot() {
      const now = Date.now();
      const revived = [];
      for (const p of Array.from(pending.values())) {
        if (p.at && p.at < now) { p.at = now + 15000; revived.push(p.id); }
      }
      return revived;
    },
    /* 模拟「时间到，系统投递」。advanceMs 用来把时钟往前推，
     * 否则模拟不出「开机 15 秒后那批通知一起弹出来」。 */
    fireNow(advanceMs) {
      const at = Date.now() + (advanceMs || 0);
      for (const p of Array.from(pending.values())) {
        if (p.at && p.at <= at) { delivered.set(p.id, { id: p.id }); pending.delete(p.id); }
      }
      return delivered.size;
    },
    /* 往存储里塞一条「过去某次会话排下、现在已过期」的通知 —— 复现用户手机的状态 */
    seedExpired(n, startId) {
      const past = Date.now() - 3600 * 1000;
      for (let i = 0; i < n; i++) pending.set(startId + i, { id: startId + i, title: '该服药了', at: past });
      return n;
    },
    createChannel() { return Promise.resolve(); },
    registerActionTypes() { return Promise.resolve(); },
    addListener() { return Promise.resolve(); },
    checkPermissions() { return Promise.resolve({ display: 'granted' }); },
    requestPermissions() { return Promise.resolve({ display: 'granted' }); },
    areEnabled() { return Promise.resolve({ value: true }); },
    listChannels() { return Promise.resolve({ channels: [{ id: 'doses', importance: 5 }] }); }
  };
  return sys;
}

/* ---------------- 一次「App 会话」：同一个 system，但 JS 状态全新 ----------------
 * store 传入即代表「App 重启后 localStorage 还在」；
 * 不传就是全新机型（清过数据）。 */
function boot(system, store) {
  const ls = store || new Map();
  const sandbox = {
    console: { warn() {}, log() {}, error() {} },
    Promise, Date, Math, JSON, Object, Array, String, Number, Boolean, isFinite, parseInt, parseFloat,
    setTimeout, clearTimeout, setInterval, clearInterval,
    localStorage: {
      getItem: k => (ls.has(k) ? ls.get(k) : null),
      setItem: (k, v) => { ls.set(k, String(v)); },
      removeItem: k => { ls.delete(k); }
    }
  };
  sandbox.window = sandbox;
  sandbox.window.Capacitor = {
    Plugins: { LocalNotifications: system },
    PluginHeaders: [{ name: 'LocalNotifications' }]
  };
  vm.createContext(sandbox);
  vm.runInContext(SRC, sandbox);
  return { N: sandbox.window.MedNotify, ls, sandbox };
}

const doseList = (specs) => specs.map(s => ({
  id: s.id, timeStr: s.timeStr || '08:00', medName: s.medName || '药A',
  at: new Date(Date.now() + s.inMin * 60000)
}));

(async function main() {
  console.log('=== A. 复现「一次冒出来一堆」（旧行为）===');
  {
    const sys = makeSystem();
    sys.seedExpired(6, 101);                       // 6 条过期通知积压在插件存储里
    // 旧实现：cancelAll 只看内存变量 scheduled，冷启动后为空 → 什么都不清。
    // 这里不调用任何清理，直接模拟「手机开机」。
    const revived = sys.boot();
    t('★ 复现：开机后这 6 条被改成「15 秒后响」', revived.length === 6, revived.join(','));
    // ⚠️ 必须把时钟推进过那 15 秒 —— 刚 boot 完它们还没到点（这里我第一版就写错了，
    //    直接 fireNow() 当然得到 0）。真实场景就是"用户开机十几秒后，通知一起弹出来"。
    const delivered = sys.fireNow(16000);
    t('★ 复现：开机 15 秒后这 6 条一起投递（用户看到的就是「一次冒出 6 个」）', delivered === 6, delivered);
  }

  console.log('');
  console.log('=== B. 修复后：启动清场把僵尸清干净 ===');
  {
    const sys = makeSystem();
    sys.seedExpired(6, 201);
    const s = boot(sys);
    await s.N.init(() => {});
    t('清场前：存储里有 6 条', sys.pending.size === 6, sys.pending.size);
    await s.N.purge();
    t('★ purge 后存储里 0 条', sys.pending.size === 0, sys.pending.size);
    t('★ 开机也不会再复活', sys.boot().length === 0, sys.boot().length);
    t('★ 投递 0 条', sys.fireNow() === 0, sys.fireNow());
  }

  console.log('');
  console.log('=== C. 跨会话排程：旧的通知必须被清掉，不能与新的并存 ===');
  {
    const store = new Map();          // App 重启，localStorage 保留
    const sys = makeSystem();

    // 会话 1：排 3 条
    const s1 = boot(sys, store);
    await s1.N.init(() => {});
    s1.N.sync(doseList([{ id: 'd1', inMin: 60 }, { id: 'd2', inMin: 120 }, { id: 'd3', inMin: 180 }]));
    await settle();
    t('会话 1 排了 3 条', sys.pending.size === 3, sys.pending.size);
    const idsA = Array.from(sys.pending.keys()).sort().join(',');

    // 会话 2（App 被系统杀掉后重新打开，内存台账归零）
    const s2 = boot(sys, store);
    await s2.N.init(() => {});
    s2.N.sync(doseList([{ id: 'd4', inMin: 30 }, { id: 'd5', inMin: 90 }]));
    await settle();
    t('★ 会话 2 排完后系统里只有 2 条（旧 3 条已清）', sys.pending.size === 2, sys.pending.size);
    const kept = Array.from(sys.pending.values()).every(p => p.title === '该服药了');
    t('★ 留下的都是新一轮的', kept && Array.from(sys.pending.keys()).sort().join(',') !== idsA,
      Array.from(sys.pending.keys()).sort().join(','));
  }

  console.log('');
  console.log('=== D. 测试通知幂等（直接对应「点一次出 6 个」）===');
  {
    const sys = makeSystem();
    const s = boot(sys);
    await s.N.init(() => {});
    for (let i = 0; i < 5; i++) await s.N.test(10000);
    const testOnes = Array.from(sys.pending.values()).filter(p => p.title === '测试提醒');
    t('★ 连点 5 次，系统里只有 1 条测试通知', testOnes.length === 1, testOnes.length);
    t('★ 它就是固定 id（990000001）', testOnes[0] && testOnes[0].id === 990000001, testOnes[0] && testOnes[0].id);

    // 反证：旧实现的随机 id 会怎样
    const sysOld = makeSystem();
    for (let i = 0; i < 5; i++) {
      await sysOld.schedule({ notifications: [{ title: '测试提醒', id: 1 + Math.floor(Math.random() * 2000000000), schedule: { at: new Date(Date.now() + 10000) } }] });
    }
    t('★ 反证：旧写法连点 5 次留下 5 条谁也认不出的通知', sysOld.pending.size === 5, sysOld.pending.size);
  }

  console.log('');
  console.log('=== E. doSync 串行化（并发不再留僵尸）===');
  {
    const sys = makeSystem();
    const s = boot(sys);
    await s.N.init(() => {});
    // 连发三次 sync；防抖会收敛，但再叠加一层并发保护的保证是：
    // 最终系统里的条数 = 最后一次列表的条数，不多不少
    s.N.sync(doseList([{ id: 'a1', inMin: 60 }]));
    await sleep(120);
    s.N.sync(doseList([{ id: 'b1', inMin: 60 }, { id: 'b2', inMin: 120 }, { id: 'b3', inMin: 180 }]));
    await sleep(120);
    s.N.sync(doseList([{ id: 'c1', inMin: 60 }, { id: 'c2', inMin: 120 }]));
    await settle(900);
    t('★ 最终只剩最后一轮的 2 条', sys.pending.size === 2, sys.pending.size);
    t('★ 没有中途遗留', Array.from(sys.pending.values()).every(p => p.at > Date.now()), '有过去时间的残留');
  }

  console.log('');
  console.log('=== F. 台账落盘 / clearDelivered / stat ===');
  {
    const store = new Map();
    const sys = makeSystem();
    const s = boot(sys, store);
    await s.N.init(() => {});
    s.N.sync(doseList([{ id: 'x1', inMin: 60 }, { id: 'x2', inMin: 120 }]));
    await settle();
    const raw = store.get('medreminder.notifPending.v1');
    t('★ 台账已落盘', !!raw, raw);
    t('★ 台账记了 2 个 id', raw && Object.keys(JSON.parse(raw)).length === 2, raw);

    await sys.getPending().then(r => { sys.delivered.set(r.notifications[0].id, { id: 'v' }); sys.delivered.set(999, { id: 999 }); });
    const before = sys.pending.size;
    await s.N.clearDelivered();
    t('clearDelivered 清空通知栏', sys.delivered.size === 0, sys.delivered.size);
    t('★ clearDelivered 不动已排的闹钟', sys.pending.size === before, sys.pending.size + ' vs ' + before);

    const st = await s.N.stat();
    t('stat 报出排程条数', st.pending === before, JSON.stringify(st));
    t('stat 报出通知栏条数', st.delivered === 0, JSON.stringify(st));
    t('stat 能标出测试通知', st.test === false, JSON.stringify(st));

    await s.N.test(10000);
    const st2 = await s.N.stat();
    t('★ 排了测试提醒后 stat.test 为真', st2.test === true, JSON.stringify(st2));
    t('★ 测试提醒也进统计（+1 条）', st2.pending === before + 1, JSON.stringify(st2));
  }

  console.log('');
  console.log('=== G. purge 同时清排程与通知栏 ===');
  {
    const sys = makeSystem();
    const s = boot(sys);
    await s.N.init(() => {});
    s.N.sync(doseList([{ id: 'p1', inMin: 60 }, { id: 'p2', inMin: 120 }]));
    await settle();
    sys.delivered.set(777, { id: 777 });
    await s.N.purge();
    t('purge 清排程', sys.pending.size === 0, sys.pending.size);
    t('purge 清通知栏', sys.delivered.size === 0, sys.delivered.size);
    t('purge 清台账', Object.keys(JSON.parse(s.ls.get('medreminder.notifPending.v1') || '{}')).length === 0, s.ls.get('medreminder.notifPending.v1'));
  }

  console.log('');
  console.log('=== H. 启动顺序：purge 与排程不会互相踩 ===');
  {
    const sys = makeSystem();
    sys.seedExpired(4, 301);
    const s = boot(sys);
    await s.N.init(() => {});
    s.N.purge();                                       // 不 await，模拟启动时的调用方式
    s.N.sync(doseList([{ id: 'q1', inMin: 60 }, { id: 'q2', inMin: 120 }]));   // 紧接着就排程
    await settle(700);
    t('★ 清场 + 排程后只剩今天的 2 条', sys.pending.size === 2, sys.pending.size);
    t('★ 没有僵尸残留', sys.boot().length === 0, sys.boot().length);
  }

  console.log('');
  console.log('=== I. 降级与回归 ===');
  {
    // I-1 浏览器模式（无 Capacitor）：所有新 API 都不能崩
    const ls = new Map();
    const sandbox = {
      console: { warn() {}, log() {} },
      Promise, Date, Math, JSON, Object, Array, String, Number, Boolean, isFinite, parseInt, parseFloat,
      setTimeout, clearTimeout, setInterval, clearInterval,
      localStorage: { getItem: k => (ls.has(k) ? ls.get(k) : null), setItem: (k, v) => ls.set(k, String(v)), removeItem: k => ls.delete(k) }
    };
    sandbox.window = sandbox;
    vm.createContext(sandbox);
    vm.runInContext(SRC, sandbox);
    const B = sandbox.window.MedNotify;
    t('浏览器模式 native=false', B.native === false, B.native);
    let threw = '';
    try {
      await B.purge(); await B.clearDelivered();
      const st = await B.stat();
      t('浏览器模式 stat 不崩', st && st.native === false, JSON.stringify(st));
      await B.test(1); await B.sync([]); await B.cancelOne('x');
    } catch (e) { threw = String(e && e.message); }
    t('★ 浏览器模式新 API 全部安全降级', threw === '', threw);

    // I-2 插件没有 getPending（旧版本）→ 退回自建台账
    const sys2 = makeSystem();
    const saved = sys2.getPending;
    delete sys2.getPending;
    const s2 = boot(sys2);
    await s2.N.init(() => {});
    s2.N.sync(doseList([{ id: 'y1', inMin: 60 }, { id: 'y2', inMin: 120 }]));
    await settle();
    t('没有 getPending 时照常排程', sys2.pending.size === 2, sys2.pending.size);
    await s2.N.purge();
    t('★ 退回台账也能清干净', sys2.pending.size === 0, sys2.pending.size);
    sys2.getPending = saved;

    // I-3 cancelOne 正常，且台账同步摘掉
    const sys3 = makeSystem();
    const s3 = boot(sys3);
    await s3.N.init(() => {});
    s3.N.sync(doseList([{ id: 'z1', inMin: 60 }, { id: 'z2', inMin: 120 }]));
    await settle();
    const before3 = sys3.pending.size;
    await s3.N.cancelOne('z1');
    t('cancelOne 取消一条', sys3.pending.size === before3 - 1, sys3.pending.size);
    t('★ cancelOne 不误伤另一条', sys3.pending.size === 1, sys3.pending.size);
  }

  console.log('');
  console.log('=== J. 源码级断言（防止改回去）===');
  {
    // 剥离注释再查 —— 说明注释里也写着旧写法，不剥离就是假阳性
    const CODE = SRC.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
    t('★ cancelAll 里不再有「内存为空就早退」', CODE.indexOf('!scheduled.length') < 0, '仍有 scheduled.length 早退');
    t('★ cancelAll 走 getPending 拿权威清单', CODE.indexOf('LN.getPending') >= 0, '缺少 getPending');
    t('★ 测试通知不再用随机 id', CODE.indexOf('Math.random() * 2000000000') < 0, '仍是随机 id');
    t('★ 测试通知用固定 TEST_ID', /var TEST_ID = \d+/.test(CODE), '没有 TEST_ID');
    t('★ 有 purge / clearDelivered / stat 且已导出',
      CODE.indexOf('function purge()') >= 0 && CODE.indexOf('function clearDelivered()') >= 0 &&
      CODE.indexOf('function stat()') >= 0 && CODE.indexOf('purge: purge') >= 0, '导出不全');
    t('★ doSync 有串行化标记', CODE.indexOf('if (syncBusy)') >= 0, '缺 syncBusy');
    t('★ doSync 等启动清场', CODE.indexOf('bootPurge') >= 0, '缺 bootPurge');
    t('★ 已排通知台账落盘', CODE.indexOf('medreminder.notifPending.v1') >= 0, '缺台账');

    const APP = fs.readFileSync(require("path").join(__dirname, "..", "www/js/app.js"), "utf8");
    const APPCODE = APP.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
    t('★ 启动时清场', APPCODE.indexOf('window.MedNotify.purge()') >= 0, '启动未清场');
    t('★ 回到前台清通知栏', APPCODE.indexOf('clearDelivered()') >= 0, 'resume 未清理');
    t('★ DEBUG 卡有一键清理按钮', APP.indexOf('id="btnPurgeNotif"') >= 0, '缺按钮');
    t('★ 诊断行报系统排程条数', APP.indexOf('系统排程：') >= 0, '缺诊断行');
    t('★ 测试按钮回报排程条数', APP.indexOf('系统当前排程共') >= 0, '缺回报');
  }

  console.log('');
  console.log('==========================================');
  console.log('通过 %d / 共 %d', pass, pass + fail);
  if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
  process.exit(fail ? 1 : 0);
})().catch(e => { console.error('用例本身抛错:', e); process.exit(2); });
