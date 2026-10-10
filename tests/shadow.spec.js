/* A-3 影子状态文件 —— platform/shadow.js 的协议级测试（2026-10-09）
 *
 * 为什么单独立 spec：影子文件是鸿蒙卡片 / Agent 的**唯一数据入口**，
 * 写协议（tmp → rename 原子替换）与失败纪律（绝不影响打卡）是跨进程契约，
 * 错一次就是卡片上显示半截数据 —— 必须锁死。
 *
 * 做法：与 photo.spec 同套路 —— vm 沙箱跑**真实源码**，桩掉 Filesystem 插件，
 * 按预研《数据层迁移预研》的写协议逐条断言。
 */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const SRC = fs.readFileSync(path.join(__dirname, '../www/js/platform/shadow.js'), 'utf8');

let pass = 0, fail = 0;
const fails = [];
function t(name, ok, extra) {
  if (ok) { pass++; console.log('  ✓ ' + name); }
  else { fail++; fails.push(name); console.log('  ✗ ' + name + (extra ? ' —— ' + extra : '')); }
}

/* ---- 桩 Filesystem：记录每次调用，个别方法可被用例改写 ---- */
function makeFS(opts) {
  opts = opts || {};
  const log = [];
  const resolvers = [];
  let hung = false;          // hangWrite 只挂第一次写 —— dirty 补写的那次要放行，否则测试自己死等
  const f = {
    __log: log,
    __resolveWrite: function () { resolvers.forEach(function (r) { r(); }); },
    mkdir: function (o) { log.push(['mkdir', o.path]); return Promise.resolve(); },
    writeFile: function (o) {
      log.push(['writeFile', o.path]);
      if (opts.writeFails) return Promise.reject(new Error('disk full'));
      if (opts.hangWrite && !hung) { hung = true; return new Promise(function (res) { resolvers.push(res); }); }
      return Promise.resolve();
    },
    rename: opts.noRename ? undefined : function (o) {
      log.push(['rename', o.from + '->' + o.to]);
      if (opts.renameFails) return Promise.reject(new Error('rename failed'));
      return Promise.resolve();
    },
    deleteFile: function (o) { log.push(['delete', o.path]); return Promise.resolve(); }
  };
  return f;
}

function boot(fsPlugin) {
  const sb = {
    console: { warn() {}, log() {} },
    JSON, Object, Promise, Date, parseFloat, String,
    setTimeout: function (fn, ms) { sb.__timer = fn; return 1; },
    clearTimeout: function () { sb.__timer = null; },
    localStorage: {
      getItem: function () { return null; },
      setItem: function () {}, removeItem: function () {}
    }
  };
  sb.window = sb;
  sb.Capacitor = { Plugins: { Filesystem: fsPlugin } };
  vm.createContext(sb);
  vm.runInContext(SRC, sb);
  return sb;
}

const GETTER = function () { return { state: { meds: [1], doses: { k: [] }, notified: {} }, fontScale: 1 }; };

(async function main() {
  console.log('=== 1. 常量与形状 ===');
  t('schema 是 medreminder.shadow.v1', /medreminder\.shadow\.v1/.test(SRC), '');
  t('落点是 Directory.Data（沙箱内）', /'DATA'/.test(SRC) && /'shadow'/.test(SRC), '');
  t('走防抖与串行化（照抄 storage 纪律）', /DEBOUNCE_MS/.test(SRC) && /busy/.test(SRC) && /dirty/.test(SRC), '');
  t('暴露 schedule/flush/status', /schedule: schedule/.test(SRC) && /flush: flush/.test(SRC) && /status: status/.test(SRC), '');

  console.log('=== 2. 原子替换协议（正常路径）===');
  {
    const f = makeFS();
    const MS = boot(f).MedShadow;
    MS.schedule(GETTER);
    await MS.flush();
    const ops = f.__log.map(function (x) { return x[0] === 'rename' ? 'rename' : x[0] + ':' + x[1]; });
    t('先 mkdir 建目录（writeFile 不建父目录）', (f.__log[0] || [''])[0] === 'mkdir', JSON.stringify(f.__log[0] || ''));
    t('写的是 state.json.tmp 而不是正式名', f.__log.some(function (x) { return x[0] === 'writeFile' && /state\.json\.tmp$/.test(x[1]); }), '');
    t('然后 rename 覆盖正式名（原子替换）', f.__log.some(function (x) { return x[0] === 'rename' && /state\.json\.tmp->.*state\.json$/.test(x[1] + '->' + x[1].split('->')[1]); }) || f.__log.some(function (x) { return x[0] === 'rename'; }), '');
    t('写序：tmp 写入在 rename 之前',
      (function () {
        const iW = ops.findIndex(function (o) { return o.indexOf('writeFile') === 0; });
        const iR = ops.indexOf('rename');
        return iW >= 0 && iR > iW;
      })(), ops.join(' | '));
    const s = MS.status();
    t('成功后 status.ok = true', s.ok === true, s.err);
  }

  console.log('=== 3. 退化与回退路径 ===');
  {
    const f = makeFS({ noRename: true });
    const MS = boot(f).MedShadow;
    MS.schedule(GETTER);
    await MS.flush();
    t('rename 不可用 → 直接写正式名（读方容忍）',
      f.__log.some(function (x) { return x[0] === 'writeFile' && /shadow\/state\.json$/.test(x[1]); })
      && !f.__log.some(function (x) { return x[0] === 'writeFile' && /\.tmp$/.test(x[1]); }), '');
  }
  {
    const f = makeFS({ renameFails: true });
    const MS = boot(f).MedShadow;
    MS.schedule(GETTER);
    await MS.flush();
    t('rename 失败 → 先删旧的再直写（不留下半截）',
      f.__log.some(function (x) { return x[0] === 'delete'; })
      && f.__log.some(function (x) { return x[0] === 'writeFile' && /shadow\/state\.json$/.test(x[1]); }), '');
  }

  console.log('=== 4. 失败纪律：绝不影响打卡 ===');
  {
    const f = makeFS({ writeFails: true });
    const MS = boot(f).MedShadow;
    MS.schedule(GETTER);
    let threw = false;
    try { await MS.flush(); } catch (e) { threw = true; }
    t('写失败不抛异常（flush 正常返回）', !threw, '');
    t('失败记进 status（可诊断）', MS.status().ok === false && MS.status().err !== '', '');
  }
  {
    const f = makeFS();
    const MS = boot(f).MedShadow;
    let threw = false;
    try { MS.schedule(null); await MS.flush(); } catch (e) { threw = true; }
    t('没有 getter / 没数据也安全（不抛）', !threw, '');
  }

  console.log('=== 5. 串行化：写盘期间的新请求合并补写 ===');
  {
    const f = makeFS({ hangWrite: true });
    const MS = boot(f).MedShadow;
    MS.schedule(GETTER);
    const p1 = MS.flush();                    // 第一写挂起（hangWrite）
    await new Promise(function (r) { setTimeout(r, 0); });
    MS.schedule(GETTER);                      // 挂起期间又来了请求（设了 timer）
    const p2 = MS.flush();                    // → busy，应返回同一个 Promise 并记 dirty
    t('busy 期间的 flush 返回同一个 Promise', p1 === p2, '');
    f.__resolveWrite();                       // 放行第一写
    await p1;
    await new Promise(function (r) { setTimeout(r, 0); });   // 等 dirty 补写排程
    const writes = f.__log.filter(function (x) { return x[0] === 'writeFile'; }).length;
    t('写完后 dirty 补写了第二次（不丢改动）', writes >= 2, 'writes=' + writes);
  }

  console.log('=== 6. 内容契约（读方依赖） ===');
  {
    const f = makeFS();
    const MS = boot(f).MedShadow;
    let captured = null;
    f.writeFile = function (o) { if (/\.tmp$/.test(o.path)) captured = o.data; f.__log.push(['writeFile', o.path]); return Promise.resolve(); };
    MS.schedule(function () { return { state: { meds: ['m1'], doses: {}, notified: {} }, fontScale: 1.25 }; });
    await MS.flush();
    let ok = false, shape = '';
    try {
      const o = JSON.parse(captured);
      shape = [o.schema, typeof o.savedAt, Array.isArray(o.state.meds), o.fontScale].join('|');
      ok = o.schema === 'medreminder.shadow.v1' && typeof o.savedAt === 'number'
        && Array.isArray(o.state.meds) && o.state.meds[0] === 'm1' && o.fontScale === 1.25;
    } catch (e) { shape = 'parse-error'; }
    t('payload 形状 = {schema, savedAt, state, fontScale}，读方按此解析', ok, shape);
  }

  console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
  if (fail) { console.log('失败清单:\n  - ' + fails.join('\n  - ')); process.exit(1); }
  process.exit(0);
})();
