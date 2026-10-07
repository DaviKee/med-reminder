/* 重复弹窗防线 —— 2026-10-07 真机反馈：点「已服药」后同一种药的打卡框又弹出来。
 *
 * 桌面 Chromium 复现不了（所有静态路径都有护栏），设备上的真实路径不明。
 * 因此修法是**三道互相独立的防线**，无论真实根因是哪条，症状都被堵死：
 *   ① showReminder 自身幂等：非 pending 一律不弹（之前护栏在调用方，函数本体裸奔）；
 *   ② tick 对「同一种药 2 分钟内刚打过卡」的下一条剂量不再自动弹（冷却闸）；
 *   ③ remindDone 补 cancelOne（对齐 onNotifyAction 的 'taken' 路径 —— 那条有这条漏）。
 *
 * app.js 的提醒流程在 IIFE 里，vm 沙箱抽不动（依赖 document/窗口状态），
 * 断言为源码级 —— 与 back.spec 同一套路。每条断言写清「它防的是什么」。
 */
'use strict';
const fs = require('fs');
const path = require('path');

const APP = require('./sources').all();
const APPJS = fs.readFileSync(path.join(__dirname, '../www/js/app.js'), 'utf8');

let pass = 0, fail = 0;
const fails = [];
function t(name, ok, extra) {
  if (ok) { pass++; console.log('  ✓ ' + name); }
  else { fail++; fails.push(name); console.log('  ✗ ' + name + (extra ? ' —— ' + extra : '')); }
}

/* ⚠️ 作用域教训（menu.spec 踩过）：只查**函数体**，别查全文件 ——
 * 同名字符串出现在别的函数里会造成假绿/假红。 */
function fnBody(name) {
  const i = APPJS.indexOf('function ' + name);
  if (i < 0) return '';
  let d = 0, j = APPJS.indexOf('{', i);
  for (let k = j; k < APPJS.length; k++) {
    if (APPJS[k] === '{') d++;
    else if (APPJS[k] === '}') { d--; if (!d) return APPJS.slice(i, k + 1); }
  }
  return '';
}

console.log('=== ① showReminder 幂等护栏 ===');
const sr = fnBody('showReminder');
t('能抽到 showReminder 函数体', sr.length > 200, '改名了？');
t('★ 非 pending 一律不弹（函数自身的护栏，不再只靠调用方）',
  /ds\.status !== 'pending'\)\s*return/.test(sr), '护栏没了');
t('护栏在打开浮层之前（openDlg 之后才有效就晚了）',
  sr.indexOf("status !== 'pending'") < sr.indexOf("openDlg($('#dlgRemind'))"), '顺序不对');

console.log('=== ② tick 冷却闸 ===');
const tick = fnBody('tick');
t('能抽到 tick 函数体', tick.length > 300, '');
t('★ 同一种药 2 分钟内刚打过卡 → 不再自动弹',
  /lastTk && Date\.now\(\) - lastTk < 120000/.test(tick), '冷却闸没了');
t('冷却闸触发时静默标记 notified（不会每秒重判）',
  (tick.split('lastTk && Date.now() - lastTk < 120000')[1] || '').indexOf('S.notified[ds.id] = 1') >= 0, '');
t('冷却闸触发时同时撤系统通知',
  (tick.split('lastTk && Date.now() - lastTk < 120000')[1] || '').indexOf('cancelOne(ds.id)') >= 0, '');
t('冷却窗口按 medId 算（同药才拦，别的药照常提醒）',
  /o\.medId === ds\.medId/.test(tick), '');
t('冷却只看已打卡的（taken），pending 不算',
  /o\.status === 'taken'/.test(tick), '');

console.log('=== ③ remindDone 撤通知 ===');
/* remindDone 是 onclick 赋值，抽法不同：定位 $('#remindDone').onclick 起点 braces */
function eventBody(anchor) {
  const i = APPJS.indexOf(anchor);
  if (i < 0) return '';
  let d = 0, j = APPJS.indexOf('{', i);
  for (let k = j; k < APPJS.length; k++) {
    if (APPJS[k] === '{') d++;
    else if (APPJS[k] === '}') { d--; if (!d) return APPJS.slice(i, k + 1); }
  }
  return '';
}
const rd = eventBody("$('#remindDone').onclick");
t('能抽到 remindDone 处理体', rd.length > 200, '');
t('★ 打卡完成后撤掉该剂量的系统通知（原来漏了，靠 250ms 防抖的 sync 兜底）',
  /cancelOne\(dose\.id\)/.test(rd), '没撤通知');
t('markTaken 仍在（打卡本体没被改坏）', /markTaken\(dose, at\)/.test(rd), '');
t('onNotifyAction 的「已服用」路径本来就有 cancelOne（对齐基准）',
  eventBody("onNotifyAction").indexOf('cancelOne(ds.id)') >= 0, '');

console.log('=== ④ 幂等护栏没有误伤：三个调用方仍在 ===');
t('tick 到点仍会弹（可见时）', /showReminder\(ds\)/.test(tick), '');
t('通知点开（open）仍会弹（pending 时）', /showReminder\(ds\)/.test(APPJS), '');
t('测试提醒（浏览器降级）仍会弹', /showReminder\(td\)/.test(APPJS), '');

console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) { console.log('失败清单:\n  - ' + fails.join('\n  - ')); process.exit(1); }
