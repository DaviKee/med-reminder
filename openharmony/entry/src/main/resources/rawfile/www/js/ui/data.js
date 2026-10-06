/* ui/data.js —— 数据管理：备份 / 导出 / 恢复 / 清理 / 服药报告
 *
 * 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 */
import { $, $$, copyText, esc, fmtDate, minOfDay, minToStr, nowMin } from '../core/util.js';
import { S, cleanPreview, save, storageError, storageStats } from '../core/store.js';
import { adherenceReport, medById, silenceOverdue } from '../core/schedule.js';
import { closeDlg, openDlg, rememberFocus } from './overlay.js';
import { render } from './render.js';
import { toast } from './toast.js';
import { syncNotifications } from './actions.js';

/* ---------------- 备份 / 导出 / 恢复 ----------------
 * 记录默认只存在本机，清缓存或换机即全丢。这里给两条零依赖的出路：
 *   · JSON 备份 —— 可完整恢复（含药品与全部剂量）
 *   · CSV      —— 给人看（就诊时打印或发给医生）
 * 不依赖任何 Capacitor 插件：Android WebView 里 a[download] 常常不生效，
 * 所以「复制到剪贴板」是主路径，「下载文件」是浏览器上的加分项。 */
export var BACKUP_FORMAT = 'medreminder.backup';

export var BACKUP_VERSION = 1;

/* 导入体积上限。localStorage 在 Android WebView 里通常 5 MB（UTF-16 计 2 字节/字符），
 * 一份备份的字符数超过这个门槛就几乎必然塞爆配额 —— 与其导入到一半失败、
 * 把用户现有数据搞成"改了一半"，不如**在改数据之前**就明确拒绝。
 * 取 2 MB 字符：即便按 2 字节/字符也才 4 MB，低于 5 MB 预算并留出余量。
 * 正常用户一个月的记录约 30 KB（见测试用例 F），离上限还有两个数量级。 */
export var BACKUP_MAX_CHARS = 2 * 1024 * 1024;

/* 剂量单日上限：一天最多 2880 个 30 分钟槽位，再往上必然是脏数据或恶意构造 */
export var BACKUP_MAX_DOSES_PER_DAY = 2880;

/* status 只认这三个值 —— 其它值既不算已服也不算待服，会静默污染统计 */
export var DOSE_STATUSES = ['pending', 'taken', 'skipped'];

/* 合法日期键：YYYY-MM-DD，且**必须是真实存在的日期**（含闰年规则）。
 * 用日期回写比对而不是正则，才能拦下 2026-02-30 / 2025-02-29 这种。 */
export function isDateKey(k) {
  if (typeof k !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(k)) return false;
  var y = +k.slice(0, 4), mo = +k.slice(5, 7), d = +k.slice(8, 10);
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
  var dt = new Date(y, mo - 1, d);
  return dt.getFullYear() === y && dt.getMonth() === mo - 1 && dt.getDate() === d;
}

export var dataMode = null;       // backup | csv | restore

export var restoreArmed = false;  // 恢复需点两次，避免误覆盖

export function buildBackup() {
  // notified 是当天的提醒登记簿，属临时状态，不进备份
  return {
    format: BACKUP_FORMAT,
    version: BACKUP_VERSION,
    app: 'MedReminder',
    exportedAt: new Date().toISOString(),
    data: { meds: S.meds, doses: S.doses }
  };
}

/* ⚠️ 剂量侧的校验（H-3）。
 *
 * 这段存在的理由：doses 里的每一处非法值都不会**当场**报错，而是
 * 在被渲染/统计/排程时炸出来，用户看到的是"导入成功了，但记录页白屏"。
 * 逐个堵住后果：
 *   · 值不是数组        → S.doses[k].some/forEach → TypeError（整页崩）
 *   · 日期键不是真日期  → 排序、按月前缀过滤（adherenceStats 按 'YYYY-MM' 取前缀）全错
 *   · time 越界/非数字  → minToStr、排程比较得到荒谬结果
 *   · 缺 id             → 照片回收、去重按 undefined 归组
 *   · status 未知值     → 既不算已服也不算待服，静默丢出统计
 *   · 单日剂量爆炸      → 体积/内存
 * 返回 true 表示「有问题」，与上方 meds 的写法保持一致。 */
export function dosesInvalid(doses) {
  if (Array.isArray(doses)) return true;                  // 整体必须是「日期 → 数组」的对象，不是数组
  var keys = Object.keys(doses), total = 0;
  for (var i = 0; i < keys.length; i++) {
    var k = keys[i];
    if (!isDateKey(k)) return true;
    var arr = doses[k];
    if (!Array.isArray(arr)) return true;                 // 一天的值必须是数组
    if (arr.length > BACKUP_MAX_DOSES_PER_DAY) return true;
    total += arr.length;
    for (var j = 0; j < arr.length; j++) {
      var d = arr[j];
      if (!d || typeof d !== 'object' || Array.isArray(d)) return true;
      if (typeof d.id !== 'string' || !d.id) return true;
      if (typeof d.medId !== 'string' || !d.medId) return true;   // 允许指向已删除的药（保留历史）
      if (typeof d.time !== 'number' || !isFinite(d.time)) return true;
      if (d.time < 0 || d.time > 1439) return true;
      if (DOSE_STATUSES.indexOf(d.status) < 0) return true;
      /* takenAt 允许 null（未服）或数字（时间戳）；其它类型一律可疑 */
      if (d.takenAt != null && (typeof d.takenAt !== 'number' || !isFinite(d.takenAt))) return true;
    }
  }
  return false;
}

/* 导入前严格校验：宁可拒绝，也不要让半截数据覆盖掉用户现有记录 */
export function parseBackup(txt) {
  if (!txt || !String(txt).trim()) return { err: '请先粘贴备份内容' };
  /* 体积先于解析判断 —— JSON.parse 一个巨型字符串本身就可能把内存打满 */
  if (String(txt).length > BACKUP_MAX_CHARS) {
    return { err: '备份太大（超过 2 MB），可能不是本 App 导出的备份' };
  }
  var o;
  try { o = JSON.parse(txt); } catch (e) { return { err: '内容不是合法 JSON，可能复制不完整' }; }
  if (!o || o.format !== BACKUP_FORMAT) return { err: '这不是本 App 导出的备份' };
  if (typeof o.version !== 'number' || o.version > BACKUP_VERSION) return { err: '备份来自更新版本的 App' };
  if (!o.data || !Array.isArray(o.data.meds) || !o.data.doses || typeof o.data.doses !== 'object') {
    return { err: '备份内容不完整' };
  }
  var badMed = !o.data.meds.every(function (m) {
    if (!m || typeof m.id !== 'string' || typeof m.name !== 'string') return false;
    /* interval 必须仍合法：它是固定时刻模式下的「默认间隔」，也用于旧数据 */
    if (typeof m.interval !== 'number' || !isFinite(m.interval) || m.interval <= 0) return false;
    /* mode / times 是可选的（旧备份没有这两项）。有就必须合法 ——
     * 脏数据直接进排程是会死循环或排 0 次的。 */
    if (m.mode != null && m.mode !== 'interval' && m.mode !== 'fixed') return false;
    if (m.mode === 'fixed') {
      if (!Array.isArray(m.times) || !m.times.length) return false;
      if (!m.times.every(function (x) {
        return typeof x === 'number' && isFinite(x) && x >= 0 && x <= 1439;
      })) return false;
    }
    return true;
  });
  if (badMed) return { err: '备份里的药品数据有问题' };
  if (dosesInvalid(o.data.doses)) return { err: '备份里的服药记录有问题' };
  return { ok: o };
}

export function csvCell(v) {
  var s = String(v == null ? '' : v);
  return /[",\r\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
}

export function buildCsv() {
  var rows = [['日期', '计划时刻', '实际服药时刻', '药品', '间隔(小时)', '状态']];
  Object.keys(S.doses).sort().forEach(function (k) {
    (S.doses[k] || []).slice().sort(function (a, b) { return a.time - b.time; }).forEach(function (d) {
      var m = medById(d.medId);
      rows.push([
        k,
        minToStr(d.time),
        (d.status === 'taken' && d.takenAt) ? minToStr(minOfDay(d.takenAt)) : '',
        m ? m.name : '已删除药品',
        m ? m.interval : '',
        d.status === 'taken' ? '已服用' : (d.status === 'skipped' ? '已跳过' : '待服用')
      ]);
    });
  });
  return rows.map(function (r) { return r.map(csvCell).join(','); }).join('\r\n');
}

export function doseRecordCount() {
  var n = 0;
  Object.keys(S.doses).forEach(function (k) { n += (S.doses[k] || []).length; });
  return n;
}

/* `null`（没有可统计的剂量）显示成「—」，**不能显示 0%** ——「没数据」≠「没吃」。 */
export function reportRateText(rate) {
  return (rate == null) ? '—' : rate + '%';
}

export function reportHtml(rep) {
  var tot = rep.total;
  var h = '<div class="rpt-head">'
    + '<div class="rpt-rate">' + reportRateText(tot.rate) + '</div>'
    + '<div class="rpt-rate-cap">依从率</div>'
    + '</div>'
    + '<div class="rpt-meta">' + esc(rep.from) + ' ～ ' + esc(rep.to) + ' · 共 ' + rep.days + ' 天</div>'
    + '<div class="rpt-chips">'
    + '<span class="rpt-chip ok">已服 ' + tot.taken + '</span>'
    + '<span class="rpt-chip bad">漏服 ' + tot.missed + '</span>'
    + (tot.skipped ? '<span class="rpt-chip">主动跳过 ' + tot.skipped + '</span>' : '')
    + '</div>'
    + '<table class="rpt-tb"><tr><th>日期</th><th>已服</th><th>漏服</th><th>跳过</th><th>依从率</th></tr>';
  rep.rows.forEach(function (r) {
    h += '<tr><td>' + esc(r.key.slice(5)) + '</td><td>' + r.taken + '</td>'
      + '<td>' + (r.missed ? '<b class="bad">' + r.missed + '</b>' : '0') + '</td>'
      + '<td>' + r.skipped + '</td><td>' + reportRateText(r.rate) + '</td></tr>';
  });
  return h + '</table>';
}

export function reportPlain(rep) {
  var tot = rep.total;
  var L = ['服药情况报告',
           '统计范围：' + rep.from + ' ~ ' + rep.to + '（共 ' + rep.days + ' 天）',
           '生成时间：' + fmtDate(new Date()) + ' ' + minToStr(nowMin()),
           '',
           '总计：已服 ' + tot.taken + ' 次 · 漏服 ' + tot.missed + ' 次'
             + (tot.skipped ? ' · 主动跳过 ' + tot.skipped + ' 次' : '')
             + ' · 依从率 ' + reportRateText(tot.rate),
           '',
           '逐日：'];
  rep.rows.forEach(function (r) {
    L.push(r.key + '  已服 ' + r.taken + '  漏服 ' + r.missed
      + (r.skipped ? '  跳过 ' + r.skipped : '') + '  依从率 ' + reportRateText(r.rate));
  });
  L.push('');
  L.push('（数据来自本机打卡记录，由「药准时」生成）');
  return L.join('\n');
}

export var reportText = '';   // 当前报告的可复制文本

export function openReportDlg() {
  var rep = adherenceReport(7);
  reportText = reportPlain(rep);
  /* 一条都没统计到 vs 数据不全（比如今天刚装的 App）要分开说 —— 别让医生以为"全没吃"。 */
  $('#rptHint').textContent = (rep.total.taken + rep.total.missed === 0)
    ? '最近 7 天还没有服药记录。有记录后，这里会显示每天按时吃药的情况。'
    : '近 7 天的服药情况，可直接拿给医生看，或点「复制文字」发给家人。';
  $('#rptArea').innerHTML = reportHtml(rep);
  rememberFocus();
  openDlg($('#dlgReport'));
}

export function downloadText(filename, text, mime) {
  try {
    var blob = new Blob([text], { type: mime + ';charset=utf-8' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    setTimeout(function () {
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }, 1500);
    return true;
  } catch (e) { return false; }
}

export function openDataDlg(mode) {
  dataMode = mode;
  restoreArmed = false;
  var ta = $('#dataArea');
  var isRestore = mode === 'restore';
  ta.value = '';
  ta.readOnly = !isRestore;

  if (mode === 'backup') {
    $('#dataEyebrow').textContent = 'BACKUP';
    $('#dataTitle').textContent = '导出备份';
    $('#dataHint').textContent = '复制下面全部内容，存进备忘录或网盘。换手机、清缓存后可用它完整恢复。';
    ta.value = JSON.stringify(buildBackup(), null, 2);
  } else if (mode === 'csv') {
    $('#dataEyebrow').textContent = 'CSV';
    $('#dataTitle').textContent = '导出服药记录';
    $('#dataHint').textContent = doseRecordCount()
      ? '下面是全部服药记录（计划时刻 / 实际服药时刻 / 状态），复制后粘进 Excel，或直接发给医生。'
      : '还没有任何服药记录，导出内容只有表头。';
    ta.value = buildCsv();
  } else {
    $('#dataEyebrow').textContent = 'RESTORE';
    $('#dataTitle').textContent = '从备份恢复';
    $('#dataHint').textContent = '把备份内容粘贴到下面（或点「从文件选择」），再点「恢复」。这会覆盖当前全部药品与记录。';
  }

  $('#dataCopy').classList.toggle('hidden', isRestore);
  $('#dataDownload').classList.toggle('hidden', isRestore || !ta.value);
  $('#dataPick').classList.toggle('hidden', !isRestore);
  $('#dataApply').classList.toggle('hidden', !isRestore);
  var ap = $('#dataApply');
  ap.textContent = '恢复';
  ap.classList.remove('btn-accent');
  ap.classList.add('btn-primary');
  $('#dataDownload').textContent = mode === 'csv' ? '下载 .csv 文件' : '下载 .json 文件';
  openDlg($('#dlgData'));
}

export function applyRestore() {
  var r = parseBackup($('#dataArea').value);
  if (r.err) { toast(r.err); return; }
  var d = r.ok.data;
  var nMed = d.meds.length;
  var nDay = Object.keys(d.doses).length;
  if (!restoreArmed) {           // 第一次点：只做提示，不动数据
    restoreArmed = true;
    var b = $('#dataApply');
    b.textContent = '再点一次 · 覆盖当前数据';
    b.classList.remove('btn-primary');
    b.classList.add('btn-accent');
    toast('将覆盖现有内容（' + nMed + ' 个药品 · ' + nDay + ' 天记录）');
    return;
  }
  restoreArmed = false;
  S.meds = d.meds;
  S.doses = d.doses;
  S.notified = {};      // 提醒登记簿重建，避免旧标记把今天的提醒压掉
  silenceOverdue();     // 与启动逻辑一致：过期太久的不补响
  save();               // 内含 syncNotifications()
  closeDlg($('#dlgData'));
  render();
  syncNotifications();
  toast('已恢复 ' + nMed + ' 个药品 · ' + nDay + ' 天记录');
}

/* 自动备份的状态卡 */
export function autoBackupCardHtml() {
  var A = window.MedAutoBackup;
  if (!A) return '';
  var s = A.status();
  var line;
  if (!s.supported) line = '这台设备不支持（浏览器模式没有文件系统）';
  else if (!s.at) line = '还没备份过 —— 改动一次数据就会自动存';
  else if (!s.ok) line = '上次没成功：' + (s.err || '未知原因');
  else line = '最近一次：' + A.stamp(s.at) + (s.count == null ? '' : ' · 保留 ' + s.count + ' 份');

  return '<div class="card" style="display:flex;flex-direction:column;gap:8px">'
    + '<span class="eyebrow">AUTO · 自动备份</span>'
    + '<p class="body">每次改动都会自动往手机里存一份完整数据，随时能找回来。</p>'
    + '<p class="hint">' + esc(line) + '</p>'
    /* 目录是长 token —— `/` 与 `.` 都不是断行点，默认整串不拆，会顶出卡片右边
     * （2026-09-28 真机反馈）。手工在每个 `/` 后插一个 <wbr>（零宽断行机会），
     * 断行就落在目录分隔处，比让浏览器在任意字符处硬断好看得多。
     * 先 esc 再插 <wbr>：`/` 不在 esc 的转义表里，先后无所谓，但先 esc 更稳妥。 */
    + '<p class="hint">位置：'
    + esc('Android/data/com.medreminder.app/files/' + A.DIR).replace(/\//g, '/<wbr>')
    + '（用电脑 USB 能看到）</p>'
    + '<p class="hint">⚠️ 卸载 App 会连这个目录一起删掉 —— <b>换手机或重装之前，'
    + '请先用上面「导出备份」另存一份到别处</b>。</p>'
    + '<button class="btn btn-ghost" id="btnAutoBak" style="height:44px;font-size:calc(14px * var(--fs))">立即备份一次</button>'
    + '</div>';
}

/* 存储状态卡：让「什么时候会满」提前可见，而不是等它满了才知道 */
export function storageCardHtml() {
  var st = storageStats();
  var level, note;
  if (storageError)      { level = 'err';  note = '写入失败，新数据没能存进手机。请先导出备份，再清理旧记录。'; }
  else if (st.pct >= 85) { level = 'err';  note = '已接近上限，随时可能写不进去。请立即导出备份并清理旧记录。'; }
  else if (st.pct >= 70) { level = 'warn'; note = '占用偏高。建议导出备份后清理旧记录。'; }
  else                   { level = 'ok';   note = '记录只存在这台手机上，清缓存或换手机会丢，建议定期导出备份。'; }

  var color = level === 'err' ? '#FF3B30' : (level === 'warn' ? '#FF7A17' : '#3ECF8E');
  var barW = st.pct > 0 ? Math.max(2, st.pct) : 0;   // 有数据至少画一小段，0% 的空条看起来像坏了

  return '<div class="card" style="display:flex;flex-direction:column;gap:10px">'
    + '<div style="display:flex;align-items:baseline;justify-content:space-between;gap:12px">'
    + '<span class="eyebrow">STORAGE · 本地存储</span>'
    + '<span class="meta">' + st.days + ' 天 · ' + st.doses + ' 条</span></div>'
    + '<div class="bar" role="img" aria-label="已用 ' + st.pct + '%"><i style="width:' + barW + '%;background:' + color + '"></i></div>'
    + '<p class="body" style="margin:0">已用约 ' + st.kb + ' KB'
    + (st.photoFiles
        ? ('（记录 ' + Math.round((st.bytes - st.photoBytes) / 1024) + ' KB + 照片 ' + st.photoFiles + ' 张 ' + Math.round(st.photoBytes / 1024) + ' KB）')
        : '')
    + '，上限 5 MB（' + st.pct + '%）。' + esc(note) + '</p>'
    + '<button class="btn btn-ghost" id="btnClean" style="height:44px;font-size:calc(14px * var(--fs));align-self:flex-start">清理旧记录</button>'
    + '</div>';
}

/* 清理对话框。默认停在「不清理」，且**必须用户主动点确认** —— 破坏性操作不设默认值。 */
export var CLEAN_LEVELS = [
  { label: '90 天', keep: 90 },
  { label: '180 天', keep: 180 },
  { label: '1 年', keep: 365 },
  { label: '不清理', keep: 0 }
];

export var cleanKeep = 0;

export function renderCleanRow() {
  var row = $('#cleanRow');
  if (!row) return;
  row.innerHTML = CLEAN_LEVELS.map(function (lv) {
    var on = lv.keep === cleanKeep;
    return '<button class="chip fs-chip' + (on ? ' on' : '') + '" data-clean="' + lv.keep + '"'
      + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + lv.label + '</button>';
  }).join('');
  $$('[data-clean]').forEach(function (el) {
    el.onclick = function () {
      cleanKeep = parseInt(el.getAttribute('data-clean'), 10) || 0;
      renderCleanRow();
      renderCleanPreview();
      var btn = $('#cleanConfirm');
      if (btn) btn.disabled = cleanKeep === 0;   // 「不清理」= 什么都不做，确认键就该是灰的
    };
  });
}

export function renderCleanPreview() {
  var pv = $('#cleanPreview');
  if (!pv) return;
  if (!cleanKeep) { pv.textContent = '不清理 · 保留全部记录'; return; }
  var r = cleanPreview(cleanKeep);
  pv.textContent = r.days
    ? ('保留最近 ' + cleanKeep + ' 天 · 删除 ' + r.days + ' 天 / ' + r.doses + ' 条')
    : ('保留最近 ' + cleanKeep + ' 天 · 没有可删的记录');
}

export function openCleanDlg() {
  cleanKeep = 0;          // 每次都从「不清理」开始，避免误触上一次的档位
  renderCleanRow();
  renderCleanPreview();
  var btn = $('#cleanConfirm');
  btn.disabled = true;    // 没选范围就不让点，避免「点了没反应」
  openDlg($('#dlgClean'));
}

/* 数据浮层 / 报告浮层 / 恢复的按钮绑定 —— 与 `dataMode` / `restoreArmed` /
 * `reportText` 同住（写点全在这段里，拆开就得配 setter）。
 * 函数体一字未改，只去掉一层缩进。 */
export function bindDataDlg() {
  /* ---- 备份 / 导出 / 恢复 ---- */
  $('#dataClose').onclick = function () { restoreArmed = false; closeDlg($('#dlgData')); };
  $('#histClose').onclick = function () { closeDlg($('#dlgHistory')); };
  /* S-7 报告：「复制」读的是内存里的 reportText，不是 DOM ——
   * 报告正文是给眼睛看的 HTML，直接抄 DOM 会把表格拍成一团乱码文本。 */
  $('#rptClose').onclick = function () { closeDlg($('#dlgReport')); };
  $('#rptCopy').onclick = function () {
    if (!reportText) { toast('没有可复制的内容'); return; }
    copyText(reportText).then(function (ok) {
      toast(ok ? '已复制到剪贴板' : '复制失败，请长按选择后手动复制');
    });
  };
  $('#dataCopy').onclick = function () {
    var txt = $('#dataArea').value;
    if (!txt) { toast('没有可复制的内容'); return; }
    copyText(txt).then(function (ok) {
      toast(ok ? '已复制到剪贴板' : '复制失败，请长按文本框全选后手动复制');
    });
  };
  $('#dataDownload').onclick = function () {
    var isCsv = dataMode === 'csv';
    var txt = $('#dataArea').value;
    if (!txt) { toast('没有可导出的内容'); return; }
    // CSV 前置 BOM，Excel 才会按 UTF-8 解析中文
    var ok = downloadText(
      'medreminder-' + (isCsv ? 'records' : 'backup') + '-' + fmtDate(new Date()) + (isCsv ? '.csv' : '.json'),
      (isCsv ? '\ufeff' : '') + txt,
      isCsv ? 'text/csv' : 'application/json'
    );
    toast(ok ? '已开始下载；若手机没有下载目录，改用「复制」' : '当前环境不支持下载，请改用「复制」');
  };
  $('#dataPick').onclick = function () { $('#pickBackup').click(); };
  $('#pickBackup').onchange = function (e) {
    var f = e.target.files && e.target.files[0];
    if (!f) return;
    var fr = new FileReader();
    fr.onload = function () { $('#dataArea').value = String(fr.result || ''); toast('已读入文件，确认内容后点「恢复」'); };
    fr.readAsText(f);
    e.target.value = '';   // 允许重复选择同一个文件
  };
  $('#dataApply').onclick = applyRestore;
}

/* 对外唯一的「写入口」：解除恢复的二次确认武装。
 *
 * 为什么需要：通用关浮层（closeTopDialog）会在关掉任何浮层时清这个标记
 * —— 那是 app.js 的逻辑，不该反向 import 本模块的状态。
 * ⚠️ 只读的需求走 `export var` 活绑定；**写一律走这里**。 */
export function disarmRestore() {
  restoreArmed = false;
}
