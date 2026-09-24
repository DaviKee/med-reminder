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
var CACHE = 'medreminder-v3';

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
  './js/notify.js',
  './js/photo.js',
  './js/backup.js',
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

self.addEventListener('fetch', function (e) {
  var req = e.request;
  if (req.method !== 'GET') return;

  var url;
  try { url = new URL(req.url); } catch (err) { return; }
  if (url.origin !== self.location.origin) return;   // 跨域（远程图标等）不插手

  if (isShellRequest(req, url)) {
    /* network-first：拿得到就用新的（并回填缓存）；拿不到才退回缓存。
     * 这是修「装了新版仍是旧代码」的关键一步。 */
    e.respondWith(
      fetch(req).then(function (res) { return putInCache(req, res); })
        .catch(function () {
          return caches.match(req).then(function (hit) {
            return hit || caches.match('./index.html');
          });
        })
    );
    return;
  }

  /* 静态资源：cache-first */
  e.respondWith(
    caches.match(req).then(function (hit) {
      return hit || fetch(req).then(function (res) { return putInCache(req, res); });
    })
  );
});
