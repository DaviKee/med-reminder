/* 影子状态文件 —— A-3 数据层（《MedReminder-数据层迁移预研》方案 B）
 *
 * 为什么存在：S-9 桌面卡片与 Agent（鸿蒙侧）是**独立进程**，读不到 WebView 的
 * localStorage。本模块在每次 save() 之后，经已有的 Filesystem 插件把主状态
 * JSON **原子写入** Directory.Data/shadow/state.json —— 鸿蒙侧该目录映射应用
 * 沙箱 filesDir，卡片 / Agent 进程按应用共享沙箱，直接 fileIo 读。
 *
 * 设计要点（全部来自预研定稿，别改）：
 *   ① **单一真相源仍在 web 层**（localStorage）—— 影子只是副本，不迁移本体；
 *   ② **原子替换是安全核心**：写 state.json.tmp → rename 覆盖 state.json。
 *      读方任何时候看到的要么是旧完整文件要么是新完整文件，不存在半截 JSON；
 *   ③ **单写多读**：只有主进程（web 层）写它。Agent 二期要写打卡也必须走
 *      「拉起主进程」或写别的文件（inbox），绝不碰这个文件；
 *   ④ **失败全程 catch**：影子是锦上添花，任何失败都不能影响打卡主流程；
 *   ⑤ 落点 Directory.Data（沙箱内私有）—— 与备份分开：备份要用户可见性，
 *      影子要进程可达性，语义不同不混放。
 *
 * 工程纪律照抄 storage.js（自动备份）：防抖 / 串行化（busy+dirty）/ flush。
 * 为什么不塞进 storage.js：备份的目录回退链、日轮转、状态上报都是备份自己的
 * 语义，影子不沾；两个写盘动作共用一个 flush 时机即可。
 */
(function () {
  'use strict';

  var C = window.Capacitor;
  var FS = (C && C.Plugins && C.Plugins.Filesystem) || null;

  var DIR = 'shadow';
  var DIR_ID = 'DATA';              // Directory.Data：应用私有沙箱，双端一致
  var NAME = 'state.json';
  var TMP = 'state.json.tmp';
  var DEBOUNCE_MS = 3000;
  var SCHEMA = 'medreminder.shadow.v1';

  var getState = null;              // 调用方注入：返回 { state, fontScale }
  var timer = null;
  var busy = false;
  var dirty = false;
  var pending = null;
  var st = { ok: false, at: 0, err: '还没写过' };

  /* 原子替换：tmp 写完 → rename 覆盖。rename 不可用或失败 → 退化「先删再直写」
   * （预研 §5.2：读方容忍瞬时 ENOENT，用兜底文案即可，不崩溃）。 */
  function writeAtomic(text) {
    var base = { directory: DIR_ID, encoding: 'utf8' };
    return FS.mkdir({ path: DIR, directory: DIR_ID, recursive: true })
      .catch(function () { /* 已存在会抛，忽略 */ })
      .then(function () {
        if (typeof FS.rename !== 'function') {
          return FS.writeFile({ path: DIR + '/' + NAME, data: text, directory: DIR_ID, encoding: 'utf8' });
        }
        return FS.writeFile({ path: DIR + '/' + TMP, data: text, directory: DIR_ID, encoding: 'utf8' })
          .then(function () {
            return FS.rename({ from: DIR + '/' + TMP, to: DIR + '/' + NAME, directory: DIR_ID });
          })
          .catch(function () {
            // rename 失败：退化为直写。先删旧的（旧内容好过半新半旧，直写本身也基本原子）
            return FS.deleteFile({ path: DIR + '/' + NAME, directory: DIR_ID })
              .catch(function () { /* 不存在，正好 */ })
              .then(function () {
                return FS.writeFile({ path: DIR + '/' + NAME, data: text, directory: DIR_ID, encoding: 'utf8' });
              });
          });
      });
  }

  function doWrite() {
    if (!FS || !getState) {
      st = { ok: false, at: Date.now(), err: '没有文件系统插件' };
      return Promise.resolve(st);
    }
    var payload = null;
    try { payload = getState(); } catch (e) { payload = null; }
    if (!payload || !payload.state) {
      st = { ok: false, at: Date.now(), err: '没有可写的数据' };
      return Promise.resolve(st);
    }
    var text;
    try {
      text = JSON.stringify({
        schema: SCHEMA,
        savedAt: Date.now(),
        state: payload.state,
        fontScale: payload.fontScale
      });
    } catch (e) {
      st = { ok: false, at: Date.now(), err: '序列化失败' };
      return Promise.resolve(st);
    }
    return writeAtomic(text)
      .then(function () { st = { ok: true, at: Date.now(), err: '' }; return st; })
      .catch(function (e) {
        st = { ok: false, at: Date.now(), err: (e && e.message) || '未知错误' };
        return st;
      });
  }

  /* 串行化：与 storage.run 同一套纪律 —— 写盘期间又来请求，记 dirty 写完补一次；
   * 并发调用返回同一个 Promise，调用方拿到的形状才稳定。 */
  function run() {
    if (busy) {
      dirty = true;
      return pending || Promise.resolve(st);
    }
    busy = true;
    dirty = false;
    var p = doWrite().then(function (r) {
      busy = false;
      if (dirty) { dirty = false; pending = null; return run(); }
      pending = null;
      return r;
    }, function () {
      busy = false; pending = null;
      return st;
    });
    pending = p;
    return p;
  }

  /* 防抖排程：save() 一次打卡会连调多次，不能每次都写盘。
   * getter 每次都更新 —— 总是拿最新值。 */
  function schedule(getter) {
    if (typeof getter === 'function') getState = getter;
    if (!FS || !getState) return;
    clearTimeout(timer);
    timer = setTimeout(function () { timer = null; run(); }, DEBOUNCE_MS);
  }

  /* 有待写的就立刻写（App 切到后台时与自动备份同一个 flush 时机） */
  function flush() {
    if (timer) { clearTimeout(timer); timer = null; return run(); }
    return Promise.resolve(st);
  }

  function status() {
    return {
      supported: !!FS,
      ok: !!st.ok,
      at: st.at || 0,
      err: st.err || '',
      schema: SCHEMA,
      dir: DIR
    };
  }

  window.MedShadow = {
    schedule: schedule,
    flush: flush,
    status: status,
    SCHEMA: SCHEMA,
    DIR: DIR,
    NAME: NAME,
    DEBOUNCE_MS: DEBOUNCE_MS
  };
})();
