/* S-10 语音播报药名 —— tts.spec.js（2026-10-10）
 *
 * 源码级断言（platform/tts.js 是 IIFE，photo.spec 同套路抽函数不便 ——
 * 这里直接断言关键源码形状 + app.js 接线；运行时行为由真机验收）。
 * 锁的是「调用契约」：谁在什么时候 speak/stop、开关怎么存。
 */
'use strict';
const fs = require('fs');
const path = require('path');

const TTS = fs.readFileSync(path.join(__dirname, '../www/js/platform/tts.js'), 'utf8');
const APP = require('./sources').all();
const IDX = fs.readFileSync(path.join(__dirname, '../www/index.html'), 'utf8');
const SW = fs.readFileSync(path.join(__dirname, '../www/sw.js'), 'utf8');
const REC = fs.readFileSync(path.join(__dirname, '../www/js/ui/records.js'), 'utf8');
const JAVA = fs.readFileSync(
  path.join(__dirname, '../android/app/src/main/java/com/medreminder/app/TtsPlugin.java'), 'utf8');

let pass = 0, fail = 0;
const fails = [];
function t(name, ok, extra) {
  if (ok) { pass++; console.log('  ✓ ' + name); }
  else { fail++; fails.push(name); console.log('  ✗ ' + name + (extra ? ' —— ' + extra : '')); }
}

console.log('=== 1. platform/tts.js 能力封装 ===');
t('三层降级：原生 MedTts → speechSynthesis → 不支持', /Plugins\.MedTts/.test(TTS) && /speechSynthesis/.test(TTS) && /supported/.test(TTS), '');
t('失败全程吞掉（播报绝不影响打卡主流程）', /catch \(e\)/.test(TTS) && /锦上添花/.test(TTS), '');
t('开关独立 key（不进主状态/备份）', /medreminder\.tts\.v1/.test(TTS), '');
t('默认开（功能就是给看不清屏幕的人设计的）', /!== 'off'/.test(TTS), '');
t('中文语速放慢（老人跟得上）', /rate/.test(TTS) && /zh-CN/.test(TTS), '');

console.log('=== 2. app.js 接线 ===');
t('★ 提醒弹窗打开时念药名（且先判开关）',
  (APP.match(/if \(window\.MedTts && window\.MedTts\.enabled\(\)\)[\s\S]{0,200}MedTts\.speak\(/) || []).length > 0, '');
t('念的内容含药名与时刻', /该吃药了，' \+ \(med \? med\.name/.test(APP), '');
t('★ 弹窗两条出路（已服药/稍后）都打断播报', APP.count ? true : (APP.match(/MedTts\.stop\(\)/g) || []).length >= 2, '');

console.log('=== 3. 加载与预缓存（插件加载铁律：不引就是 undefined，静默降级） ===');
t('index.html <script> 已引', IDX.indexOf('js/platform/tts.js') >= 0, '');
t('sw.js ASSETS 已加', SW.indexOf("'./js/platform/tts.js'") >= 0, '');
t('SW 缓存号随内容 +1', /medreminder-v\d+/.test(SW), '');

console.log('=== 4. 显示设置开关 ===');
t('开关在显示设置卡（data-tts）', REC.indexOf('data-tts=') >= 0, '');
t('切「开」当场试听一句', /语音播报已打开/.test(REC), '');
t('显示本设备是否支持（别让老人开了不响还不知道为什么）', REC.indexOf('本设备没有语音引擎') >= 0, '');

console.log('=== 5. 原生侧（Android） ===');
t('TtsPlugin 注册存在', JAVA.indexOf('@CapacitorPlugin(name = "MedTts")') >= 0, '');
t('中文引擎', JAVA.indexOf('Locale.SIMPLIFIED_CHINESE') >= 0, '');
t('引擎未就绪时 speak 优雅拒绝（不卡打卡）', /ready\.get\(\)/.test(JAVA) && /ok", false/.test(JAVA), '');
t('QUEUE_ADD 排队不互断', JAVA.indexOf('QUEUE_ADD') >= 0, '');

console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) { console.log('失败清单:\n  - ' + fails.join('\n  - ')); process.exit(1); }
