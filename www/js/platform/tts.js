/* 语音播报药名（S-10）—— 平台层封装
 *
 * 出口：window.MedTts = { speak, stop, enabled, setEnabled, supported }
 *
 * 三层实现，按可用性自动降级：
 *   ① 原生插件 MedTts（Android TextToSpeech / 鸿蒙 TTS Kit）—— 主路径；
 *   ② window.speechSynthesis（桌面浏览器可测；Android WebView 不支持）；
 *   ③ 都没有 → supported=false，speak 静默返回 false —— 播报是锦上添花，
 *      绝不能影响打卡主流程（与 shadow/backup 同一纪律）。
 *
 * 开关：localStorage 'medreminder.tts.v1'（'on' | 'off'），默认 **on** ——
 *   这个功能就是为"看不清屏幕"的老人设计的，装上了就该响；
 *   不想要的人去「显示设置」一键关掉。与字号/主题同一存法（独立 key，不进主状态）。
 */
(function () {
  'use strict';

  var KEY = 'medreminder.tts.v1';
  var SCHEMA = 'medreminder.tts.state.v1';

  var C = window.Capacitor;
  var IS_NATIVE = !!(C && typeof C.getPlatform === 'function' && C.getPlatform() !== 'web');
  var WEB_TTS = ('speechSynthesis' in window) ? window.speechSynthesis : null;

  /* ---- 插件解析（2026-10-20 真机反馈「TTS 不起作用」的直接修复） ----
   * MedTts 是**本项目自写、没有 JS 包装**的插件：正常情况下由原生侧
   * JSExport 注入成 Capacitor.Plugins.MedTts；万一那份注入没生效（本机就是），
   * 必须用 core 的 registerPlugin('MedTts') 建代理 —— 只要有 PluginHeaders 就能调通。
   * 这套兜底在 notifications.js 的 settingsPlugin() 里已经验证过，这里照抄。
   * 原实现只查 Capacitor.Plugins.MedTts → 注入没生效时 supported()=false，
   * 表现为「开关打开、到点不响、也不报错」。 */
  var NATIVE = null;
  function hasHeader(name) {
    var hs = (C && C.PluginHeaders) || [];
    for (var i = 0; i < hs.length; i++) if (hs[i] && hs[i].name === name) return true;
    return false;
  }
  function pluginMode() {
    if (!IS_NATIVE) return 'web';
    var P = C && C.Plugins && C.Plugins.MedTts;
    if (P && typeof P.speak === 'function') return 'injected';
    if (hasHeader('MedTts') && typeof C.registerPlugin === 'function') return 'proxy';
    return 'missing';
  }
  function getNative() {
    if (NATIVE) return NATIVE;
    if (!IS_NATIVE) return null;
    var P = C && C.Plugins && C.Plugins.MedTts;
    if (P && typeof P.speak === 'function') { NATIVE = P; return NATIVE; }
    if (hasHeader('MedTts') && typeof C.registerPlugin === 'function') {
      try {
        var g = C.registerPlugin('MedTts');
        if (g && typeof g.speak === 'function') { NATIVE = g; return NATIVE; }
      } catch (e) { /* 插件不存在时会抛，忽略 */ }
    }
    return null;
  }

  /* 最近一次原生状态（诊断卡用） */
  var st = { mode: '', ready: null, available: null, at: 0, err: '' };
  function probe() {
    var P = getNative();
    if (!P) { st = { mode: pluginMode(), ready: null, available: null, at: Date.now(), err: '插件未注册' }; return Promise.resolve(st); }
    var p = null;
    try { p = P.status(); } catch (e) { p = null; }
    if (!p || typeof p.then !== 'function') {
      st = { mode: pluginMode(), ready: null, available: null, at: Date.now(), err: 'status 不可用' };
      return Promise.resolve(st);
    }
    return p.then(function (r) {
      st = { mode: pluginMode(), ready: !!(r && r.ready), available: !!(r && r.ttsAvailable), at: Date.now(), err: '' };
      return st;
    }).catch(function (e) {
      st = { mode: pluginMode(), ready: false, available: false, at: Date.now(), err: (e && e.message) || 'status 失败' };
      return st;
    });
  }

  /* 预热：插件是**懒加载**的 —— 第一次调用才触发原生 TtsPlugin.load() 与
   * TextToSpeech 的异步 onInit。不预热的话「第一条提醒」必然念不出来
   * （ready 还是 false）。启动时探一次，把引擎叫醒。 */
  try { setTimeout(function () { probe(); }, 600); } catch (e) { /* vm 沙箱无定时器 */ }

  function enabled() {
    try { return localStorage.getItem(KEY) !== 'off'; }   // 缺省 = on
    catch (e) { return true; }
  }
  function setEnabled(v) {
    try { localStorage.setItem(KEY, v ? 'on' : 'off'); } catch (e) { /* ignore */ }
  }

  function supported() {
    return !!(getNative() || WEB_TTS);
  }

  /* 播报一段文本。返回 true = 已交出去（不保证播完），false = 这边没有能力。
   * ⚠️ 任何异常都吞掉 —— 调用方（提醒弹窗）不等待、不重试。 */
  function speak(text) {
    if (!enabled() || !supported()) return false;
    var t = String(text == null ? '' : text).trim();
    if (!t) return false;
    try {
      var P = getNative();
      if (P && typeof P.speak === 'function') {
        var p = P.speak({ text: t });
        if (p && typeof p.then === 'function') {
          p.then(function (r) {
            /* 引擎还在初始化（首次调用/刚开机）→ 800ms 后补一次。
             * 只在 ready===false 时补：那意味着这句话根本没念出去，补一次不会重音。 */
            if (r && r.ok === false && r.ready === false) {
              setTimeout(function () {
                try { var pp = P.speak({ text: t }); if (pp && pp.catch) pp.catch(function () {}); } catch (e) { /* ignore */ }
              }, 800);
            }
            if (r && typeof r.ready === 'boolean') { st.ready = r.ready; st.at = Date.now(); }
          }).catch(function () { /* ignore */ });
        }
        return true;
      }
      if (WEB_TTS) {
        var u = new SpeechSynthesisUtterance(t);
        u.lang = 'zh-CN';
        u.rate = 0.95;                     // 念药名稍慢一点，老人跟得上
        WEB_TTS.speak(u);
        return true;
      }
    } catch (e) { /* ignore */ }
    return false;
  }

  function stop() {
    try {
      var P = getNative();
      if (P && typeof P.stop === 'function') { P.stop(); return; }
      if (WEB_TTS) WEB_TTS.cancel();
    } catch (e) { /* ignore */ }
  }

  window.MedTts = {
    speak: speak,
    stop: stop,
    enabled: enabled,
    setEnabled: setEnabled,
    supported: supported,
    probe: probe,          // 返回 Promise<{mode, ready, available, err}>，诊断卡用
    lastStatus: function () { return st; },
    KEY: KEY,
    SCHEMA: SCHEMA
  };
})();
