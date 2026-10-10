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

  var C = window.Capacitor;
  var NATIVE = (C && C.Plugins && C.Plugins.MedTts) || null;
  var WEB_TTS = ('speechSynthesis' in window) ? window.speechSynthesis : null;

  function enabled() {
    try { return localStorage.getItem(KEY) !== 'off'; }   // 缺省 = on
    catch (e) { return true; }
  }
  function setEnabled(v) {
    try { localStorage.setItem(KEY, v ? 'on' : 'off'); } catch (e) { /* ignore */ }
  }

  function supported() {
    return !!(NATIVE || WEB_TTS);
  }

  /* 播报一段文本。返回 true = 已交出去（不保证播完），false = 这边没有能力。
   * ⚠️ 任何异常都吞掉 —— 调用方（提醒弹窗）不等待、不重试。 */
  function speak(text) {
    if (!enabled() || !supported()) return false;
    var t = String(text == null ? '' : text).trim();
    if (!t) return false;
    try {
      if (NATIVE && typeof NATIVE.speak === 'function') {
        NATIVE.speak({ text: t });
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
      if (NATIVE && typeof NATIVE.stop === 'function') { NATIVE.stop(); return; }
      if (WEB_TTS) WEB_TTS.cancel();
    } catch (e) { /* ignore */ }
  }

  window.MedTts = {
    speak: speak,
    stop: stop,
    enabled: enabled,
    setEnabled: setEnabled,
    supported: supported,
    KEY: KEY
  };
})();
