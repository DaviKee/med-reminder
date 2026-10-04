/* ui/theme.js —— 明暗主题（S-5）：读 / 存 / 应用
 *
 * 与 ui/fontsize.js 同一套路：
 *  · **独立的 localStorage key**，不写进主状态 —— 备份格式（BACKUP_VERSION）完全不用动，
 *    也不会影响导入校验。
 *  · 只改 <html> 上的一个属性，颜色全交给 CSS 变量，**不动布局**。
 *
 * ⚠️ 「跟随系统」的判断**只在这里做**，CSS 里不写 @media (prefers-color-scheme)。
 *    理由：① 用户手动选的必须能覆盖系统；② 判断集中一处才能被 spec 测到；
 *    ③ 免得 CSS 与 JS 各判一次、结果打架。
 */

export var THEME_KEY = 'medreminder.theme.v1';

/* 档位顺序 = 界面上按钮的顺序。「深色」排第一，与现状一致。 */
export var THEME_MODES = [
  { id: 'dark', label: '深色' },
  { id: 'light', label: '浅色' },
  { id: 'sys', label: '跟随系统' }
];

/* 默认「深色」—— 与改造前逐像素一致，老用户升级后观感**零变化**。
 *
 * 刻意**不**默认「跟随系统」：老人手机设置多为浅色，一升级整个界面变白，
 * 第一反应是"App 坏了"。想让 App 跟系统走的人，自己去「记录 → 显示」里选。
 * （零行为变化优先于"更聪明"，这是本项目一贯的取舍。） */
export var themeMode = 'dark';

/* 系统当前是否深色。
 *
 * ⚠️ Android WebView **默认不把 `prefers-color-scheme` 透传给页面**
 *    （Capacitor 官方 issue #4677；官方推荐的补丁是 `@capawesome/capacitor-android-dark-mode-support`）。
 *    所以这里返回 false 是常态 —— 「跟随系统」在 Android 上可能等同「浅色」。
 *    已列入真机验证清单；真要修就是引原生插件，届时**只改这一个函数**。 */
export function systemPrefersDark() {
  try {
    if (!window.matchMedia) return false;
    return window.matchMedia('(prefers-color-scheme: dark)').matches === true;
  } catch (e) {
    return false;
  }
}

export function loadThemeMode() {
  var raw = null;
  try { raw = localStorage.getItem(THEME_KEY); } catch (e) { /* ignore */ }
  for (var i = 0; i < THEME_MODES.length; i++) {
    if (THEME_MODES[i].id === raw) return THEME_MODES[i].id;
  }
  return 'dark';
}

/* 档位 + 系统偏好 → 实际生效的主题（'dark' | 'light'）。
 * 纯函数：spec 直接跑判定矩阵，不碰 DOM。 */
export function resolveTheme(mode, sysDark) {
  if (mode === 'light') return 'light';
  if (mode === 'dark') return 'dark';
  return sysDark ? 'dark' : 'light';
}

export function applyTheme(mode, persist) {
  themeMode = mode || 'dark';
  var eff = resolveTheme(themeMode, systemPrefersDark());
  var el = document.documentElement;

  /* 深色 = `:root` 的默认值 → **不写属性**；只有浅色才写 data-theme。
     这样"没设属性"与"设了深色"是同一个状态，少一处可能不一致的地方。 */
  if (eff === 'light') el.setAttribute('data-theme', 'light');
  else el.removeAttribute('data-theme');

  /* <meta name="theme-color"> 管浏览器 UI / 状态栏底色。
     Capacitor 里状态栏由原生主题控制（meta 不生效），它主要在
     浏览器预览与「添加到主屏」时有用 —— 顺手保持一致，成本为零。 */
  try {
    var tc = document.querySelector('meta[name="theme-color"]');
    if (tc) tc.setAttribute('content', eff === 'light' ? '#F2F3F5' : '#0A0A0A');
  } catch (e) { /* ignore */ }

  if (persist) {
    try { localStorage.setItem(THEME_KEY, themeMode); } catch (e) { /* ignore */ }
  }
}

export function themeLabel() {
  for (var i = 0; i < THEME_MODES.length; i++) {
    if (THEME_MODES[i].id === themeMode) return THEME_MODES[i].label;
  }
  return '深色';
}

/* 系统主题变化时调 onChange（只有「跟随系统」档的实际结果会变）。
 * ⚠️ `addListener` 是废弃 API，但个别 WebView 只认它 → 两条都接。 */
export function watchSystemTheme(onChange) {
  try {
    if (!window.matchMedia) return;
    var mq = window.matchMedia('(prefers-color-scheme: dark)');
    if (mq.addEventListener) mq.addEventListener('change', onChange);
    else if (mq.addListener) mq.addListener(onChange);
  } catch (e) { /* ignore */ }
}

/* 启动时调一次即可：同步 JS 状态 + 挂上系统监听。
 * 首屏的 data-theme 已由 index.html 的内联脚本设好（那里等不及模块加载），
 * 这里补的是 JS 侧状态与后续的跟随。 */
export function initTheme() {
  applyTheme(loadThemeMode(), false);
  watchSystemTheme(function () { applyTheme(themeMode, false); });
}
