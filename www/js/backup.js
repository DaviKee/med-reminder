/* 自动本地备份 —— 「每次保存都往本地存一份」
 *
 * 需求：「我每次保存的时候应该能全部导出来数据，然后本地也保存一份，
 *        而且每次保存的时候本地也得保存。」
 *
 * 目录选择（这是本模块最关键的决策）：
 *   本 App 的 Manifest 里**没有任何存储权限**，而 Android 10+ 的 scoped storage
 *   下没有权限写公共目录（Documents/Download）会直接失败。
 *   所以主路径用 Directory.External —— **app 专属外部目录**
 *   （/storage/emulated/0/Android/data/<包名>/files/），它：
 *     · 不需要任何权限
 *     · 「清除应用数据 / 清除缓存」**不会**碰它（这正是备份的意义）
 *     · 可以用电脑 USB 打开
 *   ⚠️ 但**卸载 App 会连它一起删**。所以换机 / 重装前仍须手动导出一份到别处 ——
 *      这一点在记录页的卡片上明确写给用户看，不含糊。
 *   万一 External 不可用（个别机型），退到 Directory.Data（应用私有目录，一定能写）。
 *
 * 三条工程约束：
 *   ① save() 调用非常频繁（打卡、跳过、改设置…）→ 必须**防抖**，不能每次都写盘
 *   ② 写文件是 IO，**任何失败都不能影响打卡** → 全程 catch，只把状态记下来
 *   ③ 备份格式与手动导出**完全一致** → 现有恢复流程能直接读回来
 */
(function () {
  'use strict';

  var C = window.Capacitor;
  var FS = (C && C.Plugins && C.Plugins.Filesystem) || null;

  var DIR = 'MedReminder';
  var KEEP = 14;                    // 保留最近多少份「日备份」
  var DEBOUNCE_MS = 3000;
  var STATUS_KEY = 'medreminder.autoBackup.v1';

  var getText = null;               // 调用方给的「取当前数据」函数
  var timer = null;
  var busy = false;
  var dirty = false;
  var st = loadStatus();

  function loadStatus() {
    try {
      var r = localStorage.getItem(STATUS_KEY);
      var o = r ? JSON.parse(r) : null;
      return (o && typeof o === 'object') ? o : {};
    } catch (e) { return {}; }
  }
  function saveStatus() {
    try { localStorage.setItem(STATUS_KEY, JSON.stringify(st)); } catch (e) { /* ignore */ }
  }

  function p2(n) { return String(n).padStart(2, '0'); }
  function dayStr(d) { return d.getFullYear() + p2(d.getMonth() + 1) + p2(d.getDate()); }
  function stamp(ms) {
    var d = new Date(ms);
    return d.getFullYear() + '-' + p2(d.getMonth() + 1) + '-' + p2(d.getDate())
      + ' ' + p2(d.getHours()) + ':' + p2(d.getMinutes());
  }

  /* 目标目录，按顺序尝试。两个都不需要「存储权限」，这是选它们的原因。 */
  var DIRS = ['EXTERNAL', 'DATA'];

  function writeOne(dir, name, text) {
    /* ⚠️ writeFile 不会自动建父目录（父目录不存在会 reject
     * 「Parent folder doesn't exist」）—— 必须先 mkdir；已存在会抛，忽略即可。 */
    return FS.mkdir({ path: DIR, directory: dir, recursive: true })
      .catch(function () { /* 已存在 */ })
      .then(function () {
        return FS.writeFile({
          path: DIR + '/' + name, data: text, directory: dir, encoding: 'utf8'
        });
      });
  }

  /* 清理旧的日备份，只留最近 KEEP 份。
   * 失败不算错 —— 顶多占一点空间，不能因此让备份本身失败。 */
  function prune(dir) {
    if (typeof FS.readdir !== 'function') return Promise.resolve(null);
    return FS.readdir({ path: DIR, directory: dir })
      .then(function (r) {
        var names = ((r && r.files) || []).map(function (f) {
          return typeof f === 'string' ? f : (f && f.name);
        }).filter(function (n) { return /^backup-\d{8}\.json$/.test(n || ''); });
        names.sort();
        var drop = names.slice(0, Math.max(0, names.length - KEEP));
        return drop.reduce(function (p, n) {
          return p.then(function () {
            return FS.deleteFile({ path: DIR + '/' + n, directory: dir }).catch(function () {});
          });
        }, Promise.resolve()).then(function () { return names.length - drop.length; });
      })
      .catch(function () { return null; });
  }

  function attempt(dirIndex) {
    if (dirIndex >= DIRS.length) return Promise.resolve(null);
    var dir = DIRS[dirIndex];
    var text;
    try { text = getText(); } catch (e) { text = ''; }
    if (!text) return Promise.resolve(null);

    var day = dayStr(new Date());
    return writeOne(dir, 'latest.json', text)
      .then(function () { return writeOne(dir, 'backup-' + day + '.json', text); })
      .then(function () { return prune(dir).then(function (n) { return { dir: dir, count: n }; }); })
      .catch(function () { return attempt(dirIndex + 1); });     // 换下一个目录试
  }

  function doWrite() {
    if (!FS || !getText) {
      st = { ok: false, at: Date.now(), err: '没有文件系统插件' };
      saveStatus();
      return Promise.resolve(st);
    }
    return Promise.resolve()
      .then(function () { return attempt(0); })
      .then(function (r) {
        st = r
          ? { ok: true, at: Date.now(), dir: r.dir, count: r.count, err: '' }
          : { ok: false, at: Date.now(), err: '所有可写目录都失败了' };
        saveStatus();
        return st;
      })
      .catch(function (e) {
        st = { ok: false, at: Date.now(), err: (e && e.message) || '未知错误' };
        saveStatus();
        return st;
      });
  }

  /* 串行化：正在写的时候又来请求，就记个 dirty，写完补一次。
   * 否则两次写同一文件会互相交错，落地的是哪一份说不准。
   * 并发调用**返回同一个 Promise**（而不是各自拿到一个没写完的状态对象）——
   * 否则调用方 await 到的形状时有时无，没法用。 */
  var pending = null;
  function run() {
    if (busy) {
      dirty = true;
      return pending || Promise.resolve(status());
    }
    busy = true;
    dirty = false;
    var p = doWrite().then(function (r) {
      busy = false;
      if (dirty) { dirty = false; pending = null; return run(); }   // 写期间又改了 → 补一次
      pending = null;
      return r;
    }, function () {
      busy = false; pending = null;
      return status();
    });
    pending = p;
    return p;
  }

  /* 防抖排程。save() 一次打卡会连调好几次，每次写盘既浪费也拖慢界面。 */
  function schedule(getter) {
    if (typeof getter === 'function') getText = getter;
    if (!FS || !getText) return;
    clearTimeout(timer);
    timer = setTimeout(function () { timer = null; run(); }, DEBOUNCE_MS);
  }

  /* 有待写的就立刻写（App 切到后台时用） */
  function flush() {
    if (timer) { clearTimeout(timer); timer = null; return run(); }
    return Promise.resolve(st);
  }

  function now() {
    clearTimeout(timer);
    timer = null;
    return run();
  }

  function status() {
    return {
      supported: !!FS,
      ok: !!st.ok,
      at: st.at || 0,
      dir: st.dir || '',
      count: typeof st.count === 'number' ? st.count : null,
      err: st.err || ''
    };
  }

  window.MedAutoBackup = {
    schedule: schedule,
    flush: flush,
    now: now,
    status: status,
    stamp: stamp,
    DIR: DIR,
    KEEP: KEEP,
    DEBOUNCE_MS: DEBOUNCE_MS
  };
})();
