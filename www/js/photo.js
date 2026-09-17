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

  /* ---------------- L1 质量校验 + L2 防重复 ----------------
   * 目标不是"认药"，而是**防敷衍**：挡住对着天花板 / 墙面 / 手掌拍，
   * 以及"拍一张照片、之后对着它翻拍"这种做法。
   * 明确**不做药物识别** —— 那需要 ML 模型（几 MB 打进 APK），而且误判会把
   * 正常吃药的人挡在打卡之外，代价比偶尔漏放一张高得多。
   *
   * ⚠️ 总原则：**宁可放行，不可误伤**。
   *   ① 任何一步不确定（canvas 不可用、读像素失败、图片加载超时）都放行；
   *   ② 校验不通过时不落盘（省空间），但「拍不了 · 直接跳过」的逃生通道始终在。
   */
  var QUALITY = {
    minLum: 16,      // 平均亮度低于此 = 基本全黑
    maxLum: 245,     // 高于此 = 过曝 / 对着灯或白墙
    minStd: 5,       // 亮度标准差低于此 = 几乎纯色
    minEdge: 3,      // 平均边缘强度低于此 = 过糊 / 没内容
    dupDist: 6,      // 汉明距离 ≤ 此值视为同一张图（64 位里只差 6 位）
    dupWindow: 30,   // 保留最近多少张指纹用于比对
    loadTimeout: 4000
  };
  var HASH_KEY = 'medreminder.photoHashes.v1';

  function loadImage(url) {
    return new Promise(function (resolve) {
      var done = false;
      function fin(v) { if (!done) { done = true; resolve(v); } }
      var img;
      try { img = new Image(); } catch (e) { return fin(null); }
      if (!img) return fin(null);
      /* 超时也必须放行 —— 不能因为图片解码卡住就让用户打不了卡 */
      var to = setTimeout(function () { fin(null); }, QUALITY.loadTimeout);
      img.onload = function () { clearTimeout(to); fin(img); };
      img.onerror = function () { clearTimeout(to); fin(null); };
      try { img.src = url; } catch (e) { clearTimeout(to); fin(null); }
    });
  }

  function luma(r, g, b) { return 0.299 * r + 0.587 * g + 0.114 * b; }

  /* 一次解码，算出三件事：平均亮度、亮度标准差、平均边缘强度；外加 64 位 dHash 指纹。
   * 返回 null 表示"分析不了"→ 调用方一律放行。 */
  function analyze(img) {
    try {
      if (!img || !document.createElement) return null;
      var c = document.createElement('canvas');
      if (!c || !c.getContext) return null;
      c.width = 64; c.height = 64;
      var ctx = c.getContext('2d');
      if (!ctx) return null;
      ctx.drawImage(img, 0, 0, 64, 64);
      var px = ctx.getImageData(0, 0, 64, 64).data;
      var N = 64 * 64, lum = new Array(N), i, sum = 0;
      for (i = 0; i < N; i++) {
        var v = luma(px[i * 4], px[i * 4 + 1], px[i * 4 + 2]);
        lum[i] = v; sum += v;
      }
      var mean = sum / N, vs = 0;
      for (i = 0; i < N; i++) { var d = lum[i] - mean; vs += d * d; }
      var std = Math.sqrt(vs / N);
      /* 边缘强度：与右邻、下邻的绝对差均值（够用的近似，不必上 Sobel） */
      var edge = 0, cnt = 0;
      for (var y = 0; y < 64; y++) {
        for (var x = 0; x < 64; x++) {
          var k = y * 64 + x;
          if (x < 63) { edge += Math.abs(lum[k + 1] - lum[k]); cnt++; }
          if (y < 63) { edge += Math.abs(lum[k + 64] - lum[k]); cnt++; }
        }
      }
      edge = cnt ? edge / cnt : 0;

      /* 指纹：9×8 差分哈希（dHash）。对整体亮度变化不敏感，
       * 专治"翻拍旧照片 / 对着屏幕拍"这类高度相似的画面。 */
      var hash = '';
      try {
        var c2 = document.createElement('canvas');
        c2.width = 9; c2.height = 8;
        var x2 = c2.getContext('2d');
        if (x2) {
          x2.drawImage(img, 0, 0, 9, 8);
          var d2 = x2.getImageData(0, 0, 9, 8).data;
          var bits = '';
          for (var yy = 0; yy < 8; yy++) {
            for (var xx = 0; xx < 8; xx++) {
              var p1 = (yy * 9 + xx) * 4, p2 = (yy * 9 + xx + 1) * 4;
              bits += (luma(d2[p1], d2[p1 + 1], d2[p1 + 2]) > luma(d2[p2], d2[p2 + 1], d2[p2 + 2])) ? '1' : '0';
            }
          }
          for (var q = 0; q < 16; q++) hash += parseInt(bits.substr(q * 4, 4), 2).toString(16);
        }
      } catch (e2) { hash = ''; }   // 指纹失败不影响质量判定

      return { lum: mean, std: std, edge: edge, hash: hash };
    } catch (e) {
      return null;   // 读像素被拒（跨域等）→ 放行
    }
  }

  function qualityIssue(a) {
    if (!a) return null;                                   // 分析不了 → 放行
    if (a.lum < QUALITY.minLum) return { code: 'dark', msg: '画面太暗，看不出拍的是什么' };
    if (a.lum > QUALITY.maxLum) return { code: 'bright', msg: '画面几乎全白，是不是对着灯或白墙' };
    if (a.std < QUALITY.minStd) return { code: 'flat', msg: '画面几乎是单色，没有内容' };
    if (a.edge < QUALITY.minEdge) return { code: 'blur', msg: '画面太糊，看不清' };
    return null;
  }

  function loadHashes() {
    try { var r = localStorage.getItem(HASH_KEY); return r ? (JSON.parse(r) || []) : []; } catch (e) { return []; }
  }
  function rememberHash(h) {
    if (!h) return;
    try {
      var l = loadHashes();
      l.unshift({ h: h, at: Date.now() });
      localStorage.setItem(HASH_KEY, JSON.stringify(l.slice(0, QUALITY.dupWindow)));
    } catch (e) { /* 存不下就算了，不影响打卡 */ }
  }
  function hamming(a, b) {
    if (!a || !b || a.length !== b.length) return 999;
    var n = 0;
    for (var i = 0; i < a.length; i++) {
      var x = parseInt(a[i], 16) ^ parseInt(b[i], 16);
      while (x) { n += x & 1; x >>= 1; }
    }
    return n;
  }
  function isDuplicate(h) {
    if (!h) return false;
    var l = loadHashes();
    for (var i = 0; i < l.length; i++) {
      if (l[i] && hamming(h, l[i].h) <= QUALITY.dupDist) return true;
    }
    return false;
  }

  function displayUrl(p) {
    var c = window.Capacitor;
    return (c && typeof c.convertFileSrc === 'function') ? c.convertFileSrc(p) : p;
  }

  /* 校验 → 落盘。校验不过就**不落盘**（别让垃圾照片占空间），
   * 把原因交回调用方去提示重拍 —— 逃生通道（跳过）始终可用。 */
  function checkAndPersist(photoPath, rel) {
    return loadImage(displayUrl(photoPath)).then(function (img) {
      var a = analyze(img);
      var q = qualityIssue(a);
      if (q) return { ok: false, reason: 'low-quality', code: q.code, msg: q.msg };
      if (a && isDuplicate(a.hash)) {
        return { ok: false, reason: 'duplicate', msg: '和最近拍过的某张几乎一模一样（是不是用了旧照片）' };
      }
      return persist(photoPath, rel).then(function () {
        if (a && a.hash) rememberHash(a.hash);
        return { ok: true, rel: rel, hash: a ? a.hash : '' };
      });
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
      /* ⚠️ 这里必须是**全大写**的 'CAMERA'。
       * Java 侧是 `settings.setSource(CameraSource.valueOf(call.getString("source", ...)))`，
       * 枚举名全大写；小写会抛 IllegalArgumentException 被 catch 掉、**静默降级成 PROMPT**，
       * 表现就是每次拍照都弹出「从相册选择 / 拍照」的底部选择框。
       * 需求是「只允许现场拍」，所以这里既不能写小写、也不要传 promptLabel* 参数
       * （那些只在 PROMPT 模式用，传了反而像在暗示要走选择框）。 */
      source: 'CAMERA',
      quality: 60,                 // 药盒上的药名要能看清；再低就可能糊到没法辨认
      width: 1024,
      correctOrientation: true,    // 不加这个，Android 上竖拍的照片可能横过来
      saveToGallery: false         // 存系统相册要存储权限，而且隐私差
    }).then(function (photo) {
      if (!photo || !photo.path) return { ok: false, reason: 'error', msg: 'no path' };
      return checkAndPersist(photo.path, rel);
    }).catch(function (e) {
      var m = msgOf(e);
      if (/cancel/i.test(m)) return { ok: false, reason: 'cancelled' };
      return { ok: false, reason: 'error', msg: m };
    });
  }

  /* appRestoredResult 兜底用：App 被系统杀掉后恢复时，照片数据从这里取。
   * （Capacitor 官方明确要求监听 appRestoredResult，否则会同时丢照片和打卡。） */
  /* ⚠️ 恢复路径**刻意不做质量/重复校验**：用户已经被系统杀掉一次 App，
   * 不该再被拦一道。那条路同样必经相机，作弊成本已经付过。 */
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
    gc: gc,
    /* 供诊断与测试：阈值 / 指纹条数 / 单张分析 */
    QUALITY: QUALITY,
    hashCount: function () { return loadHashes().length; },
    analyze: analyze,
    qualityIssue: qualityIssue,
    hamming: hamming,
    isDuplicate: isDuplicate
  };
})();
