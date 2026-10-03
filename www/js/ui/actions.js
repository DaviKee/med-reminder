/* ui/actions.js —— 业务动作：删除药品 / 打卡提示 / 字号提示 / 通知同步
 *
 * 2026-10-01 架构重构（A-2 第 15 步）：从 app.js 抽出。
 * 函数体一字未改，只去掉一层缩进并加 export。
 *
 * ⚠️ 为什么是 `ui/` 而不是原计划的 `core/actions.js`：
 *    这些动作要调 `render` / `toast`（都在 `ui/`），而 `ui/render.js`、`ui/toast.js`
 *    本身就只依赖 `core/util.js` —— 依赖方向是**按实际依赖算的，不是按目录名**。
 *    放进 `core/` 反而会让 core 反向依赖 ui，那才是真的破规矩。
 *
 * ⚠️ `rollDayIfNeeded`（用 `lastDay`）与 `setTab`（用 `currentTab`）**故意没搬** ——
 *    那两个变量在 app.js 里有外部写点，import 绑定只读，搬走就 TypeError。
 *    要搬得先给它们配套 setter。
 */
import { minToStr, minOfDay, dateAt, todayKey } from '../core/util.js';
import { S, save } from '../core/store.js';
import { dueAt, medById, todayDoses, ensureFixedDoses } from '../core/schedule.js';
import { FS_HINT_KEY } from './cards.js';
import { render } from './render.js';

export function dismissFsHint() {
  try { localStorage.setItem(FS_HINT_KEY, '1'); } catch (e) { /* ignore */ }
}

export function syncNotifications() {
  if (!window.MedNotify) return;
  var list = todayDoses()
    .filter(function (d) { return d.status === 'pending'; })
    .map(function (d) {
      var m = medById(d.medId);
      /* 系统通知的**响铃时刻**用 dueAt（延后过就按延后时刻），
       * 但正文里仍写计划时刻 —— 用户关心的是"这次药本来该几点吃"。
       * 延后过则把延后时刻一并说清，避免"通知怎么晚响了"的困惑。 */
      var t = minToStr(d.time);
      if (d.snoozeUntil != null) t += '（已延后至 ' + minToStr(d.snoozeUntil) + '）';
      return { id: d.id, timeStr: t, medName: m ? m.name : '服药', at: dateAt(dueAt(d)) };
    });
  window.MedNotify.sync(list);
}

/* 顺延后重排序号，保证「第 N / 共 M 次」仍然正确 */
/* 删除药品。两件事缺一不可：
 *   ① **撤掉今天该药已经排进系统的通知**。旧实现只清了 App 内的剂量数组，
 *      系统闹钟照旧会响；点开时那一刻 doseId 已经查不到了（onNotifyAction 里
 *      `if (!ds) return;`）—— 表现就是「点了没反应」。
 *   ② **保留历史记录**。吃过药的事实不该因为药品被删掉而消失，
 *      否则按药统计和依从率会凭空变好看。 */
export function deleteMed(id) {
  var med = medById(id);
  if (!med) return null;
  var todays = todayDoses().filter(function (d) { return d.medId === id; });
  todays.forEach(function (d) { if (window.MedNotify) window.MedNotify.cancelOne(d.id); });
  S.meds = S.meds.filter(function (m) { return m.id !== id; });
  S.doses[todayKey()] = todayDoses().filter(function (d) { return d.medId !== id; });
  return { name: med.name, doses: todays.length };
}

export function takenToast(med, takenMs, r) {
  var s = '已记录 ' + minToStr(minOfDay(takenMs)) + ' · ' + (med ? med.name : '');
  if (r && r.shifted) s += '，后续 ' + r.shifted + ' 次已顺延';
  if (r && r.dropped) s += '，' + r.dropped + ' 次越过零点不再提醒';
  return s;
}

/* 跨天处理 —— **三个入口共用**：启动后、30 秒轮询、以及**从后台回到前台**。
 * ⚠️ 最后一处最容易被漏掉：App 在后台时 JS 定时器被系统暂停，后台过夜就收不到轮询；
 * 回到前台若不补这一下，今天一条剂量都不会生成 —— 用户此时点打卡，就会走进
 * checkIn 的「从此刻起按间隔排」，表现为「固定时刻被顺延」（2026-09-28 真机反馈）。 */
export var lastDay = null;

export function rollDayIfNeeded() {
  if (todayKey() === lastDay) return false;
  lastDay = todayKey();
  S.notified = {};
  save();
  ensureFixedDoses();      // 新的一天，固定时刻要重新排一遍
  render();
  syncNotifications();
  return true;
}

/* 「今天已处理」—— 供**启动时**调用（boot 里那句原本是 `lastDay = todayKey();`）。
 *
 * 为什么要有这个函数：`lastDay` 搬进本模块后，app.js 那边再直接赋值就是
 * `TypeError: Assignment to constant variable`（import 绑定只读）。
 * 这也是「整体赋值的变量搬不走」那条硬约束的标准解法 —— **给外部配一个语义化的写入口**，
 * 而不是把裸变量交出去。 */
export function markToday() {
  lastDay = todayKey();
}
