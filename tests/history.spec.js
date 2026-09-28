/* 「记录」页的历史：摘要 + 二级菜单（2026-09-28 新增功能）
 *
 * 现场：「HISTORY · 近 14 天」把每一天都铺成一张卡片，用得越久越长、翻不到底。
 * 改法：页面只留「摘要 + 最近 3 天 + 查看全部历史(N 天)」，完整按天列表进二级菜单。
 *
 * ⚠️ 这条**必须**配行为断言，不能只比源码字符串：
 *   「摘要只列 3 天」和「菜单列全部天」是这次改动的**全部意义**，
 *   而它们都只是字符串拼接的结果 —— 只有真跑一遍才验得到。
 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const APP_SRC = require('./sources').all();
const HTML = fs.readFileSync(path.join(ROOT, 'www/index.html'), 'utf8');
const CSS = fs.readFileSync(path.join(ROOT, 'www/css/app.css'), 'utf8');
const OVERLAY = fs.readFileSync(path.join(ROOT, 'www/js/ui/overlay.js'), 'utf8');

const strip = s => s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
const APP = strip(APP_SRC);

let pass = 0, fail = 0;
const fails = [];
function t(name, cond, extra) {
  if (cond) { pass++; console.log('  \u2713 ' + name); }
  else { fail++; fails.push(name + '  [' + extra + ']'); console.log('  \u2717 ' + name + '   [' + extra + ']'); }
}

function cut(src, header) {
  const i = src.indexOf(header);
  if (i < 0) throw new Error('抽不到: ' + header);
  const b = src.indexOf('{', i);
  let d = 0, j = b;
  while (j < src.length) {
    if (src[j] === '{') d++;
    else if (src[j] === '}') { d--; if (d === 0) break; }
    j++;
  }
  return src.slice(i, j + 1);
}

/* 有些工具是 `var pad = function (...) {...};` 形式，cut() 抓不到函数声明 ——
 * 直接整行取过来，保证与源码一字不差（不手抄语义，免得以后源码改了测试还自说自话）。 */
function line(src, prefix) {
  const i = src.indexOf(prefix);
  if (i < 0) throw new Error('抽不到行: ' + prefix);
  const j = src.indexOf('\n', i);
  return src.slice(i, j < 0 ? src.length : j);
}

console.log('=== 0. 自检 ===');
t('聚合源码非空', APP.length > 50000, APP.length);
t('自检：能抽到 historySummaryHtml', cut(APP, 'function historySummaryHtml(').length > 200, '改名了？');

console.log('');
console.log('=== 1. ★ 行为：摘要短、列表全（跑真代码）===');
const ctx = vm.createContext({});
vm.runInContext([
  /* 数据桩：meds 是必须的 —— histDoseRowHtml 要靠 medById 把 medId 翻成药名 */
  'var S = { doses: {}, meds: [{ id: "m1", name: "药一" }, { id: "m2", name: "药二" }] };',
  'var histMedId = null;',
  'var HIST_DAYS = 14;',
  'var MISS_GRACE_MIN = 30;',
  'var ICON = { cam: "<svg></svg>" };',      // 图标内容与本次断言无关，只保证有值
  'function todayKey() { return "2026-09-28"; }',
  'function nowMin() { return 600; }',
  cut(APP, 'function esc('),
  line(APP, 'var pad = '),                   // minToStr 依赖它（var 形式，cut 抓不到）
  cut(APP, 'function minToStr('),
  cut(APP, 'function medById('),             // core/schedule.js
  cut(APP, 'function dueAt('),               // core/schedule.js
  cut(APP, 'function historyDays('),
  cut(APP, 'function histDayLabel('),
  cut(APP, 'function histStatusLabel('),
  cut(APP, 'function histDayMatch('),
  cut(APP, 'function histDayDoses('),
  cut(APP, 'function histDays('),
  cut(APP, 'function histDoseRowHtml('),
  cut(APP, 'function historySummaryHtml('),
  cut(APP, 'function historyListHtml('),
  'globalThis.api = { sum: historySummaryHtml, list: historyListHtml,'
  + ' setFilter: function (v) { histMedId = v; }, setDoses: function (d) { S.doses = d; } };',
].join('\n'), ctx);
const api = ctx.api;

/* 造 10 天数据：每天都有 m2；m1 只在偶数天（这样按药筛选能看出差别） */
const doses = {};
for (let i = 0; i < 10; i++) {
  const k = '2026-09-' + String(28 - i).padStart(2, '0');
  doses[k] = [{ id: k + 'b', medId: 'm2', time: 1200, status: 'taken' }];
  if (i % 2 === 0) doses[k].unshift({ id: k + 'a', medId: 'm1', time: 480, status: 'taken' });
}
api.setDoses(doses);

const sum = api.sum();
const list = api.list();

t('摘要报了天数总量（共 10 天）', /共 <b>10<\/b> 天/.test(sum), sum.slice(0, 160));
t('摘要报了次数总量（合计 15 次 = 10 + 5）', /合计 <b>15<\/b> 次/.test(sum), sum.slice(0, 160));
t('★ 摘要只列最近 3 天（不是 10 天）',
  (sum.match(/justify-content:space-between/g) || []).length === 3,
  (sum.match(/justify-content:space-between/g) || []).length);
t('摘要里的 3 天是**最新的**那 3 天（含今天）', sum.indexOf('今天 · 2026-09-28') >= 0
  && sum.indexOf('09-27') >= 0 && sum.indexOf('09-26') >= 0, sum.slice(0, 240));
t('摘要不含第 4 天（09-25）', sum.indexOf('09-25') < 0, '摘要漏出更多天了');
t('★ 入口按钮标出完整天数', sum.indexOf('查看全部历史（10 天）') >= 0, sum.slice(-140));
t('★ 二级菜单列**全部** 10 天（不是 3 天）',
  (list.match(/<div class="card sched">/g) || []).length === 10,
  (list.match(/<div class="card sched">/g) || []).length);
t('菜单里每天的量都在（15 条剂量行）',
  (list.match(/class="dose"/g) || []).length === 15, (list.match(/class="dose"/g) || []).length);

console.log('');
console.log('=== 2. 按药筛选：摘要与列表一起变 ===');
api.setFilter('m1');
const sum1 = api.sum();
const list1 = api.list();
t('筛 m1 后摘要变成 5 天', /共 <b>5<\/b> 天/.test(sum1), sum1.slice(0, 120));
t('筛 m1 后合计 5 次（只算该药）', /合计 <b>5<\/b> 次/.test(sum1), sum1.slice(0, 120));
t('筛 m1 后按钮天数也跟着变', sum1.indexOf('查看全部历史（5 天）') >= 0, sum1.slice(-120));
t('筛 m1 后菜单只剩 5 天', (list1.match(/<div class="card sched">/g) || []).length === 5,
  (list1.match(/<div class="card sched">/g) || []).length);
api.setFilter(null);
t('清掉筛选后回到 10 天', /共 <b>10<\/b> 天/.test(api.sum()), '筛选状态没复位');

console.log('');
console.log('=== 3. 空数据不崩 ===');
api.setDoses({});
t('没有记录时摘要给引导文案而不是空白', /还没有服药记录/.test(api.sum()), api.sum());
t('没有记录时列表也给文案', /还没有服药记录/.test(api.list()), api.list());
t('空数据时不存在入口按钮（没什么可看）',
  api.sum().indexOf('data-hall') < 0, '空列表还给了入口');
api.setDoses(doses);

console.log('');
console.log('=== 4. 结构：二级菜单的骨架与关闭路径 ===');
t('index.html 有 dlgHistory', /id="dlgHistory"/.test(HTML), '缺');
t('菜单里有筛选区 / 列表区 / 关闭按钮',
  /id="histFilter"/.test(HTML) && /id="histBody"/.test(HTML) && /id="histClose"/.test(HTML), '缺元素');
t('★ 返回键能关掉它（加进了 closableDialogs）',
  /'dlgHistory'/.test(strip(OVERLAY)), '返回键关不掉，只能点关闭');
t('#histClose 接了线', /\$\('#histClose'\)\.onclick/.test(APP), '没接');
t('★ 列表区自己有高度上限（否则对话框被撑出屏幕）',
  /\.dlg-scroll\{[^}]*max-height/.test(strip(CSS)), '缺 max-height');
t('列表区可滚动', /\.dlg-scroll\{[^}]*overflow-y:auto/.test(strip(CSS)), '不能滚');

console.log('');
console.log('=== 5. 结构：事件委托（这是"点了没反应"的防线）===');
t('★ 筛选走 document 级委托', /document\.addEventListener\('click'/.test(APP), '还是直接绑元素');
t('委托用 closest 命中 chips 与入口按钮',
  /closest\('\[data-hist\]'\)/.test(APP) && /closest\('\[data-hall\]'\)/.test(APP), '缺');
t('★ 旧的直接绑定已删除（否则双触发/失效）',
  !/\$\$\('\[data-hist\]'\)\.forEach/.test(APP), '旧绑定还在');
t('筛选变化时页面与菜单一起刷新',
  /refreshHistoryDlg\(false\)/.test(APP), '菜单不会跟着变');
t('页面不再直接铺全部天（摘要是唯一入口）',
  !/hdays\.forEach/.test(cut(APP, 'function renderRecords(')), '页面还在铺长列表');

console.log('');
console.log('通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) {
  console.log('  失败清单：');
  fails.forEach(f => console.log('    - ' + f));
}
process.exit(fail ? 1 : 0);
