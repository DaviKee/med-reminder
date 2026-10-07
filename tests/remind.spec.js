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
/* 同上，但作用于任意源码文本（photo.js 等模块也要抽函数体） */
function fnBodyIn(src, name) {
  const i = src.indexOf('function ' + name);
  if (i < 0) return '';
  let d = 0, j = src.indexOf('{', i);
  for (let k = j; k < src.length; k++) {
    if (src[k] === '{') d++;
    else if (src[k] === '}') { d--; if (!d) return src.slice(i, k + 1); }
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
/* showReminder 现在带第二参数 via（取证日志来源），所以用 [,)] 匹配调用 */
t('tick 到点仍会弹（可见时）', /showReminder\(ds[,)]/.test(tick), '');
t('通知点开（open）仍会弹（pending 时）', /showReminder\(ds[,)]/.test(APPJS), '');
t('测试提醒（浏览器降级）仍会弹', /showReminder\(td[,)]/.test(APPJS), '');

console.log('=== ⑤ 第四道防线：浮层开着不弹新打卡框（2026-10-07 二次反馈） ===');
/* 真机时序：点「已服药」→ 拍照框弹出 → tick 每秒照跑，同药另一条到点剂量
 * 把打卡框叠在拍照框上再弹 → 用户体感"点了没效还得再点一次"。 */
t('★ tick 弹窗前检查 isOverlayOpen', /isOverlayOpen\(\)/.test(tick), '');
t('★ 检查位置在 showReminder 之前（弹完再查就晚了）',
  tick.indexOf('isOverlayOpen()') >= 0
  && tick.indexOf('isOverlayOpen()') < tick.indexOf('showReminder(ds'), '顺序不对');
t('★ 浮层开着时不置已告知标记（否则浮层关了就永远不弹了）',
  /visibilityState === 'visible' && isOverlayOpen\(\)/.test(tick), '');

console.log('=== ⑥ 恢复路径不再丢打卡（photo.js resumeShot） ===');
/* resumeShot 原来只认药品 id，而 remindDone/btnEarly/通知路径传的都是剂量 id
 * → App 拍照期间被杀的用户回来发现"没记上"，只能再点一次。 */
const PHOTO = fs.readFileSync(path.join(__dirname, '../www/js/ui/photo.js'), 'utf8');
const rs = fnBodyIn(PHOTO, 'resumeShot');
t('能抽到 resumeShot 函数体', rs.length > 300, '');
t('★ 先按剂量 id 找（三种调用方传的都是剂量 id）', /findDoseById\(p\.doseId\)/.test(rs), '');
t('★ 找到 pending 剂量直接 markTaken（与死亡流程本来要做的事一致）',
  /markTaken\(ds0, Date\.now\(\)\)/.test(rs), '');
t('药品 id 老路径（data-checkin）仍走 checkIn', /checkIn\(p\.doseId\)/.test(rs), '');
t('兜底挂照片优先挑 pending 的剂量（旧代码拿第一条，可能挂到已服用的上）',
  rs.indexOf("status === 'pending'") >= 0 && rs.indexOf('hitAny') >= 0, '');

console.log('=== ⑦ 弹窗取证日志（真机复现时的时间线证据） ===');
t('showReminder 写弹窗日志', /pushRemindLog\(/.test(fnBody('showReminder')), '');
t('remindDone 写「已服药」日志', /pushRemindLog\(/.test(rd), '');
t('onNotifyAction taken 路径也写日志', /pushRemindLog\(/.test(eventBody('onNotifyAction')), '');
t('调试卡渲染时间线（remindLogHtml）', /remindLogHtml/.test(
  fs.readFileSync(path.join(__dirname, '../www/js/ui/records.js'), 'utf8')), '');

console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) { console.log('失败清单:\n  - ' + fails.join('\n  - ')); process.exit(1); }
