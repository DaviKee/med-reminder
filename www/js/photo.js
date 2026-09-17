/* 拍照打卡 —— 照片能力层
 *
 * 与 notify.js 同样的定位：把原生插件差异收在这一层，app.js 只管打卡流程。
 *
 * 关键决策与理由：
 * ① 用 @capacitor/camera 且 **不设 saveToGallery** —— 那样**不需要任何存储权限**
 *    （它通过 Intent 调系统相机 Activity，不是自己开摄像头）。
 * ② resultType 用 Uri 而不是 Base64 —— 官方最佳实践写的是
 *    `resultType: Uri, // Not Base64 for large images`，大字符串过 bridge 有内存压力。
 * ③ 照片存 Directory.Data（应用私有目录，**无需运行时权限**，随卸载清理），
 *    **绝不进 localStorage**：压缩后一张 base64 约 110-270 KB，5 MB 上限只够 18-45 张，
 *    一天三次药 6-15 天就满，而 save() 配额满时是静默失败的（D-1 才刚补上可见化）。
 * ④ 照片**不进备份** —— 备份仍是 JSON。几十 MB 照片会让「剪贴板是主路径」这个前提失效。
 * ⑤ gc() 回收孤儿照片：用户清理旧记录后，对应文件必须跟着删，否则 D-1 的体积治理失效。
 */
(function () {
  'use strict';

  /* 插件惰性获取，不在加载时把引用定死 —— 这样即使脚本顺序变了、
   * 或插件注册晚于本文件，也不会被误判成「不可用」。 */
  function plugin(name) {
    var c = window.Capacitor;
    return (c && c.Plugins && c.Plugins[name]) || null;
  }
  function camera() { return plugin('Camera'); }
  function fs() { return plugin('Filesystem'); }

  /* 只看插件是否真的注册进来了。
   *
   * ⚠️ 这里刻意**不用** Capacitor.isNativePlatform() —— 它在 native bridge 里是
   * `const isNativePlatform = () => true`，只表示"跑在 Capacitor 环境里"，
   * **不代表插件可用**。上一版把 NATIVE 建立在它之上，结果判断通过、调用失败，
   * 表现成「点了没反应」，还完全没有报错 —— 真问题是插件的 JS 根本没被加载。 */
  function ready() { return !!(camera() && fs()); }

  var DIR = 'photos';          // Directory.Data 下的子目录
  var PREFIX = 'photos/';      // 记进 dose 的相对路径前缀

  function two(n) { return String(n).padStart(2, '0'); }

  /* 文件名带日期时刻，便于人工排查；后面接 doseId 保证唯一 */
  function stamp() {
    var d = new Date();
    return d.getFullYear() + two(d.getMonth() + 1) + two(d.getDate())
      + '_' + two(d.getHours()) + two(d.getMinutes()) + two(d.getSeconds());
  }

  function relName(doseId) {
    return DIR + '/' + stamp() + '_' + String(doseId || 'x').replace(/[^A-Za-z0-9_-]/g, '') + '.jpg';
  }

  function msgOf(e) { return String((e && (e.message || e.name || e)) || ''); }

  /* 确保 photos 子目录存在 —— **这一步不能省**（最初就是漏了它）。
   *
   * 源码级证据（@capacitor/filesystem）：
   *   · writeFile：父目录不存在且未传 recursive 时 reject「Parent folder doesn't exist」
   *     （FilesystemPlugin.java:110-113，recursive 默认 false）
   *   · copy：**根本不读 recursive**，父目录不存在直接抛
   *     「The parent object of the destination does not exist」（Filesystem.java:135-137）
   * 于是「相机拍成功、落盘失败」，真机上看到的正是这条报错。 */
  var dirReady = false;
  function ensureDir() {
    if (dirReady) return Promise.resolve(true);
    if (!ready()) return Promise.resolve(false);
    return fs().mkdir({ path: DIR, directory: 'DATA', recursive: true })
      .then(function () { dirReady = true; return true; })
      .catch(function (e) {
        /* 已存在是正常情况 —— 插件会抛「Directory exists」（Filesystem.java:83-85），
         * 这时目录本来就是就绪的，不能当成失败。 */
        if (/exist/i.test(msgOf(e))) { dirReady = true; return true; }
        return false;   // 其它原因的失败不阻断：下面的 writeFile 带 recursive 还能兜一次
      });
  }

  /* 把相机产出的临时文件落到私有目录。
   * 优先 copy（不过 JS 层，最省内存）；copy 的 from 走 file:// URI（副本路径由
   * getFileObject 直接解析，因此不能同时传 directory），失败时退回「读出 base64 再写入」。
   * 两条都失败时**把两步的原因都带出来** —— 否则真机上只能看到一个笼统的失败。 */
  function persist(photoPath, rel) {
    var firstErr = '';
    return ensureDir()
      .then(function () { return fs().copy({ from: photoPath, to: rel, toDirectory: 'DATA' }); })
      .then(function () { dirReady = true; })
      .catch(function (e) {
        firstErr = msgOf(e);
        return ensureDir()
          .then(function () { return fs().readFile({ path: photoPath }); })
          .then(function (r) {
            return fs().writeFile({ path: rel, data: r.data, directory: 'DATA', recursive: true });
          })
          .then(function () { dirReady = true; })
          .catch(function (e2) {
            throw new Error('copy: ' + (firstErr || '?') + ' / write: ' + msgOf(e2));
          });
      });
  }

  /* 拍照并落盘。永远 resolve，不 reject —— 调用方只需要看 ok/reason，
   * 不用到处写 catch。（用户取消是最常见的情况，不该当异常处理。） */
  function take(doseId) {
    if (!ready()) return Promise.resolve({ ok: false, reason: 'unsupported' });
    var rel = relName(doseId);
    return camera().getPhoto({
      resultType: 'uri',           // 官方推荐：大图不要用 Base64
      source: 'camera',
      quality: 60,                 // 药盒上的药名要能看清；再低就可能糊到没法辨认
      width: 1024,
      correctOrientation: true,    // 不加这个，Android 上竖拍的照片可能横过来
      saveToGallery: false         // 存系统相册要存储权限，而且隐私差
    }).then(function (photo) {
      if (!photo || !photo.path) return { ok: false, reason: 'error', msg: 'no path' };
      return persist(photo.path, rel).then(function () {
        return { ok: true, rel: rel };
      });
    }).catch(function (e) {
      var m = msgOf(e);
      if (/cancel/i.test(m)) return { ok: false, reason: 'cancelled' };
      return { ok: false, reason: 'error', msg: m };
    });
  }

  /* appRestoredResult 兜底用：App 被系统杀掉后恢复时，照片数据从这里取。
   * （Capacitor 官方明确要求监听 appRestoredResult，否则会同时丢照片和打卡。） */
  function fromRestored(photoPath, doseId) {
    var rel = relName(doseId);
    return persist(photoPath, rel).then(function () {
      return { ok: true, rel: rel };
    }).catch(function (e) {
      return { ok: false, reason: 'error', msg: msgOf(e) };
    });
  }

  /* 相对路径 → <img src> 能用的 URL。webPath 由 Capacitor 转成 WebView 可读形式。 */
  function src(rel) {
    if (!rel) return Promise.resolve('');
    if (!ready()) return Promise.resolve('');
    return fs().getUri({ path: rel, directory: 'DATA' })
      .then(function (r) {
        var c = window.Capacitor;
        return (c && typeof c.convertFileSrc === 'function') ? c.convertFileSrc(r.uri) : r.uri;
      })
      .catch(function () { return ''; });
  }

  /* 照片目录占用。供 D-1 的存储卡纳入统计 —— 否则「已用 X KB / 5 MB」会变成错的，
   * 而那张卡的全部意义就是让容量可见。 */
  function dirStats() {
    if (!ready()) return Promise.resolve({ files: 0, bytes: 0 });
    return fs().readdir({ path: DIR, directory: 'DATA' })
      .then(function (r) {
        var files = 0, bytes = 0;
        (r.files || []).forEach(function (f) {
          if (f.type === 'file') { files++; bytes += (f.size || 0); }
        });
        return { files: files, bytes: bytes };
      })
      .catch(function () { return { files: 0, bytes: 0 }; });   // 目录不存在 = 还没拍过
  }

  /* 回收孤儿照片：只删「没有任何记录引用」的文件。
   * 与 notify.js 的 gcNotified 同一思路 —— 纯垃圾回收，不碰被引用的文件。
   * 清理旧记录后必须跑一次，否则照片会一直占着空间。 */
  function gc(refs) {
    if (!ready()) return Promise.resolve(0);
    var alive = {};
    (refs || []).forEach(function (r) { if (r) alive[r] = 1; });
    return fs().readdir({ path: DIR, directory: 'DATA' })
      .then(function (rd) {
        var orphans = (rd.files || []).filter(function (f) {
          return f.type === 'file' && !alive[PREFIX + f.name];
        });
        /* 串行删，避免并发写文件系统的未知行为 */
        return orphans.reduce(function (p, f) {
          return p.then(function (n) {
            return fs().deleteFile({ path: DIR + '/' + f.name, directory: 'DATA' })
              .then(function () { return n + 1; })
              .catch(function () { return n; });
          });
        }, Promise.resolve(0));
      })
      .catch(function () { return 0; });
  }

  window.MedPhoto = {
    ready: ready,
    take: take,
    fromRestored: fromRestored,
    src: src,
    dirStats: dirStats,
    gc: gc
  };
})();
