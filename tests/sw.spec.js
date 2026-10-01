/* Service Worker 离线缓存（H-2）
 *
 * 修的三件事：
 *   ① 预缓存清单漏了 `js/photo.js` / `js/backup.js`（以及整组 `vendor/plugin-*.js`）
 *      —— 漏掉插件脚本最隐蔽：离线时 `Capacitor.Plugins.*` 全是 undefined，调用**静默降级**。
 *   ② 纯 cache-first **永不回源** → 装了新版、界面还是旧的。
 *   ③ install 用 `addAll` 且吞掉错误 → 有一个 404 就整体失败，且什么都不说。
 *
 * 桩一律照 Service Worker 真实语义写：
 *   · `caches.match()` 找不到返回 **undefined**（不是 null、也不是 reject）；
 *   · `Response.clone()` 必须真的复制（回填缓存不能把还给页面的那份消耗掉）；
 *   · `e.waitUntil` / `e.respondWith` 都是"接管一个 promise"，不是同步调用。
 * 查源码先剥离注释 —— 我们在注释里大量引用旧写法做对比，不剥离就会把说明文字当代码。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const SW_SRC = fs.readFileSync(path.join(ROOT, 'www/sw.js'), 'utf8');
const HTML_SRC = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const APP_SRC = require('./sources').all();

const stripJs = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const SW = stripJs(SW_SRC);
const APP = stripJs(APP_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}
const eq = (name, a, b) => t(name, JSON.stringify(a) === JSON.stringify(b),
  JSON.stringify(a) + ' !== ' + JSON.stringify(b));

const ORIGIN = 'https://localhost';

/* ---------------- 桩 ---------------- */
function mkRes(url, body, ok) {
  return {
    url: ORIGIN + url,
    ok: ok !== false,
    status: ok === false ? 404 : 200,
    type: 'basic',
    body: body,
    clone() { return mkRes(url, body, ok); }   // 必须真复制
  };
}

function mkReq(url, opts) {
  opts = opts || {};
  return {
    __url: url,
    url: ORIGIN + url,
    method: opts.method || 'GET',
    mode: opts.mode || 'no-cors'
  };
}

function makeEnv(opts) {
  opts = opts || {};
  const handlers = {};
  const stores = new Map();          // cacheName -> Map(key -> res)
  const deleted = [];
  const warns = [];
  const fetchCalls = [];
  let skipWaiting = false, claim = false;

  function storeOf(name) {
    if (!stores.has(name)) stores.set(name, new Map());
    return stores.get(name);
  }

  const caches = {
    open(name) {
      const m = storeOf(name);
      return Promise.resolve({
        add(u) {
          if (opts.failAssets && opts.failAssets.indexOf(u) >= 0) {
            return Promise.reject(new Error('404: ' + u));
          }
          m.set(u, mkRes(u, 'pre:' + u));
          return Promise.resolve();
        },
        put(req, res) { m.set(req.__url || req, res); return Promise.resolve(); }
      });
    },
    match(req) {
      const key = req && (req.__url || req);
      for (const m of stores.values()) if (m.has(key)) return Promise.resolve(m.get(key));
      return Promise.resolve(undefined);        // 真实语义：找不到 → undefined
    },
    keys() { return Promise.resolve(Array.from(stores.keys())); },
    delete(k) { deleted.push(k); stores.delete(k); return Promise.resolve(true); }
  };

  function fetchStub(r) {
    const u = r && (r.__url || r);
    fetchCalls.push(u);
    /* 「半死网络」：连得上但永远不回包 —— 没有超时的话界面会一直等下去 */
    if (opts.fetchHang) return new Promise(function () {});
    if (opts.fetchAllFail) return Promise.reject(new Error('offline'));
    if (opts.fetchFail && opts.fetchFail.indexOf(u) >= 0) return Promise.reject(new Error('offline'));
    const body = (opts.networkBody && opts.networkBody[u]) || ('net:' + u);
    return Promise.resolve(mkRes(u, body));
  }

  const selfStub = {
    location: { origin: ORIGIN },
    addEventListener(type, fn) { handlers[type] = fn; },
    skipWaiting() { skipWaiting = true; return Promise.resolve(); },
    clients: { claim() { claim = true; return Promise.resolve(); } }
  };

  const sandbox = { self: selfStub, caches, fetch: fetchStub,
                    console: { warn: m => warns.push(String(m)), log() {}, error() {} },
                    URL, Promise, Set, Map,
                    /* Service Worker 全局里有 setTimeout/clearTimeout（超时回退要用）。
                     * 第一版桩漏了它们 → 报 "setTimeout is not defined" ——
                     * 又是桩不全，不是源码错。 */
                    setTimeout, clearTimeout };
  vm.createContext(sandbox);
  vm.runInContext(SW_SRC, sandbox);

  return {
    handlers, stores, deleted, warns, fetchCalls,
    ASSETS: sandbox.ASSETS,
    CACHE: sandbox.CACHE,
    skipWaiting: () => skipWaiting,
    claim: () => claim,
    seed(cacheName, key, body) { storeOf(cacheName).set(key, mkRes(key, body)); },
    cached(cacheName, key) { const m = stores.get(cacheName); return m && m.get(key); },
    async fire(type, ev) {
      let p;
      handlers[type](Object.assign({ waitUntil(x) { p = x; } }, ev));
      if (p && p.then) await p;
      return p;
    },
    async fireFetch(request) {
      let p, called = false;
      handlers.fetch({ request, respondWith(x) { called = true; p = x; } });
      if (called && p && p.then) p = await p;
      return { called, res: p };
    }
  };
}

(async () => {
  /* ---------------- A. 预缓存清单 ---------------- */
  console.log('=== A. 预缓存清单（与 index.html 真正加载的资源对齐）===');
  {
    const e = makeEnv();
    const A = e.ASSETS;

    t('没有用 addAll（用了一个 404 就全盘失败）', !/\.addAll\(/.test(SW), '还在用 addAll');
    t('缓存名带版本号', /medreminder-v\d+/.test(e.CACHE), String(e.CACHE));

    t('★ 含 js/platform/camera.js（原缺失）', A.indexOf('./js/platform/camera.js') >= 0, '缺');
    t('★ 含 js/platform/storage.js（原缺失）', A.indexOf('./js/platform/storage.js') >= 0, '缺');
    t('含 js/platform/notifications.js', A.indexOf('./js/platform/notifications.js') >= 0, '缺');
    t('含 js/app.js', A.indexOf('./js/app.js') >= 0, '缺');
    t('含 css/app.css', A.indexOf('./css/app.css') >= 0, '缺');

    /* index.html 用的是**裸相对路径**（`js/app.js`，没有 `./` 前缀）。
     * 第一版我按 `./` 前缀去解析，结果解析到 0 条 —— 而"全部在清单里"这条
     * **对空集也成立**，于是看着是绿的、其实什么都没验。
     * 幸好加了下面那条"解析器没瞎"的自检把它揪出来了：
     * **空集通过 ≠ 通过**，凡是"筛选后应为空"的断言都必须配一条"筛出来过东西"的自检。 */
    const refs = [];
    const re = /(?:src|href)\s*=\s*["']([^"']+)["']/g;
    let m;
    while ((m = re.exec(HTML_SRC))) {
      const v = m[1];
      if (/^([a-z]+:|\/\/|#|data:)/i.test(v)) continue;      // 外链 / 锚点 / 内联 不算
      refs.push(v.replace(/^\.\//, ''));
    }
    const uniq = Array.from(new Set(refs));
    const ASET = A.map(x => String(x).replace(/^\.\//, ''));
    t('index.html 里解析到本地资源（自检：解析器没瞎）', uniq.length >= 8, '只解析到 ' + uniq.length);
    eq('★ index.html 引用的本地资源全部在清单里', uniq.filter(r => ASET.indexOf(r) < 0), []);

    const vendor = uniq.filter(r => r.indexOf('vendor/') === 0);
    t('index.html 确实引用了 vendor 脚本', vendor.length >= 5, vendor.join(','));
    eq('★ 全部 vendor 脚本都在清单里', vendor.filter(r => ASET.indexOf(r) < 0), []);
    t('含 plugin-camera.js', A.indexOf('./vendor/plugin-camera.js') >= 0, '缺');
    t('含 plugin-filesystem.js', A.indexOf('./vendor/plugin-filesystem.js') >= 0, '缺');
    t('含 plugin-local-notifications.js', A.indexOf('./vendor/plugin-local-notifications.js') >= 0, '缺');
  }

  /* ---------------- B. install ---------------- */
  console.log('');
  console.log('=== B. install：逐个 add，失败只告警不整体崩 ===');
  {
    const e = makeEnv();
    await e.fire('install', {});
    const m = e.stores.get(e.CACHE);
    t('全部资源写进缓存', !!m && m.size === e.ASSETS.length,
      (m ? m.size : 0) + '/' + e.ASSETS.length);
    t('调用了 skipWaiting（新版立刻接管）', e.skipWaiting(), '没调');
    eq('没有告警', e.warns, []);
  }
  {
    const e = makeEnv({ failAssets: ['./js/platform/camera.js'] });
    await e.fire('install', {});
    const m = e.stores.get(e.CACHE);
    t('★ 单个资源 404 不拖垮整批（其余照常写入）',
      !!m && m.size === e.ASSETS.length - 1, (m ? m.size : 0) + '/' + e.ASSETS.length);
    t('★ 失败留下告警（不再静默）', e.warns.some(w => w.indexOf('./js/platform/camera.js') >= 0),
      e.warns.join(' | '));
  }

  /* ---------------- C. activate ---------------- */
  console.log('');
  console.log('=== C. activate：清掉旧版本缓存 ===');
  {
    const e = makeEnv();
    /* 当前版本的缓存也必须先存在 —— 否则 caches.keys() 里根本没有它，
     * "当前版本缓存保留"这条断言测的是幻觉（它本来就不在）。 */
    e.seed(e.CACHE, './js/app.js', 'current');
    e.seed('medreminder-v1', './js/app.js', 'ancient');
    e.seed('medreminder-v2', './js/app.js', 'old');
    await e.fire('activate', {});
    eq('旧缓存被删', e.deleted.sort(), ['medreminder-v1', 'medreminder-v2']);
    t('当前版本缓存保留', e.stores.has(e.CACHE), '被误删了');
    t('调用了 clients.claim()', e.claim(), '没调');
  }

  /* ---------------- D. fetch：核心 = network-first ---------------- */
  console.log('');
  console.log('=== D. fetch：app shell 走 network-first，静态资源走 cache-first ===');
  {
    const e = makeEnv();
    e.seed(e.CACHE, '/js/app.js', 'CACHED-OLD');
    const r = await e.fireFetch(mkReq('/js/app.js'));
    t('★ 缓存里有旧版、网络上有新版 → 返回新版（H-2 本体）',
      r.res && r.res.body === 'net:/js/app.js',
      '返回了 ' + (r.res && r.res.body));
    t('★ 新版本回填进缓存（下次离线也能用）',
      e.cached(e.CACHE, '/js/app.js') && e.cached(e.CACHE, '/js/app.js').body === 'net:/js/app.js',
      '缓存没更新');
  }
  {
    const e = makeEnv();
    e.seed(e.CACHE, '/index.html', 'CACHED-INDEX');
    await e.fireFetch(mkReq('/index.html'));
    t('index.html 也走 network-first', e.cached(e.CACHE, '/index.html').body === 'net:/index.html',
      e.cached(e.CACHE, '/index.html').body);
  }
  {
    const e = makeEnv({ fetchAllFail: true });
    e.seed(e.CACHE, '/js/app.js', 'CACHED-OLD');
    const r = await e.fireFetch(mkReq('/js/app.js'));
    t('★ 离线时回退到缓存', r.res && r.res.body === 'CACHED-OLD', '没回退');
  }
  {
    const e = makeEnv({ fetchAllFail: true });
    e.seed(e.CACHE, './index.html', 'SHELL');
    const r = await e.fireFetch(mkReq('/js/nothing.js'));
    t('★ 离线且该文件没缓存 → 兜底给 index.html', r.res && r.res.body === 'SHELL', '没兜底');
  }
  {
    const e = makeEnv();
    e.seed(e.CACHE, '/icon-192.png', 'ICON-CACHED');
    const r = await e.fireFetch(mkReq('/icon-192.png'));
    t('★ 图片命中缓存 → 直接用缓存（cache-first 保留）', r.res && r.res.body === 'ICON-CACHED', '不是缓存');
    eq('★ 图片命中缓存时**不发网络请求**', e.fetchCalls, []);
  }
  {
    const e = makeEnv();
    const r = await e.fireFetch(mkReq('/icon-192.png'));
    t('图片未命中 → 拉网络并回填',
      r.res && r.res.body === 'net:/icon-192.png' && e.cached(e.CACHE, '/icon-192.png'),
      '没回填');
  }
  {
    const e = makeEnv();
    const r = await e.fireFetch(mkReq('/js/app.js', { mode: 'navigate' }));
    t('导航请求按 shell 处理', r.called && r.res && r.res.body.indexOf('net:') === 0, '');
  }
  {
    const e = makeEnv();
    const r = await e.fireFetch(mkReq('/js/app.js', { method: 'POST' }));
    t('非 GET 不插手', r.called === false, '插手了');
  }
  {
    const e = makeEnv();
    const cross = { __url: '/js/x.js', url: 'https://cdn.example.com/js/x.js', method: 'GET', mode: 'no-cors' };
    const r = await e.fireFetch(cross);
    t('跨域请求不插手（不代理别人的流量）', r.called === false, '插手了');
    eq('跨域时确实没发请求', e.fetchCalls, []);
  }
  {
    /* 策略正确性：shell 用 network-first，是为了"改了代码立刻生效"。
     * 这条用"连续两次 fetch"验证第二次仍取网络、不会被自己刚写的缓存挡住。 */
    const e = makeEnv({ networkBody: { '/js/app.js': 'v1' } });
    const r1 = await e.fireFetch(mkReq('/js/app.js'));
    const r2 = await e.fireFetch(mkReq('/js/app.js'));
    /* 设了 networkBody，返回值就应该是 'v1'（而不是默认的 'net:...'）——
     * 第一版我按默认值断言，自己把自己绊了一跤。 */
    t('★ 连续两次都回源（不会被自己刚写的缓存挡住）',
      r1.res.body === 'v1' && r2.res.body === 'v1' && e.fetchCalls.length === 2,
      'fetch 次数 ' + e.fetchCalls.length + '，body=' + (r2.res && r2.res.body));
  }

  /* ---------------- D2. 超时回退（网络半死时不能卡住） ---------------- */
  console.log('');
  console.log('=== D2. network-first 必须带超时（网络挂起 → 回退缓存，不能白屏干等） ===');
  {
    const e = makeEnv({ fetchHang: true });
    e.seed(e.CACHE, '/js/app.js', 'CACHED');
    const t0 = Date.now();
    const r = await e.fireFetch(mkReq('/js/app.js'));
    const dt = Date.now() - t0;
    t('★ 网络挂起时回退到缓存（不是一直等）',
      r.res && r.res.body === 'CACHED', '拿到 ' + (r.res && r.res.body));
    t('★ 超时在合理范围内（< 3s）', dt < 3000, dt + 'ms');
  }
  {
    const e = makeEnv({ fetchHang: true, });
    e.seed(e.CACHE, './index.html', 'SHELL');
    const r = await e.fireFetch(mkReq('/js/nothing.js'));
    t('★ 超时且无缓存 → 兜底 index.html',
      r.res && r.res.body === 'SHELL', '拿到 ' + (r.res && r.res.body));
  }
  {
    /* 网络正常时不应该白等超时 —— 否则每次加载都慢 1.5 秒 */
    const e = makeEnv();
    const t0 = Date.now();
    const r = await e.fireFetch(mkReq('/js/app.js'));
    const dt = Date.now() - t0;
    t('★ 网络正常时立刻返回，不白等超时',
      r.res && r.res.body === 'net:/js/app.js' && dt < 300, dt + 'ms');
  }

  /* ---------------- E. 源码级接线（防改回去）---------------- */
  console.log('');
  console.log('=== E. 源码级接线（防改回去）===');
  {
    t('★ shell 分支走 networkFirst（而不是直接 cache-first）',
      /isShellRequest\(req, url\)\) \{\s*e\.respondWith\(networkFirst\(req\)\);/.test(SW),
      'shell 分支没走 networkFirst');
    t('★ networkFirst 内部是「先 fetch，失败或超时才 caches.match」',
      /fetch\(req\)\.then[\s\S]{0,400}?catch\(function \(\) \{[\s\S]{0,300}?caches\.match\(req\)/.test(SW),
      '顺序不对');
    t('★ 旧写法「cache-first 无条件先 caches.match」已消失',
      !/e\.respondWith\(\s*caches\.match\(e\.request\)/.test(SW), '旧写法又回来了');
    t('有 isShellRequest 判定', /function isShellRequest\(/.test(SW), '缺');
    t('install 里有 console.warn（不再吞错误）', /\[sw\] 预缓存失败/.test(SW), '缺');
    t('app.js：注册失败不再静默',
      /serviceWorker\.register\('sw\.js'\)[\s\S]{0,300}console\.warn/.test(APP), '还是静默的');
    t('app.js：注册成功后调 update()', /register\('sw\.js'\)\.then[\s\S]{0,80}reg\.update\(\)/.test(APP), '缺');
    t('★ shell 的 network-first 带超时（NET_TIMEOUT_MS 存在且被用上）',
      /var NET_TIMEOUT_MS = \d+;/.test(SW) && /function networkFirst\(req\)/.test(SW)
      && /e\.respondWith\(networkFirst\(req\)\)/.test(SW), '缺超时');
    t('超时分支会回退缓存并兜底 index.html',
      /setTimeout\(function \(\) \{[\s\S]{0,400}?caches\.match\(req\)/.test(SW), '缺回退');
    t('app.js 不再有空的 catch 吞掉注册失败',
      !/register\('sw\.js'\)\.catch\(function \(\) \{ \/\* ignore \*\/ \}\)/.test(APP), '还在吞');
  }

  console.log('');
  console.log('==========================================');
  console.log('通过 %d / 共 %d', pass, pass + fail);
  if (fail) { console.log('失败清单：'); fails.forEach(f => console.log('  - ' + f)); }
  process.exit(fail ? 1 : 0);
})().catch(e => { console.error('用例本身抛错:', e); process.exit(2); });
