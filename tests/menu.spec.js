/* 记录页「二级菜单整合」—— 2026-10-07
 *
 * 背景：月历 / 显示 / 备份 / 调试全铺在记录页上，滚动过长、信息过载（秦老师反馈）。
 * 整合后页面只留概览（本周 + 打卡连击 + 依从率）+ 功能入口列表，
 * 每个入口开各自的浮层。本 spec 锁三件事：
 *   ① 页面上确实只剩入口、大块内容确实搬走了；
 *   ② 三个新浮层的壳在 index.html、**且登记进了 closableDialogs**（返回键关不掉的坑）；
 *   ③ 各浮层的内容注入与绑定接线齐全。
 */
'use strict';
const fs = require('fs');
const path = require('path');

const APP = require('./sources').all();
const REC = fs.readFileSync(path.join(__dirname, '../www/js/ui/records.js'), 'utf8');
const IDX = fs.readFileSync(path.join(__dirname, '../www/index.html'), 'utf8');
const OVL = fs.readFileSync(path.join(__dirname, '../www/js/ui/overlay.js'), 'utf8');
const DATA = fs.readFileSync(path.join(__dirname, '../www/js/ui/data.js'), 'utf8');
const VIZ = fs.readFileSync(path.join(__dirname, '../www/js/ui/viz.js'), 'utf8');

let pass = 0, fail = 0;
const fails = [];
function t(name, ok, extra) {
  if (ok) { pass++; console.log('  ✓ ' + name); }
  else { fail++; fails.push(name); console.log('  ✗ ' + name + (extra ? ' —— ' + extra : '')); }
}

console.log('=== 1. 页面瘦身 ===');
t('菜单卡存在（MORE）', /menu-card/.test(REC) && /MORE/.test(REC), '');
t('六个入口齐全（viz/hist/report/data/disp/debug）',
  ['viz', 'hist', 'report', 'data', 'disp', 'debug'].every(k => REC.indexOf("menuRow('" + k + "'") >= 0), '');
t('页面不再铺月历（vizHtml 不在 renderRecords 里调用）',
  !/vizHtml\(\)/.test(REC), '');
t('页面不再铺历史摘要（historySummaryHtml 只喂浮层）',
  REC.indexOf('html += historySummaryHtml();') < 0, '');
/* ⚠️ 作用域：只查 **renderRecords 函数体** —— displayHtml/debugHtml 的 markup
 * 合法地住在 records.js 里（供浮层注入用），查全文件必假红。 */
const RR = (REC.match(/function renderRecords\([\s\S]*?\n\}/) || [''])[0];
t('页面不再铺显示卡 / 存储卡 / 自动备份卡 / DEBUG 卡',
  ['显示 · 主题', 'DATA · 备份', 'DEBUG · 验收用'].every(s => RR.indexOf(s) < 0),
  '还有块留在页面');
t('概览仍保留：本周点阵 + 连续打卡 + 依从率',
  /THIS WEEK/.test(REC) && /STREAK/.test(REC) && /ADHERENCE/.test(REC), '');

console.log('=== 2. 三个新浮层：壳 + 返回键登记 ===');
['dlgViz', 'dlgDisplay', 'dlgDebug'].forEach(id => {
  t('index.html 有 ' + id + ' 壳', IDX.indexOf('id="' + id + '"') >= 0, '');
  t('★ ' + id + ' 登记进 closableDialogs（返回键能关）', OVL.indexOf("'" + id + "'") >= 0, '');
});
t('关闭按钮用 data-dlg-close（三个壳共用一段绑定）',
  (IDX.match(/data-dlg-close/g) || []).length >= 3 && /bindDlgClose/.test(REC), '');

console.log('=== 3. 各浮层内容注入与绑定 ===');
t('openVizDlg 注入 vizBody 并打开', /openVizDlg/.test(VIZ) && /vizBody'\)\.innerHTML = vizHtml\(\)/.test(VIZ), '');
t('翻月只刷浮层内容（不再全页 render）',
  /vizBody'\)\.innerHTML = vizHtml\(\)/.test(VIZ) && !/render\(\)/.test(VIZ.split('openVizDlg')[1] || ''), '');
t('openDisplayDlg 注入 displayHtml + 绑定', /openDisplayDlg/.test(REC) && /displayHtml\(\)/.test(REC), '');
t('★ 主题按钮仍用 data-theme-mode（不能占用 data-theme）',
  /data-theme-mode/.test(REC) && /\$\$\('\[data-theme-mode\]'\)/.test(REC), '');
t('openDebugDlg 注入 debugHtml + 测试提醒绑定仍在',
  /openDebugDlg/.test(REC) && /btnTest/.test(REC) && /testReminderImpl/.test(REC), '');
t('★ DEBUG 一键清理按钮仍在（notify.spec / verify-apk 盯着；字面量在 permission.js 的 diagHtml）',
  /id="btnPurgeNotif"/.test(APP), '');
t('dlgData 打开时注入存储/自动备份卡（dataCards）',
  /dataCards/.test(DATA) && /storageCardHtml\(\) \+ autoBackupCardHtml\(\)/.test(DATA), '');
t('自动备份 / 清理按钮的绑定随卡片注入（不再依赖 renderRecords）',
  /btnAutoBak/.test(DATA) && /btnClean/.test(DATA) && REC.indexOf("$('#btnAutoBak')") < 0, '');

console.log('=== 4. 菜单行绑定 ===');
t('菜单行在 renderRecords 重建后绑定（data-entry）',
  /\$\$\('\[data-entry\]'\)/.test(REC) && /openHistoryDlg\(\)/.test(REC) && /openDataDlg\('backup'\)/.test(REC), '');
t('viz.spec / history.spec 关心的契约未被破坏：renderRecords 不再铺长列表',
  !/hdays\.forEach/.test((APP.match(/function renderRecords\([\s\S]*?\n\}/) || [''])[0]), '');

console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) { console.log('失败清单:\n  - ' + fails.join('\n  - ')); process.exit(1); }
