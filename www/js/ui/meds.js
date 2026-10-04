/* ui/meds.js —— 药品视图：列表渲染 + 遗留示例药的处理
 *
 * 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 */
import { $, esc } from '../core/util.js';
import { S, save } from '../core/store.js';
import { todayDoseCount } from '../core/schedule.js';
import { render } from './render.js';
import { toast } from './toast.js';
import { ICON, medScheduleLabel } from './cards.js';
import { openSheet } from './sheet.js';

/* 旧版本（≤ 2026-09-14）首次启动会写入两条示例药品，让新用户误以为那是自己的药。
 * 这里只「识别 + 交给用户一键删除」，绝不静默删除 —— 万一同名的是真实药品，静默删掉就是数据事故。 */
export var LEGACY_SAMPLE_NAMES = ['维生素 D3', '阿莫西林'];

export function legacySampleMeds() {
  var used = {};
  Object.keys(S.doses).forEach(function (k) {
    (S.doses[k] || []).forEach(function (d) { used[d.medId] = 1; });
  });
  return S.meds.filter(function (m) {
    return LEGACY_SAMPLE_NAMES.indexOf(m.name) >= 0 && !used[m.id];
  });
}

/* 删除识别出来的示例药品（用户在这张卡上点过才调用，不是自动清） */
export function dropLegacySamples() {
  var ids = legacySampleMeds().map(function (m) { return m.id; });
  if (!ids.length) return;
  S.meds = S.meds.filter(function (m) { return ids.indexOf(m.id) < 0; });
  save(); render();
  toast('已删除 ' + ids.length + ' 个示例药品');
}

/* ---------------- render: MEDS ---------------- */
export function renderMeds() {
  var host = $('#medsView');
  var html = ''
    + '<div class="card-row" style="padding:0 0 4px">'
    + '<h1 class="h1">我的药品</h1>'
    + '<button class="icon-btn" id="btnAdd" aria-label="添加药品">'
    + '<svg width="18" height="18" viewBox="0 0 24 24" fill="none"><path d="M12 5v14M5 12h14" style="stroke:var(--text)" stroke-width="1.8" stroke-linecap="round"/></svg>'
    + '</button></div>';

  if (!S.meds.length) {
    html += '<div class="card"><p class="body" style="text-align:center">还没有药品。点右上角 + 添加第一个。</p></div>';
  } else {
    html += '<div class="list">';
    S.meds.forEach(function (m) {
      html += '<button class="meditem" data-med="' + m.id + '">'
        + '<span class="badge badge-36" style="color:#DADBDF">' + ICON.pill + '</span>'
        + '<span class="medinfo"><span class="n">' + esc(m.name) + '</span>'
        + '<span class="m">' + esc(medScheduleLabel(m)) + ' · ' + (todayDoseCount(m.id) ? ('今日 ' + todayDoseCount(m.id) + ' 次') : '未排程') + '</span></span>'
        + ICON.chev + '</button>';
    });
    html += '</div>';
  }
  html += '<p class="hint" style="margin-top:-8px">点任意药品，可以改名称、改服药间隔，或者删掉它。</p>';

  var legacy = legacySampleMeds();
  if (legacy.length) {
    html += '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
      + '<span class="eyebrow">SAMPLE · 示例数据</span>'
      + '<p class="body">检测到 ' + legacy.length + ' 个从未使用过的示例药品（'
      + esc(legacy.map(function (m) { return m.name; }).join('、'))
      + '）。旧版本首次启动时会自动生成它们，不是你手动添加的。</p>'
      + '<button class="btn btn-ghost" id="btnDropLegacy" style="height:44px;font-size:calc(14px * var(--fs))">删除这 ' + legacy.length + ' 项</button>'
      + '</div>';
  }

  host.innerHTML = html;

  /* ⚠️ 这里**故意不**逐个绑 onclick —— 改由 boot() 里的一次性事件委托处理。
   * 原因见 boot 里那段注释：render() 会把视图 innerHTML 整个重建，
   * 手指落下到 click 之间只要重绘一次，正在点的元素就被换掉 → 点击被吞。 */
}
