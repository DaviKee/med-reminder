/* 离线缓存 —— 两条铁律：
 *
 *   ① **app shell（HTML / JS / CSS / 插件）必须 network-first。**
 *      此前是纯 cache-first：一旦缓存过就**永远命中缓存、从不回源**，
 *      表现是「装了新版、界面还是旧的」—— 连"到底哪个版本在跑"都查不出来。
 *   ② **预缓存清单必须与 index.html 真正加载的文件一一对应。**
 *      漏一个就离线白屏；而漏掉 plugin-*.js 更隐蔽 ——
 *      离线时 Capacitor 插件全是 undefined，调用**静默降级**、不报错。
 *
 * 缓存名带版本号：改了清单/策略就 +1，activate 会把旧缓存整体删掉。
 */
/* ⚠️ 加 ES module 时：ASSETS 也要加 —— module 是 import 进来的，
 *    不在 index.html 里，而 ASSETS 是「与 index.html 一一对应」的清单。
 *    （index.html 里同时还留了 <link rel="modulepreload"> 让这条对应关系可被自动核对。） */
var CACHE = 'medreminder-v44';

/* 顺序与 index.html 的 <script>/<link> 保持一致。
 * ⚠️ 加新插件时：这里、build-apk.sh、index.html 三处都要加（见项目 MEMORY §2）。 */
var ASSETS = [
  './',
  './index.html',
  './manifest.webmanifest',
  './css/app.css',
  './vendor/capacitor.js',
  './vendor/plugin-app.js',
  './vendor/plugin-local-notifications.js',
  './vendor/plugin-camera.js',
  './vendor/plugin-filesystem.js',
  './js/platform/notifications.js',
  './js/platform/camera.js',
  './js/platform/storage.js',
  './js/platform/lifecycle.js',
  './js/core/util.js',
  './js/core/store.js',
  './js/core/schedule.js',
  './js/core/cardSummary.js',
  './js/ui/tabs.js',
  './js/ui/render.js',
  './js/ui/cards.js',
  './js/ui/actions.js',
  './js/ui/toast.js',
  './js/ui/overlay.js',
  './js/ui/photo.js',
  './js/ui/permission.js',
  './js/ui/fontsize.js',
  './js/ui/theme.js',
  './js/ui/sheet.js',
  './js/ui/today.js',
  './js/ui/data.js',
  './js/ui/records.js',
  './js/ui/viz.js',
  './js/ui/meds.js',
  './js/app.js',
  './icon.svg',
  './icon-maskable.svg',
  './icon-192.png',
  './icon-512.png',
  './icon-maskable-192.png',
  './icon-maskable-512.png'
];

/* 判定"活文件"：这些改了必须立刻生效 → network-first。
 * 其余（图标等）走 cache-first —— 它们几乎不变，且整体由 CACHE 版本号兜底。 */
function isShellRequest(req, url) {
  if (req.mode === 'navigate') return true;
  var p = url.pathname;
  if (p === '/' || p.slice(-1) === '/') return true;
  return p.indexOf('/index.html') >= 0
      || p.indexOf('/sw.js') >= 0
      || p.indexOf('/manifest.webmanifest') >= 0
      || p.indexOf('/css/') >= 0
      || p.indexOf('/js/') >= 0
      || p.indexOf('/vendor/') >= 0;
}

self.addEventListener('install', function (e) {
  e.waitUntil(
    caches.open(CACHE).then(function (c) {
      /* 逐个 add，不用 addAll：addAll 只要有一个 404 就**整体失败且什么都不说**，
       * 那正是"离线打开白屏却没人知道为什么"的来源。 */
      return Promise.all(ASSETS.map(function (u) {
        return c.add(u).catch(function (err) {
          console.warn('[sw] 预缓存失败：' + u + ' — ' + (err && err.message));
        });
      }));
    }).then(function () { return self.skipWaiting(); })
      .catch(function (err) { console.warn('[sw] install 失败：' + (err && err.message)); })
  );
});

self.addEventListener('activate', function (e) {
  e.waitUntil(
    caches.keys().then(function (keys) {
      return Promise.all(keys.map(function (k) {
        if (k === CACHE) return null;
        console.log('[sw] 清理旧缓存：' + k);
        return caches.delete(k);
      }));
    }).then(function () { return self.clients.claim(); })
  );
});

/* 回填缓存。⚠️ 只回填成功响应 —— 把 404/不透明响应写进缓存会让"没坏"看起来像"坏了"。 */
function putInCache(req, res) {
  if (res && res.ok && res.type !== 'opaque') {
    var copy = res.clone();
    caches.open(CACHE).then(function (c) { c.put(req, copy); }).catch(function () {});
  }
  return res;
}

/* network-first：拿得到就用新的（并回填缓存）；拿不到才退回缓存。
 * 这是修「装了新版仍是旧代码」的关键一步。
 *
 * ⚠️ **必须带超时**：纯 network-first 在网络"半死"（连得上但不回包、或门户劫持）时
 * 会让启动一直等下去 —— 表现是白屏/卡住。超时后先用缓存把界面撑起来，
 * 网络那一份回来后再悄悄回填，下次就快了。 */
var NET_TIMEOUT_MS = 1500;
function networkFirst(req) {
  return new Promise(function (resolve) {
    var settled = false;
    var timer = setTimeout(function () {
      if (settled) return;
      settled = true;
      /* 超时 → 先用缓存顶上（拿不到就兜底 index.html） */
      caches.match(req).then(function (hit) { resolve(hit || caches.match('./index.html')); });
    }, NET_TIMEOUT_MS);
    fetch(req).then(function (res) {
      if (settled) { putInCache(req, res); return; }   // 已用缓存回过了：后台补写，下次更快
      settled = true; clearTimeout(timer);
      resolve(putInCache(req, res));
    }).catch(function () {
      if (settled) return;
      settled = true; clearTimeout(timer);
      caches.match(req).then(function (hit) { resolve(hit || caches.match('./index.html')); });
    });
  });
}

self.addEventListener('fetch', function (e) {
  var req = e.request;
  if (req.method !== 'GET') return;

  var url;
  try { url = new URL(req.url); } catch (err) { return; }
  if (url.origin !== self.location.origin) return;   // 跨域（远程图标等）不插手

  if (isShellRequest(req, url)) {
    e.respondWith(networkFirst(req));
    return;
  }

  /* 静态资源：cache-first */
  e.respondWith(
    caches.match(req).then(function (hit) {
      return hit || fetch(req).then(function (res) { return putInCache(req, res); });
    })
  );
});
