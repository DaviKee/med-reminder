/* ui/sheet.js —— 药品编辑浮层：打开 / 渲染 / 校验 / 绑定
 *
 * 架构重构：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 */
import { $, $$, dayDiff, minToStr, uid, todayKey } from '../core/util.js';
import { S, save } from '../core/store.js';
import { MAX_TIMES, ensureFixedDoses, medById, medClass, medMode, needRebuildDoses, normTimes,
         rebuildTodayDoses, schedLabel, normSched, timeInputToMin, todayDoses } from '../core/schedule.js';
import { askConfirm, enterOverlay, leaveOverlay, rememberFocus, restoreFocus, setScrim, sheetSaveBegin, sheetSaveReset } from './overlay.js';
import { render } from './render.js';
import { toast } from './toast.js';
import { deleteMed, endMedEarly } from './actions.js';
import { guideNotifyOnce } from './permission.js';
import { setOrDel } from './cards.js';

export function openSheet(medId) {
  /* 已经开着就直接忽略。连点「+」时后续的点击没有意义，只会白跑一轮
   * renderPreview + 抢焦点，把 App 拖卡（2026-09-24 反馈：点不动就狂点）。
   * 浮层是模态的，盖着遮罩也点不到别处，所以不存在"想换成另一个药"的场景。 */
  if ($('#sheetMed').classList.contains('show')) return;
  sheetSaveReset();                 // 每次打开浮层都是一个新会话
  editingId = medId;
  var med = medId ? medById(medId) : null;
  $('#sheetTitle').textContent = med ? '编辑药品' : '添加药品';
  $('#medName').value = med ? med.name : '';
  /* ★ S-1：三个新字段也要预填 —— 否则「编辑」一次就会把原有剂量 / 备注清空。 */
  $('#medDose').value = (med && med.dose != null) ? med.dose : '';
  $('#medUnit').value = (med && med.unit != null) ? med.unit : '';
  $('#medNote').value = (med && med.note != null) ? med.note : '';
  /* ★ S-2：剩余量也要预填 —— 否则「编辑」一次就把库存清零了。 */
  $('#medStock').value = (med && med.stock != null) ? med.stock : '';
  stepVal = med ? med.interval : 8;
  /* 编辑态的临时值：改完点「保存」才写进数据，中途关掉不影响原有设置 */
  medModeDraft = med ? medMode(med) : 'interval';
  fixedTimes = med ? normTimes(med.times) : [];
  /* ★ S-3：周期也要预填 —— 否则「编辑」一次就把周期重置成每天。
   *   旧数据没有 sched → normSched 返回 daily ✓ */
  var sc0 = normSched(med);
  schedModeDraft = sc0.mode;
  weekDraft = sc0.weekdays.slice();
  cycleDraft = sc0.everyDays >= 2 ? sc0.everyDays : 2;
  /* ★ S-4：疗程起止也要预填（空字符串 = 未填，与 input[type=date] 一致） */
  if ($('#courseFrom')) $('#courseFrom').value = sc0.from || '';
  if ($('#courseTo')) $('#courseTo').value = sc0.to || '';
  renderCourseHint();
  /* ★ 录入向导：步数归 1、剂型/长短期草稿预填（编辑一次不能把原有值冲掉） */
  wzStep = 1;
  formDraft = (med && med.form) || '';
  durDraft = (sc0.from || sc0.to) ? 'short' : 'long';
  renderWizardStatic();
  $('#deleteMed').classList.toggle('hidden', !med);
  renderPreview();
  renderWizard();
  /* 浮层打开时把焦点带进去（键盘用户不用满屏找输入框）。
   * 焦点与计数住在 ui/overlay.js —— 外部只能经这两个函数写。 */
  rememberFocus();
  $('#sheetMed').classList.add('show');
  setScrim(true); enterOverlay();
  var nf = $('#medName');
  if (nf) { try { nf.focus(); } catch (e) { /* ignore */ } }
}

export function closeSheet() {
  $('#sheetMed').classList.remove('show'); setScrim(false); leaveOverlay();
  restoreFocus();
}

/* ---------------- sheet: stepper + save ---------------- */
export var stepVal = 8;

export var editingId = null;

/* 药品编辑浮层里的临时状态：模式与时刻表。点「保存」才写进数据。 */
export var medModeDraft = 'interval';

/* ★ S-3：周期草稿。同样只在点「保存」时才写进数据 —— 中途关掉不影响原有设置。 */
export var schedModeDraft = 'daily';
export var weekDraft = [];        // [0..6]，0 = 周一（与 dowOf 同一约定）
export var cycleDraft = 2;

export var fixedTimes = [];

/* ---------------- 录入向导（2026-10-10，见 docs/MedReminder-药品录入向导与长期短期分类-设计.md） ----------------
 *
 * 设计铁律：**只重组交互，不动数据**。
 *   · 每个 .field 用 data-wz 标了步号（index.html），renderWizard 按步显隐 ——
 *     所有 id / 保存 / 校验 / 重排逻辑一概不动，旧数据天然兼容；
 *   · 长期/短期**不加数据字段**：med.end 为空 = 长期，有值 = 短期疗程（S-4 现有能力），
 *     durDraft 只是编辑期的界面草稿；
 *   · 唯一新增的数据字段是 med.form（剂型，可空）—— 与 S-1 三字段同一套 setOrDel 纪律。
 */

/* 剂型清单（国内常见口服+外用剂型；「其他」不落库）。单位与剂型联动 ——
 * 用户不该面对与自己的剂型无关的单位选项（Medisafe 同款设计，见设计文档 §1.1）。 */
var FORMS = ['片剂', '胶囊', '颗粒', '口服液', '丸剂', '含片', '缓释片', '泡腾片', '贴剂', '喷剂', '滴剂', '软膏', '其他'];
var UNIT_BY_FORM = {
  '片剂': '片', '缓释片': '片', '泡腾片': '片', '含片': '片',
  '胶囊': '粒', '颗粒': '袋', '口服液': '支', '丸剂': '丸',
  '滴剂': '滴', '贴剂': '贴', '喷剂': '喷', '软膏': '次'
};

/* 快捷说明 —— 点一下填进备注框，免得老人手打「饭后服用」。 */
var NOTE_PRESETS = ['餐前', '随餐', '餐后', '睡前', '空腹'];

var STEP_TITLES = ['', '这个药是什么', '怎么吃', '什么时候提醒', '哪些天吃', '吃多久'];

export var wzStep = 1;            // 当前向导步（1..5）
export var formDraft = '';        // 剂型草稿；'' = 未选；'其他' 不落库
export var durDraft = 'long';     // 长期/短期的界面草稿（保存的只有疗程起止日期）

/* 剂型 chips（#formRow 每次打开重建 —— 选中态跟 formDraft 走） */
function renderForm() {
  var row = $('#formRow');
  if (!row) return;
  row.innerHTML = FORMS.map(function (f) {
    var on = f === formDraft;
    return '<button type="button" class="chip fs-chip' + (on ? ' on' : '') + '" data-form="' + f + '"'
      + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + f + '</button>';
  }).join('');
}

/* 快捷说明 chips（#noteRow） */
function renderNoteRow() {
  var row = $('#noteRow');
  if (!row) return;
  row.innerHTML = NOTE_PRESETS.map(function (n) {
    var on = ($('#medNote').value || '').trim() === n;
    return '<button type="button" class="chip fs-chip' + (on ? ' on' : '') + '" data-note="' + n + '"'
      + ' aria-pressed="' + (on ? 'true' : 'false') + '">' + n + '</button>';
  }).join('');
}

/* 长期/短期 chips + 快捷天数显隐 + 「提前结束」按钮显隐（仅编辑短期药时出现） */
function renderDur() {
  $$('#durRow [data-dur]').forEach(function (el) {
    var on = el.getAttribute('data-dur') === durDraft;
    el.classList.toggle('on', on);
    el.setAttribute('aria-pressed', on ? 'true' : 'false');
  });
  var q = $('#durQuick');
  if (q) q.classList.toggle('hidden', durDraft !== 'short');
  /* 提前结束只对「编辑中且仍是短期」的药出现 —— 长期药没有"结束"一说 */
  var we = $('#wzEnd');
  if (we) {
    var m = editingId ? medById(editingId) : null;
    we.classList.toggle('hidden', !(m && medClass(m) === 'short'));
  }
}

/* 向导主渲染：按步显隐 + 进度 + 按钮 + 进入某步时的即时刷新。
 * ⚠️ 每次都全量执行「该步的渲染函数」—— 选中态/预览即最新，与 openSheet 的预填纪律一致。 */
export function renderWizard() {
  $$('#sheetMed [data-wz]').forEach(function (el) {
    el.classList.toggle('hidden', parseInt(el.getAttribute('data-wz'), 10) !== wzStep);
  });
  var prog = $('#wzProg');
  if (prog) prog.textContent = '第 ' + wzStep + ' / 5 步 · ' + STEP_TITLES[wzStep];
  var back = $('#wzBack');
  if (back) back.classList.toggle('hidden', wzStep === 1);
  var next = $('#wzNext');
  if (next) next.classList.toggle('hidden', wzStep === 5);
  var save = $('#saveMed');
  if (save) save.classList.toggle('hidden', wzStep !== 5);
  if (wzStep === 2) renderNoteRow();
  if (wzStep === 3) renderPreview();
  if (wzStep === 4) renderSched();
  if (wzStep === 5) renderDur(); renderCourseHint();
}

/* 每次打开浮层都重建静态 chips（选中态跟着草稿走） */
function renderWizardStatic() {
  renderForm();
  renderNoteRow();
  renderDur();
}

export function renderPreview() {
  /* 先按当前模式切换字段可见性（模式是编辑期的临时状态，不碰数据）。
   * ⚠️ 录入向导之后：这三个字段住在第 3 步 —— 不在第 3 步时必须**保持隐藏**，
   * 否则 openSheet 里的 renderPreview 会把第 3 步的字段漏到第 1 步来。 */
  var onStep = (wzStep === 3);
  var isFixed = medModeDraft === 'fixed';
  var fi = $('#fieldInterval'), ff = $('#fieldFixed'), fp = $('#fieldPreview');
  if (fi) fi.classList.toggle('hidden', !onStep || isFixed);
  if (ff) ff.classList.toggle('hidden', !onStep || !isFixed);
  if (fp) fp.classList.toggle('hidden', !onStep || isFixed);   // 预览只对间隔模式有意义
  $$('#modeRow [data-mode]').forEach(function (el) {
    var on = el.getAttribute('data-mode') === medModeDraft;
    el.classList.toggle('on', on);
    el.setAttribute('aria-pressed', on ? 'true' : 'false');
  });
  /* S-3：周期与「服药方式」是**正交**的两件事（隔天吃也可以固定时刻），
     所以必须在 isFixed 早退**之前**渲染。 */
  renderSched();
  if (isFixed) { renderTimes(); renderFixedHint(); return; }

  var box = $('#previewChips');
  var step = stepVal * 60;                 // 分钟
  var arr = [];
  for (var t = 360, i = 0; t < 1440 && i < 8; t += step, i++) arr.push(minToStr(t));
  box.innerHTML = arr.map(function (t) { return '<span class="chip">' + t + '</span>'; }).join('');

  /* 值 < 1 小时时数值与单位一起换（显示「30 分钟 / 次」，而不是「0.5 小时 / 次」） */
  var under = stepVal < 1;
  $('#stepNum').textContent = under ? String(Math.round(stepVal * 60)) : String(stepVal);
  var unit = $('#sheetMed .step-unit');
  if (unit) unit.textContent = under ? '分钟 / 次' : '小时 / 次';

  /* 「每天约 N 次」——间隔细化到 30 分钟后，光看数字不容易判断一天吃几次。
   * 以 06:00 起算，与上面预览的口径一致。 */
  var hint = $('#stepHint');
  if (hint) {
    var per = Math.floor((1440 - 360) / step) + 1;
    hint.textContent = '按 06:00 起算，一天约 ' + per + ' 次。'
      + (per > 8 ? '次数较多，记得确认是否与医嘱一致。' : '');
  }
}

/* F-1：步进细化为 30 分钟（0.5 小时），并吸附到 0.5 的整数倍 ——
 * 否则连续的 0.5 累加会漂成 0.30000000000000004 这类值写进数据。 */
/* 固定时刻的编辑列表。行是动态生成的，事件靠**委托**绑在 #timeList 上
 * （它本身不被替换，所以只绑一次就够）。 */
export function renderTimes() {
  var box = $('#timeList');
  if (!box) return;
  if (!fixedTimes.length) {
    box.innerHTML = '<p class="hint" style="margin:0">还没有时刻。点下面添加一个 —— 至少要有一个才会提醒。</p>';
    return;
  }
  box.innerHTML = fixedTimes.map(function (t, i) {
    return '<div class="time-row">'
      + '<input type="time" class="time-input" data-ti="' + i + '" value="' + minToStr(t) + '"'
      + ' aria-label="第 ' + (i + 1) + ' 个服药时刻">'
      + '<button class="icon-btn" data-tdel="' + i + '" aria-label="删掉第 ' + (i + 1) + ' 个时刻">'
      + '<svg width="16" height="16" viewBox="0 0 24 24" fill="none"><path d="M6 6l12 12M18 6 6 18" style="stroke:var(--text)" stroke-width="1.8" stroke-linecap="round"/></svg>'
      + '</button></div>';
  }).join('');
}

export function renderFixedHint() {
  var hint = $('#fixedHint');
  if (!hint) return;
  var ts = normTimes(fixedTimes);
  if (!ts.length) { hint.textContent = '至少要设一个时刻，否则它不会提醒。'; return; }
  hint.textContent = '每天 ' + ts.length + ' 次：' + ts.map(minToStr).join('、') + '。'
    + (ts.length > 8 ? '次数较多，确认一下是否与医嘱一致。' : '');
}

/* ⚠️ 从**草稿**构造一个临时 med，只为喂给 schedLabel 生成描述文字 ——
 * 不写进数据，也不碰真实 med。 */
function draftMed() {
  if (schedModeDraft === 'weekly') return { sched: { mode: 'weekly', weekdays: weekDraft } };
  if (schedModeDraft === 'cycle') return { sched: { mode: 'cycle', everyDays: cycleDraft } };
  return {};
}

/* S-3 周期区块：切档位、刷 chips、更新描述。 */
export function renderSched() {
  $$('#schedRow [data-sched]').forEach(function (el) {
    var on = el.getAttribute('data-sched') === schedModeDraft;
    el.classList.toggle('on', on);
    el.setAttribute('aria-pressed', on ? 'true' : 'false');
  });
  var wk = $('#weekRow'), cy = $('#cycleRow'), num = $('#cycNum'), hint = $('#schedHint');
  if (wk) wk.classList.toggle('hidden', schedModeDraft !== 'weekly');
  if (cy) cy.classList.toggle('hidden', schedModeDraft !== 'cycle');
  if (num) num.textContent = String(cycleDraft);
  $$('#weekRow [data-week]').forEach(function (el) {
    var on = weekDraft.indexOf(parseInt(el.getAttribute('data-week'), 10)) >= 0;
    el.classList.toggle('on', on);
    el.setAttribute('aria-pressed', on ? 'true' : 'false');
  });
  if (hint) {
    /* 选了「每周」却一天没选 = 一次都不吃 —— 必须**当面警告**，
     * 否则用户以为设置好了，实际再也不会提醒。 */
    hint.textContent = (schedModeDraft === 'weekly' && !weekDraft.length)
      ? '一天都没选 —— 这样它不会提醒，至少选一天。'
      : schedLabel(draftMed());
  }
}

/* 草稿 → 要写进数据的 sched 对象。返回 null 表示「全默认」（**不写字段**）。
 *
 * ⚠️ S-4 之后「返 null」的条件变严了：**只有「每天 + 无疗程」才能不写字段**。
 *    只设了疗程（每天吃、但只吃 7 天）时 sched 不能是 null，否则疗程丢了。 */
function schedFromDraft(prevMed) {
  var from = $('#courseFrom') ? $('#courseFrom').value : '';
  var to = $('#courseTo') ? $('#courseTo').value : '';
  var out;
  if (schedModeDraft === 'weekly') out = { mode: 'weekly', weekdays: weekDraft.slice() };
  else if (schedModeDraft === 'cycle') {
    /* ⚠️ anchor 必须**沿用原值**：否则改一次剂量就把周期相位重置成"从今天重新数"，
     * 用户刚调好的「隔 2 天」会错位。新建 / 从别的档位切过来时才用今天。 */
    var old = normSched(prevMed);
    out = { mode: 'cycle', everyDays: cycleDraft, anchor: old.anchor || todayKey() };
  } else {
    out = { mode: 'daily' };
  }
  /* from / to 空串 = 未填 → **不写这个字段**（而不是写 null），备份更干净 */
  if (from) out.from = from;
  if (to) out.to = to;
  if (out.mode === 'daily' && !out.from && !out.to) return null;
  return out;
}

/* S-4 疗程的即时反馈：输入后就能看到共多少天，不用等保存。 */
export function renderCourseHint() {
  var h = $('#courseHint');
  if (!h) return;
  var f = $('#courseFrom') ? $('#courseFrom').value : '';
  var t = $('#courseTo') ? $('#courseTo').value : '';
  if (f && t && f > t) { h.textContent = '开始日期不能晚于结束日期。'; return; }
  if (!f && !t) { h.textContent = '不填 = 长期服用。填了结束日，当天之后自动停提醒。'; return; }
  /* 'YYYY-MM-DD' 的字典序就是日期序 —— 比大小不用转日期。 */
  var n = (f && t) ? (dayDiff(f, t) + 1) : null;
  h.textContent = (f ? f + ' 开始' : '从今天起')
    + (t ? '，到 ' + t + ' 结束' : '，长期服用')
    + (n != null ? '（共 ' + n + ' 天）' : '');
}

export function clampStep() {
  if (!isFinite(stepVal)) stepVal = 8;
  stepVal = Math.round(stepVal * 2) / 2;
  if (stepVal < 0.5) stepVal = 0.5;
  if (stepVal > 24) stepVal = 24;
}

/* 药品编辑浮层的全部事件绑定（A-2 · 从 app.js 的 boot 里切出来）。
 *
 * ⚠️ 切出来是为了让上面那 4 个状态**没有外部写点** ——
 *    `stepVal` / `medModeDraft` / `fixedTimes` 的写点全在这个函数里。
 *    这就是「状态与绑定必须同住」：硬拆开就得给每个状态配 setter，反而更绕。
 *    函数体一字未改，只去掉一层缩进。 */
export function bindSheet() {
  $('#scrim').onclick = function () { closeSheet(); };
  $('#sheetClose').onclick = closeSheet;
  $('#stepMinus').onclick = function () { stepVal -= 0.5; clampStep(); renderPreview(); };
  $('#stepPlus').onclick = function () { stepVal += 0.5; clampStep(); renderPreview(); };

  $('#saveMed').onclick = function () {
    /* 防重入：一次浮层会话只允许保存一次。连点会各新增一条药 —— 见 sheetSaveBegin 的注释。 */
    if (!sheetSaveBegin()) return;
    var isFirstMed = false;
    var name = $('#medName').value.trim();
    if (!name) { toast('请填写药品名称'); $('#medName').focus(); return; }

    /* ★ S-1：剂量可以留空；但**填了就必须是数字** ——
     * 「半片 / 一片」这类没法参与后续的库存与统计，也会让「每次 X 片」自相矛盾。
     * 校验同样放在改数据之前（沿用本函数的既有纪律：先全验完，再落值）。 */
    var doseRaw = $('#medDose').value.trim();
    if (doseRaw && !/^\d+(\.\d+)?$/.test(doseRaw)) {
      toast('剂量请填数字（如 1 或 0.5）；不填也可以');
      $('#medDose').focus(); return;
    }
    var unitRaw = $('#medUnit').value.trim();
    var noteRaw = $('#medNote').value.trim();

    /* ★ S-2：剩余量与剂量同规则 —— 可留空，填了必须是非负数字。
     * ⚠️ `0` 是**合法值**（这盒吃完了），所以判空只能用 `=== ''`。 */
    var stockRaw = $('#medStock').value.trim();
    if (stockRaw !== '' && !/^\d+(\.\d+)?$/.test(stockRaw)) {
      toast('剩余量请填数字（如 28 或 28.5）；不填也可以');
      $('#medStock').focus(); return;
    }

    /* ★ S-4：起止日期要成序 —— 反了的话 isScheduledDay 会变成“永远不吃”，
     * 而界面上看不出问题（卡片只会说“疗程已结束”）。
     * 'YYYY-MM-DD' 的字典序就是日期序 → 直接比字符串。 */
    var cfrom = $('#courseFrom') ? $('#courseFrom').value : '';
    var cto = $('#courseTo') ? $('#courseTo').value : '';
    if (cfrom && cto && cfrom > cto) { toast('开始日期不能晚于结束日期'); return; }

    /* 校验必须放在改数据**之前** —— 否则会「改一半」再报错，
     * 留下一个模式变了、时刻却没存上的坏状态。 */
    var isFixed = medModeDraft === 'fixed';
    var ts = isFixed ? normTimes(fixedTimes) : null;
    if (isFixed && !ts.length) { toast('请至少添加一个服药时刻'); return; }

    if (editingId) {
      var m = medById(editingId);
      if (m) {
        /* 先把"改动前"的样子拍下来再落值 —— 判断必须在赋值之前，
         * 否则拿新旧一比永远是"没变"。 */
        var nextSched = schedFromDraft(m);
        var prev = { mode: medMode(m), times: normTimes(m.times), interval: m.interval, sched: m.sched || null };
        var next = { mode: medModeDraft, times: ts, interval: stepVal, sched: nextSched };
        /* ★ S-3：daily 时 setOrDel 会**删掉**这个字段（不留 null / 空对象），
           备份格式保持干净，与旧数据形态一致。 */
        setOrDel(m, 'sched', nextSched);
        m.name = name;
        m.mode = medModeDraft;
        if (isFixed) m.times = ts;
        else m.interval = stepVal;
        /* ★ S-1：三个新字段（空值走 setOrDel 删属性，而不是留空串） */
        setOrDel(m, 'dose', doseRaw);
        setOrDel(m, 'unit', unitRaw);
        setOrDel(m, 'note', noteRaw);
        /* ★ 向导：剂型（「其他」与未选都不落库） */
        setOrDel(m, 'form', formDraft === '其他' ? '' : formDraft);
        /* ★ S-2：剩余量存**数字**（要参与减法）；填 0 表示这盒吃完了，是合法值 */
        setOrDel(m, 'stock', stockRaw === '' ? '' : Number(stockRaw));
        /* 模式换了 / 间隔改了 / **固定模式时刻改了** → 今天的排程要重排（已打卡的记录一律保留）。
         * ⚠️ H-1：这里原先是 `modeChanged || intervalChanged`，而固定模式下两者恒为 false
         * （intervalChanged 带 `!isFixed` 前置，modeChanged 要求模式真的变了）——
         * 「只改时刻」于是被静默忽略：今天页与系统通知仍是旧时刻，要等次日生效，
         * 用户会以为"改了没用"。判定已抽成 needRebuildDoses（可单测，
         * 见 tests/fixed.spec.js 的 H 段）。
         * 重排只负责改数据 + 撤旧通知；新通知由随后的 save() → syncNotifications() 统一登记。 */
        if (needRebuildDoses(prev, next)) rebuildTodayDoses(m);
      }
      toast('已保存');
    } else {
      var nm = { id: uid(), name: name, interval: stepVal };
      if (isFixed) { nm.mode = 'fixed'; nm.times = ts; }
      /* ★ S-3：新建时周期从草稿来（cycle 的 anchor 取今天） */
      setOrDel(nm, 'sched', schedFromDraft(null));
      /* ★ S-1：三个新字段 */
      setOrDel(nm, 'dose', doseRaw);
      setOrDel(nm, 'unit', unitRaw);
      setOrDel(nm, 'note', noteRaw);
      /* ★ 向导：剂型（「其他」与未选都不落库） */
      setOrDel(nm, 'form', formDraft === '其他' ? '' : formDraft);
      /* ★ S-2：剩余量（数字） */
      setOrDel(nm, 'stock', stockRaw === '' ? '' : Number(stockRaw));
      S.meds.push(nm);
      toast('已添加 ' + name);
      isFirstMed = true;
    }

    save();
    /* 固定时刻模式不等打卡，保存后立刻把今天的时刻排出来 */
    if (isFixed) ensureFixedDoses();
    closeSheet(); render();
    /* 首次添加药品后主动引导一次通知授权（F-3）——放在 closeSheet 之后，
     * 否则系统权限弹窗会盖在药品编辑浮层上，关掉它才能继续操作。 */
    if (!editingId && isFirstMed) guideNotifyOnce();
  };

  $('#deleteMed').onclick = function () {
    if (!editingId) return;
    var id = editingId;
    var med = medById(id);
    var n = todayDoses().filter(function (d) { return d.medId === id; }).length;
    askConfirm(
      '删除「' + (med ? med.name : '这个药品') + '」？',
      '删除后不再提醒它' + (n ? '，今天的 ' + n + ' 次提醒也会一并取消' : '')
        + '。历史服药记录会保留，不会丢数据。',
      '确认删除',
      function () {
        var r = deleteMed(id);
        save(); closeSheet(); render();
        toast('已删除 ' + (r ? r.name : ''));
      });
  };

  /* ---- 服药方式切换（F-4） ---- */
  /* ★ S-3：周期档位 / 星期 / 隔天数 —— 写点全在这里（状态与绑定同住）。 */
  $$('#schedRow [data-sched]').forEach(function (el) {
    el.onclick = function () {
      var v = el.getAttribute('data-sched');
      if (v === schedModeDraft) return;
      schedModeDraft = v;
      renderSched();
    };
  });
  $$('#weekRow [data-week]').forEach(function (el) {
    el.onclick = function () {
      var d = parseInt(el.getAttribute('data-week'), 10);
      var i = weekDraft.indexOf(d);
      if (i >= 0) weekDraft.splice(i, 1); else weekDraft.push(d);
      weekDraft.sort(function (a, b) { return a - b; });
      renderSched();
    };
  });
  var cm = $('#cycMinus'), cp = $('#cycPlus');
  if (cm) cm.onclick = function () { cycleDraft = Math.max(2, cycleDraft - 1); renderSched(); };
  if (cp) cp.onclick = function () { cycleDraft = Math.min(30, cycleDraft + 1); renderSched(); };

  /* ★ S-4：起止日期输入 → 即时刷提示（用 change 而不是 input：
     日期选择器每选一次都会触发 change，而 input 在部分 WebView 里不稳定）。 */
  ['#courseFrom', '#courseTo'].forEach(function (sel) {
    var el = $(sel);
    if (el) el.onchange = renderCourseHint;
  });

  $$('#modeRow [data-mode]').forEach(function (el) {
    el.onclick = function () {
      var v = el.getAttribute('data-mode');
      if (v === medModeDraft) return;
      medModeDraft = v;
      /* 切到固定时刻但一个时刻都没有 → 先给两个常见起点（早 8 点、晚 8 点）。
       * 面对空白输入框比面对两个可改的默认值难用得多。 */
      if (v === 'fixed' && !fixedTimes.length) fixedTimes = [480, 1200];
      renderPreview();
    };
  });

  var at = $('#addTime');
  if (at) at.onclick = function () {
    if (fixedTimes.length >= MAX_TIMES) { toast('最多 ' + MAX_TIMES + ' 个时刻'); return; }
    var ts = normTimes(fixedTimes);
    /* 默认接在最后一个时刻后面 4 小时；还没有就用 08:00 */
    var next = ts.length ? Math.min(1439, ts[ts.length - 1] + 240) : 480;
    if (ts.length && next === ts[ts.length - 1]) { toast('已经是 23:59 了，加不了更晚的'); return; }
    fixedTimes = normTimes(ts.concat([next]));
    renderTimes(); renderFixedHint();
  };

  var tl = $('#timeList');
  if (tl) {
    tl.addEventListener('change', function (ev) {
      var el = ev.target;
      if (!el || !el.getAttribute) return;
      var i = el.getAttribute('data-ti');
      if (i == null) return;
      var v = timeInputToMin(el.value);
      if (v == null) { toast('这个时刻看不懂，请重新选一次'); renderTimes(); return; }
      /* 只改这一项、**不重排** —— 用户可能还要接着改别的，列表跳来跳去很难用 */
      fixedTimes[parseInt(i, 10)] = v;
      renderFixedHint();
    });
    tl.addEventListener('click', function (ev) {
      var el = ev.target && ev.target.closest ? ev.target.closest('[data-tdel]') : null;
      if (!el) return;
      fixedTimes.splice(parseInt(el.getAttribute('data-tdel'), 10), 1);
      renderTimes(); renderFixedHint();
    });
  }

  /* ---- 录入向导的绑定（状态与绑定同住） ---- */

  /* 剂型 chips：⚠️ #formRow 的内容每次打开都重建，必须**委托**在不会被替换的
   * #formRow 本体上（逐个 onclick 会在第二次打开时静默失效 —— 项目踩过的坑）。 */
  var fr = $('#formRow');
  if (fr) fr.addEventListener('click', function (ev) {
    var el = ev.target && ev.target.closest ? ev.target.closest('[data-form]') : null;
    if (!el) return;
    var f = el.getAttribute('data-form');
    formDraft = (f === formDraft) ? '' : f;      // 再点一下取消
    renderForm();
    /* 单位联动：剂型给了默认单位且单位还空着 → 自动填；用户自己填过就不动 */
    var def = UNIT_BY_FORM[f];
    if (def && formDraft === f && $('#medUnit').value.trim() === '') $('#medUnit').value = def;
  });

  /* 快捷说明 chips：同上委托。填进备注框；再点同一个 = 清掉。备注框本身仍可自由编辑。 */
  var nr = $('#noteRow');
  if (nr) nr.addEventListener('click', function (ev) {
    var el = ev.target && ev.target.closest ? ev.target.closest('[data-note]') : null;
    if (!el) return;
    var n = el.getAttribute('data-note');
    $('#medNote').value = ($('#medNote').value.trim() === n) ? '' : n;
    renderNoteRow();
  });

  /* 长期/短期：短期显示日期输入与快捷天数；长期清空起止（= 派生回长期） */
  $$('#durRow [data-dur]').forEach(function (el) {
    el.onclick = function () {
      durDraft = el.getAttribute('data-dur');
      if (durDraft === 'long') {
        if ($('#courseFrom')) $('#courseFrom').value = '';
        if ($('#courseTo')) $('#courseTo').value = '';
      } else {
        /* 短期默认从今天开始 —— 给个起点比空着好 */
        if ($('#courseFrom') && !$('#courseFrom').value) $('#courseFrom').value = todayKey();
      }
      renderDur(); renderCourseHint();
    };
  });
  $$('#durQuick [data-durq]').forEach(function (el) {
    el.onclick = function () {
      var n = parseInt(el.getAttribute('data-durq'), 10);
      var f = todayKey();
      var d = new Date();
      d.setDate(d.getDate() + n - 1);
      var p = function (x) { return String(x).padStart(2, '0'); };
      if ($('#courseFrom')) $('#courseFrom').value = f;
      if ($('#courseTo')) $('#courseTo').value =
        d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate());
      renderCourseHint();
    };
  });

  /* 提前结束（编辑短期药）：结束日期改成今天 —— 今天照常，明天起不再提醒。
   * 破坏性弱于删除（历史全保留），但仍走二次确认 + askConfirm 纪律。 */
  var we = $('#wzEnd');
  if (we) we.onclick = function () {
    var m = editingId ? medById(editingId) : null;
    if (!m) return;
    askConfirm(
      '提前结束「' + m.name + '」？',
      '今天照常提醒，明天起不再排。已吃的记录全部保留；之后要继续吃，把结束日期改回未来即可。',
      '确认结束',
      function () {
        endMedEarly(editingId);
        save(); closeSheet(); render();
        toast('已提前结束 ' + m.name);
      });
  };

  /* 上一步 / 下一步。下一步在第 1 步要先验药名 —— 老人不能等到最后一步才发现漏填。 */
  $('#wzBack').onclick = function () {
    if (wzStep <= 1) return;
    wzStep--;
    renderWizard();
  };
  $('#wzNext').onclick = function () {
    if (wzStep === 1 && !$('#medName').value.trim()) {
      toast('请先填药品名称'); $('#medName').focus(); return;
    }
    if (wzStep >= 5) return;                     // 第 5 步没有「下一步」，只有保存
    wzStep++;
    renderWizard();
  };
}
