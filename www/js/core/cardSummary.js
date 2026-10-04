/* core/cardSummary.js —— 桌面卡片的数据摘要（S-9 的 web 侧，2026-10-04）
 *
 * 为什么摘要要在 web 层**算好再落盘**（而不是让卡片自己算）：
 *   · ArkTS 卡片**读不到 WebView 的 localStorage** —— 必须经文件中转；
 *   · 卡片侧有 `onAddForm` 10 秒之类的限制，**不该现算排程**；
 *   · 定时刷新最短 30 分钟 → 卡片只负责「展示」，不承担提醒。
 *   见 `docs/MedReminder-OpenHarmony7-特性研究.md` §9。
 *
 * ⚠️ 只写**同应用沙箱**（`Directory.Data`），不碰公共目录、不申请任何权限 ——
 *    这是 MVP 能立刻落地的关键（代理刷新 / 推送那些受控能力二期再说）。
 *
 * ⭐ 这份摘要**双端通用**：将来 Android 的 widget 也用同一份数据源，
 *    所以它放在 `core/`（不带任何平台差异）。
 */
import { S } from './store.js';
import { progress, nextPending, medById, dueAt } from './schedule.js';
import { minToStr, todayKey } from './util.js';

var CARD_DIR = 'card';
var CARD_PATH = 'card/card_state.json';

/* 摘要就回答三件事：下次吃什么、几点、今天进度多少。
 *
 * **全字段字符串化** —— 卡片侧取值时类型不确定，字符串最省心；
 * 也顺手避开 `null` / `NaN` 在 JSON 里的歧义。空字符串 = 「没有」。
 * （例：今天的药都吃完了 → nextTime / nextMed 都是 ''，卡片据此显示「今天已完成」。） */
export function computeSummary() {
  var p = progress();
  var nxt = nextPending();
  var med = nxt ? medById(nxt.medId) : null;
  return {
    date: todayKey(),
    /* 用 dueAt 而不是 nxt.time：延后过的那次按**延后时刻**算，
     * 否则卡片会显示一个已经过去的时间（同 nextPending 的排序口径）。 */
    nextTime: nxt ? minToStr(dueAt(nxt)) : '',
    nextMed: med ? med.name : '',
    done: String(p.done),
    total: String(p.total)
  };
}

/* 把摘要写进沙箱文件，供 ArkTS 卡片读。
 *
 * 失败**一律静默** —— 卡片是「锦上添花」，它坏了绝不能影响打卡本身
 * （这条路径挂在每次 save() 上，而 save() 是用户操作的关键路径）。
 * 插件不存在（纯浏览器 / 未装插件）→ 直接跳过。 */
export function writeCardFile() {
  var C = (typeof window !== 'undefined') ? window.Capacitor : null;
  var FS = (C && C.Plugins && C.Plugins.Filesystem) || null;
  if (!FS || typeof FS.writeFile !== 'function') return Promise.resolve(null);

  var text;
  try { text = JSON.stringify(computeSummary()); } catch (e) { return Promise.resolve(null); }

  /* ⚠️ writeFile **不会**自动建父目录（父目录不存在会 reject
   * 「Parent folder doesn't exist」）—— 必须先 mkdir；已存在会抛，忽略即可。
   * 这是项目里栽过的坑，见 platform/storage.js 的同一段注释。 */
  return FS.mkdir({ path: CARD_DIR, directory: 'DATA', recursive: true })
    .catch(function () { /* 已存在 */ })
    .then(function () {
      return FS.writeFile({
        path: CARD_PATH, data: text, directory: 'DATA', encoding: 'utf8'
      });
    })
    .catch(function () { /* 静默：卡片坏了不能连累主流程 */ });
}
