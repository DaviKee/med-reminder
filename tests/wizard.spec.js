/* 录入向导 + 剂型/单位/说明词汇表 + 长期短期 —— wizard.spec.js（2026-10-10）
 *
 * 源码级断言（index.html + ui/sheet.js）。设计文档：
 * docs/MedReminder-药品录入向导与长期短期分类-设计.md
 *
 * 铁律：向导**只重组交互不动数据** —— 唯一新数据字段是 med.form；
 * 长期/短期由 med.end 派生（durDraft 只是界面草稿）。
 */
'use strict';
const fs = require('fs');
const path = require('path');

const SHEET = fs.readFileSync(path.join(__dirname, '../www/js/ui/sheet.js'), 'utf8');
const IDX = fs.readFileSync(path.join(__dirname, '../www/index.html'), 'utf8');
const APP = require('./sources').all();
const MEDS = fs.readFileSync(path.join(__dirname, '../www/js/ui/meds.js'), 'utf8');
const ACT = fs.readFileSync(path.join(__dirname, '../www/js/ui/actions.js'), 'utf8');

let pass = 0, fail = 0;
const fails = [];
function t(name, ok, extra) {
  if (ok) { pass++; console.log('  ✓ ' + name); }
  else { fail++; fails.push(name); console.log('  ✗ ' + name + (extra ? ' —— ' + extra : '')); }
}

console.log('=== 1. 分步结构（index.html） ===');
t('5 个步号都有字段（data-wz 1~5）',
  [1, 2, 3, 4, 5].every(n => IDX.indexOf('data-wz="' + n + '"') >= 0), '');
t('剂型 chips 行存在', IDX.indexOf('id="formRow"') >= 0 && IDX.indexOf('data-form=') === -1 || IDX.indexOf('id="formRow"') >= 0, '');
t('快捷说明 chips 行存在', IDX.indexOf('id="noteRow"') >= 0, '');
t('长期/短期 chips 行存在', IDX.indexOf('id="durRow"') >= 0 && IDX.indexOf('data-dur="long"') >= 0 && IDX.indexOf('data-dur="short"') >= 0, '');
t('快捷天数（7/14 天）存在', IDX.indexOf('data-durq="7"') >= 0 && IDX.indexOf('data-durq="14"') >= 0, '');
t('向导导航（进度/上一步/下一步）存在',
  IDX.indexOf('id="wzProg"') >= 0 && IDX.indexOf('id="wzBack"') >= 0 && IDX.indexOf('id="wzNext"') >= 0, '');

console.log('=== 2. sheet.js 向导状态机 ===');
t('步标题 5 步齐全', /STEP_TITLES = \['', '这个药是什么', '怎么吃', '什么时候提醒', '哪些天吃', '吃多久'\]/.test(SHEET), '');
t('★ renderWizard 按步显隐（data-wz 全量扫描）',
  /renderWizard\(\)[\s\S]{0,300}data-wz/.test(SHEET) || SHEET.indexOf("[data-wz]") >= 0, '');
t('★ 保存键只到最后一步出现', /wzStep !== 5/.test(SHEET), '');
t('第 1 步先验药名（别让老人到最后一步才发现漏填）',
  /wzStep === 1 && !\$\('#medName'\)\.value\.trim\(\)/.test(SHEET), '');
t('「下一步」到第 5 步为止（保存另有按钮）', /wzStep >= 4\) return/.test(SHEET), '');

console.log('=== 3. 剂型 → 单位词汇表 ===');
t('剂型清单覆盖国内常见剂型',
  ['片剂', '胶囊', '颗粒', '口服液', '丸剂', '贴剂', '喷剂', '滴剂', '软膏', '其他'].every(f => SHEET.indexOf("'" + f + "'") >= 0), '');
t('单位与剂型联动（映射表存在）', /UNIT_BY_FORM/.test(SHEET) && /'胶囊': '粒'/.test(SHEET) && /'颗粒': '袋'/.test(SHEET), '');
t('单位只在为空时自动填（用户填过就不动）', /#medUnit'\)\.value\.trim\(\) === ''/.test(SHEET), '');
t('「其他」不落库', /'其他' \? '' : formDraft/.test(SHEET), '');
t('form 走 setOrDel 纪律（空值删属性不留空串）', APP.indexOf("setOrDel(nm, 'form'") >= 0, '');

console.log('=== 4. 长期 / 短期 ===');
t('★ 不新增数据字段：durDraft 只是草稿，保存的只有疗程日期',
  /durDraft = 'long'/.test(SHEET) && !/med\.dur|nm\.dur|setOrDel\((m|nm), 'dur'/.test(SHEET), '');
t('选长期 → 清空疗程起止（派生回长期）', /durDraft === 'long'[\s\S]{0,120}courseFrom'\)\.value = ''/.test(SHEET), '');
t('选短期 → 开始日期默认今天（给个起点比空着好）', /!getComputedStylePlaceholder|#courseFrom'\) && !\$\('#courseFrom'\)\.value\) \$\('#courseFrom'\)\.value = todayKey\(\)/.test(SHEET)
  || /value\) \$\('#courseFrom'\)\.value = todayKey\(\)/.test(SHEET), '');
t('短期快捷天数（今天起算 N-1 天）', /setDate\(d\.getDate\(\) \+ n - 1\)/.test(SHEET), '');
t('openSheet 时长草稿从疗程字段派生（编辑一次不能把原有值冲掉）',
  /\(sc0\.from \|\| sc0\.to\) \? 'short' : 'long'/.test(SHEET), '');

console.log('=== 5. 既有能力不回归 ===');
t('三种提醒模式都还在（固定/间隔）', IDX.indexOf('data-mode="fixed"') >= 0 && IDX.indexOf('data-mode="interval"') >= 0, '');
t('三种周期都还在（每天/每周/隔几天）', IDX.indexOf('data-sched="daily"') >= 0 && IDX.indexOf('data-sched="weekly"') >= 0 && IDX.indexOf('data-sched="cycle"') >= 0, '');
t('保存 handler 的防重入守卫仍在', SHEET.indexOf('sheetSaveBegin()') >= 0, '');
t('剂型 chips 走事件委托（重建后不失效的项目坑）',
  SHEET.indexOf("fr.addEventListener('click'") >= 0
  && SHEET.indexOf("$$('#formRow [data-form]').forEach") < 0, '');

console.log('=== 6. M3 长期/短期分组 + 提前结束 ===');
t('★ medClass 派生函数存在（零新数据字段）', /function medClass\(m\)/.test(APP) && /return dayDiff\(s\.to, todayKey\(\)\) > 0 \? 'ended' : 'short'/.test(APP), '');
t('药品页分三组（长期/短期/已结束）',
  MEDS.indexOf('长期服药') >= 0 && MEDS.indexOf('短期用药') >= 0 && MEDS.indexOf('已结束') >= 0, '');
t('短期卡带剩余天数角标（今天=最后一天有专属文案）',
  /shortLeft/.test(MEDS) && /今天最后一天/.test(MEDS) && /还剩 ' \+ diff \+ ' 天/.test(MEDS), '');
{
  const eme = (ACT.match(/export function endMedEarly[\s\S]*?\n\}/) || [''])[0];
  t('★ endMedEarly 只改结束日期，不删药不动历史',
    eme.indexOf('s.to = todayKey()') >= 0 && eme.indexOf('S.meds') < 0, '函数体不对');
}
t('★ 提前结束走二次确认（askConfirm）', SHEET.indexOf("askConfirm(\n      '提前结束") >= 0 || /askConfirm\([\s\S]{0,60}提前结束/.test(SHEET), '');
t('提前结束按钮只在编辑短期药时出现', /we\.classList\.toggle\('hidden', !\(m && medClass\(m\) === 'short'\)\)/.test(SHEET), '');
t('按钮文案说清"记录保留"（别让人以为在删数据）', IDX.indexOf('已吃的记录全部保留') >= 0, '');

console.log('\n通过 ' + pass + ' / 共 ' + (pass + fail));
if (fail) { console.log('失败清单:\n  - ' + fails.join('\n  - ')); process.exit(1); }
