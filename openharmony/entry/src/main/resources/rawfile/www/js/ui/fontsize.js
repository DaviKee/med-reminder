/* ui/fontsize.js —— 字号档位：读 / 存 / 应用（--fs）
 *
 * 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 */

/* ---------------- 字号缩放 ----------------
 * 用独立的 localStorage key，不写进主状态 —— 这样备份格式（BACKUP_VERSION 1）完全不用动，
 * 也不会影响导入校验。
 * 实现：只改 CSS 变量 --fs；CSS 里所有 font-size/line-height 都写成 calc(Npx * var(--fs))，
 * 而 padding/margin 保持 px 不变 —— 所以只有文字变大，布局骨架不动。 */
export var FS_KEY = 'medreminder.fontScale.v1';

export var FS_LEVELS = [
  { label: '小', v: 0.9 },
  { label: '标准', v: 1 },
  { label: '大', v: 1.15 },
  { label: '特大', v: 1.3 }
];

export var fontScale = 1;

export function loadFontScale() {
  var raw = null;
  try { raw = localStorage.getItem(FS_KEY); } catch (e) { /* ignore */ }
  var v = parseFloat(raw);
  for (var i = 0; i < FS_LEVELS.length; i++) if (FS_LEVELS[i].v === v) return v;
  return 1;   // 默认「标准」，与改造前逐像素一致
}

export function applyFontScale(v, persist) {
  fontScale = v;
  document.documentElement.style.setProperty('--fs', String(v));
  if (persist) { try { localStorage.setItem(FS_KEY, String(v)); } catch (e) { /* ignore */ } }
}

export function fsLabel() {
  for (var i = 0; i < FS_LEVELS.length; i++) if (FS_LEVELS[i].v === fontScale) return FS_LEVELS[i].label;
  return '标准';
}
