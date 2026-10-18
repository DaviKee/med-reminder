/* ui/meds.js —— 药品视图：列表渲染 + 遗留示例药的处理
 *
 * 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 */
import { $, esc, dayDiff, todayKey } from '../core/util.js';
import { S, save } from '../core/store.js';
import { medClass, normSched, todayDoseCount } from '../core/schedule.js';
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

/* 短期药的剩余天数文案（今天 = 最后一天时有特殊文案） */
function shortLeft(m) {
  var s = normSched(m);
  if (!s.to) return '';
  var diff = dayDiff(todayKey(), s.to);       // 今天之后还有几天
  if (diff <= 0) return '今天最后一天';
  return '还剩 ' + diff + ' 天';
}

/* ---------------- 剂型图标（2026-10-18） ----------------
 * 为什么药品行要有个 44×44 的图形位：老人**靠外观认药**，不靠药名 ——
 * 「维生素 D3」和「维生素 D3 滴剂」在纯文字列表里几乎同形。
 *
 * 现状：med 里没有药品照片（拍照打卡存的是每次服药的记录，不是药品档案），
 * 所以先按 med.form（剂型）给一个兜底图标。将来加了"给药品拍张照"，
 * 直接把 <img> 塞进 .medthumb 即可，尺寸/圆角都不用动（见 views.css 注释）。
 *
 * ⚠️ 图标一律 currentColor —— 颜色由 .medthumb 的 color 决定，
 *    写死 #fff 会在亮色主题下直接看不见（theme.spec 有断言盯着）。 */
var FORM_ICON = {
  pill: '<svg width="22" height="22" viewBox="0 0 24 24" fill="none"><rect x="3" y="9" width="18" height="6" rx="3" transform="rotate(-45 12 12)" stroke="currentColor" stroke-width="1.8"/><path d="M9 9l6 6" stroke="currentColor" stroke-width="1.8"/></svg>',
  tablet: '<svg width="22" height="22" viewBox="0 0 24 24" fill="none"><rect x="5" y="5" width="14" height="14" rx="4.5" stroke="currentColor" stroke-width="1.8"/><path d="M12 8.4v7.2" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></svg>',
  bottle: '<svg width="22" height="22" viewBox="0 0 24 24" fill="none"><path d="M9.5 3.2h5M10 3.2v3.1L7.7 9.2A3 3 0 0 0 6.7 11.3v6.5a2 2 0 0 0 2 2h6.6a2 2 0 0 0 2-2v-6.5a3 3 0 0 0-1-2.1L14 6.3V3.2" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  patch: '<svg width="22" height="22" viewBox="0 0 24 24" fill="none"><rect x="3.6" y="6.4" width="16.8" height="11.2" rx="3.2" stroke="currentColor" stroke-width="1.8"/><circle cx="12" cy="12" r="2.6" stroke="currentColor" stroke-width="1.6"/></svg>'
};

function formIcon(m) {
  var f = (m && m.form) || '';
  if (!f) return FORM_ICON.pill;
  if (f === '胶囊') return FORM_ICON.pill;
  if (f === '颗粒' || f === '口服液' || f === '滴剂' || f === '喷剂') return FORM_ICON.bottle;
  if (f === '贴剂' || f === '软膏') return FORM_ICON.patch;
  return FORM_ICON.tablet;
}
export function renderMeds() {
  var host = $('#medsView');
  var html = ''
    + '<div class="card-row" style="padding:0 0 4px">'
    + '<h1 class="h1">我的药品</h1>'
    + '<span class="meta" style="flex:0 0 auto">' + (S.meds.length ? S.meds.length + ' 种' : '') + '</span>'
    + '</div>';
  /* ⚠️ 「添加」按钮**不在页头**了（2026-10-18）：挪到了 index.html 里的右下角 FAB
     （#btnAdd）。原因：六英寸屏单手够不到右上角，而这颗按钮是药品页唯一的主动作。
     点击仍走 app.js 的 document 级事件委托（`t.closest('#btnAdd')`），所以这里
     不需要绑任何东西 —— 委托不依赖本视图的重绘。 */

  if (!S.meds.length) {
    html += '<div class="card"><p class="body" style="text-align:center">还没有药品。点右上角 + 添加第一个。</p></div>';
  } else {
    /* ★ 长期/短期分组（2026-10-13，向导 M3）：medClass 派生（不加数据字段）。
     * 长期在上（一般不动）、短期居中（带剩余天数角标）、已结束垫底（不再提醒）。 */
    var groups = { long: [], short: [], ended: [] };
    S.meds.forEach(function (m) { groups[medClass(m)].push(m); });

    function medItem(m, badge) {
      var meta = medScheduleLabel(m) + ' · ' + (todayDoseCount(m.id) ? ('今日 ' + todayDoseCount(m.id) + ' 次') : '未排程');
      if (badge) meta += ' · ' + badge;
      return '<button class="meditem" data-med="' + m.id + '">'
        + '<span class="medthumb">' + formIcon(m) + '</span>'
        + '<span class="medinfo"><span class="n">' + esc(m.name) + '</span>'
        + '<span class="m">' + esc(meta) + '</span></span>'
        + ICON.chev + '</button>';
    }
    function section(title, arr, badgeFn) {
      if (!arr.length) return '';
      return '<div class="sect" style="margin-bottom:14px">'
        + '<span class="eyebrow">' + title + '</span>'
        + '<div class="list">'
        + arr.map(function (m) { return medItem(m, badgeFn ? badgeFn(m) : ''); }).join('')
        + '</div></div>';
    }

    html += section('LONG-TERM · 长期服药', groups.long, null);
    html += section('SHORT-TERM · 短期用药（到期自动停）', groups.short, shortLeft);
    html += section('ENDED · 已结束（不再提醒）', groups.ended, null);
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
