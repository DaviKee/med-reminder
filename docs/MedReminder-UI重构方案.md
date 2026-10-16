# MedReminder UI 重构方案（v1.1，已拍板）

> 2026-10-16 拟定并拍板。用户决策：**先架构后皮肤**，视觉按指定参考重设计。
> 本文是**实施方案**：架构项分解 + 设计令牌 + 动效参数 + spec 碰撞登记 + 分阶段计划。
> 拍板后按 §8 阶段逐期实施，每期一提交、harness 全绿才进下一期。

---

## 1. 参考解读（7 个链接 → 本项目映射）

| 参考 | 提取到的实质 | 落到本项目 |
|---|---|---|
| 鸿蒙引力动效规范（CSDN） | 时长分层：颜色 100ms / 小范围 150ms / 局部 200ms / 全屏 350ms；曲线三分：标准 `cubic-bezier(.4,0,.2,1)`（视线内）、减速 `cubic-bezier(0,0,.4,1)`（入场）、加速（出场）；**卡片左右滑扩展功能 + 显控比阻尼**；一镜到底（共享容器补间） | 动效令牌体系（§3.2）；行滑动的跟手+阻尼+限位参数；FAB→sheet 用共享容器补间思路 |
| 微动效 10 例（cnblogs） | 按压位移 ≤3px、100–150ms；列表项删除 = 渐隐+左滑+图标；克制、统一、自然 | 按压反馈统一 120ms；删除动效节奏；全项目动效语言统一 |
| DailyUI Calendar（Dribbble） | 深色扁平、大字时间+任务行+右侧圆形完成键、彩色 tag、留白节奏、暗色渐变让元素跳出 | 深色底 + 大字号时间 + 卡片列表的信息层级；时间=最重元素 |
| Pastel UI Wellness（Dribbble） | 圆角几何、粉彩渐变、少动效、"silence as a feature" | 只取**圆角几何与呼吸感**，不取粉彩（与用户指定 #121212 高对比冲突） |
| Liquid FAB（Dribbble） | 单击 → 纵向液态展开多动作；粗圆形、强对比 | FAB 形态（圆角矩形）+ 液态展开过渡（§3.3） |
| Google Clock M3 改版（Beebom） | squircle FAB 移右下、拉长的时间选择器、更粗字重、圆角矩形化 | FAB 位置与形状；时间数字加粗；控件去圆统一为圆角矩形/药丸 |
| 小时光（百科） | 提醒类竞品：扁平化、仪式感背景页、桌面小组件 | 不抄功能；确认"扁平 + 沉浸"是本品类共识基调 |

**用户指令（最高优先级，覆盖参考）**：
1. 视觉：大字号时间（≥42sp）、卡片式列表、高对比（深色底 `#121212` + 荧光/低饱和主色）、控件统一圆角矩形或药丸、**去拟物装饰**（无渐变堆叠、无阴影、无仿实物）
2. 动效只做三处英雄时刻：**响铃页日出渐变 / 列表项左滑删除·右滑编辑 / 添加 FAB 液态展开**；其余反馈 ≤150ms
3. 防误触：响铃页「稍后提醒」大且居中，「停止（已服用）」远且偏小——半梦半醒时拇指落点是安全动作

---

## 2. 架构重构项（先做，与视觉无关，每项可独立验证）

### A1. CSS 四文件拆分（488 行 → 4 个聚焦文件）

| 新文件 | 内容 | 行数预估 |
|---|---|---|
| `css/tokens.css` | **全部**设计令牌：颜色（:root + 亮色块）、圆角、间距、字号、动效时长/曲线、字体栈、color-scheme | ~120 |
| `css/base.css` | reset、body/.phone/main/.view/.stack、排版类（.h1/.body/.meta/.eyebrow/.num/.hint）、a11y（焦点环、reduced-motion） | ~110 |
| `css/components.css` | 跨页组件：card/pill/btn/chip/input-wrap/stepper/badge/tabbar/sheet/dlg/toast/scrim/menu-row | ~150 |
| `css/views.css` | 页级：dose 行、meditem/week dot、cal（月历）、rpt（报告）、**新增** FAB / swipe-row / sunrise | ~160 |

- `index.html` 按 tokens → base → components → views 顺序 4 个 `<link>`（SW 预缓存靠解析 index.html 链接，自动生效）
- `tests/sources.js` 增加 `cssAll()`（按序拼接 4 文件）；**6 个 spec 的读 CSS 一行改成 `cssAll()`**（layout / fixed / history / med-fields / tabs / theme）
- `sw.js` 预缓存清单同步替换；`sw.spec` 断言同步更新
- ⚠️ 顺序即依赖：tokens 必须第一（其余文件只消费变量）

### A2. 设计令牌成为唯一真源

- 圆角 `--r-pill:9999px / --r-card:20px / --r-ctl:14px / --r-fab:20px`
- 间距 `--sp-1:8 / --sp-2:12 / --sp-3:16 / --sp-4:20 / --sp-5:28`（px 值）
- 字号 `--t-hero:44 / --t-title:22 / --t-body:15 / --t-meta:13 / --t-micro:12`（全部 `calc(Npx * var(--fs))` 消费）
- 动效 `--dur-*` / `--ease-*`（§3.2）
- 规则：**新 CSS 禁止裸颜色/裸 px 圆角**（theme.spec §6 已有裸颜色禁令，扩大到圆角度）

### A3. 歼灭内联样式（90 处 → ~0）

现状 `grep 'style="' www/js` 命中 90 处，全是**静态样式写在了 JS 字符串里**——每次调视觉要改 JS，是本次重构最大的维护性债务。处理：

- 页面头部（today/meds/records 三处 `card-row`）→ `.page-head` 类
- 下次服药时间（today.js 内联 40px）→ `.time-hero` 类（同时完成 ≥42sp）
- 各类 `margin-top:-4px` 微调 hint → `.hint-tight` 等语义类
- `meds.js:68` `style="color:#DADBDF"` —— **顺手修的 bug**：硬编码浅灰，亮色主题下徽章里几乎看不见（theme.spec 只查 stroke/fill，漏网的 color）→ 改 `var(--sub)`
- 保留例外：无（这批内联没有动态值）

### A4. 新模块（沿用项目「纯函数判定 + DOM 调用」传统）

| 模块 | 职责 | 配套 spec |
|---|---|---|
| `ui/gesture.js` | 行滑动手势：`swipeRowIntent(dx,dy)` 纯函数（横向意图锁、阈值、限位）+ `#main` 上 passive 监听（**复刻 bindSwipe 模式**） | 新增 `tests/swipe-row.spec.js`（矩阵式，仿 swipe.spec） |
| `ui/fab.js` | FAB 开合：读写 FAB/sheet 矩形、加过渡类、焦点管理 | 并入 wizard/meds 既有 spec 扩展 |
| `ui/cards.js` 扩充 | `swipeRowShell(med)`（行+底层动作层）、`fabHtml()` | 跟随 cards 既有用法 |
| `ui/overlay.js` 小改 | openDlg/closeDlg 给响铃浮层加/摘 `.sunrise` 类（重播日出） | remind.spec 不动（只加 class，不改函数体逻辑） |

### A5. 不动的东西（明确边界）

- `www/js/core/`、`www/js/platform/`：零触碰
- 向导 5 步结构、`data-med` 事件委托、`dlgRemind` 的 id 与 `remindDone` 处理体：零触碰（back.spec / remind.spec / wizard.spec 盯着）
- `--fs` 字号机制：保留（109 处消费），只扩刻度
- tab 三个页签与 `.tab-ind` 位移公式：保留（tabs.spec 盯着除数=3）

---

## 3. 视觉与动效设计（皮肤）

### 3.1 设计令牌（深色为基准——用户指定；亮色对称适配，判定逻辑不动）

**表面与文字**（均在 `#121212` 系上校过 WCAG AA）：

| 令牌 | 深色 | 亮色 | 说明 |
|---|---|---|---|
| `--bg` | `#121212` | `#F6F4F0` | 页面底（用户指定深色值） |
| `--bg-outer` | `#0A0A0A` | `#E9E6E1` | 机身外缘 |
| `--card` | `#1B1B1E` | `#FFFFFF` | 卡片 |
| `--inner` | `#24242A` | `#EEEBE6` | 输入/内嵌块 |
| `--line` / `--line-2` | `#2C2C33` / `#3D3D46` | `#E2DED8` / `#C9C4BC` | 描边 |
| `--text` / `--sub` / `--muted` | `#F5F5F7` / `#C7C7CE` / `#94949E` | `#17171C` / `#4A4A52` / `#6E6E77` | muted 两套都 ≥4.5:1 |
| `--accent` / `--on-accent` | 见 §8 决策1 / `#121212` | 同左加深 / `#FFFFFF` | 主行动色 |
| `--contrast-bg` / `--contrast-text` | `#F5F5F7` / `#17171C` | `#17171C` / `#FFFFFF` | 反色块（theme.spec 盯翻转） |
| `--danger` | `#FF6B5E` | `#C62F24` | 破坏性 |
| `--ok` | `#2FA96B` | `#1E7A43` | 月历全服格（值随皮肤调，语义不变） |

**主色方向（待拍板，§8 决策1）**：
- 方向 A（推荐）：低饱和暖橙琥珀 `#F2A93B` —— 延续现有 `--accent`=「行动/提醒」语义（漏服角标、待服用 pill、报告漏出行全部依赖它，不用重映射），与日出渐变同色系，老人高可视；饱和度低于现 `#FF7A17`、亮度提高，深底上呈"低饱和荧光"感
- 方向 B：荧光青绿 `#5EEAD4` —— 健康/清凉感最强，但要把「漏服=橙」语义整体改映射（rpt-chip.bad / .btn-miss / .dose-miss / cal partial 四处语义色而动），且与日出暖色舞台打架

### 3.2 动效令牌（鸿蒙规范分层 × 用户 150ms 约束）

| 令牌 | 值 | 用途 |
|---|---|---|
| `--dur-tap` | 120ms / `--ease-std` | 按压、chip 选中、开关 |
| `--dur-fade` | 150ms / `--ease-std` | toast、scrim、ghost 反馈（用户红线） |
| `--dur-swipe` | 180ms / `--ease-std` | 行滑动吸附/回弹（规范：局部 ≤200ms） |
| `--dur-sheet` | 280ms / `cubic-bezier(.32,.72,0,1)` | sheet 出入场（现值，不动） |
| `--dur-fab` | 320ms / `--ease-liquid` | **FAB 液态展开**（英雄时刻） |
| `--dur-sunrise` | 900ms / `--ease-out` | **响铃页日出**（英雄时刻，仅一次） |
| `--ease-std` | `cubic-bezier(.4,0,.2,1)` | 视线内物体（鸿蒙标准曲线） |
| `--ease-out` | `cubic-bezier(0,0,.4,1)` | 入场（减速） |
| `--ease-in` | `cubic-bezier(.4,0,1,1)` | 出场（加速） |
| `--ease-liquid` | `cubic-bezier(.34,1.35,.5,1)` | 液态微超弹 |

全项目动效已被 `prefers-reduced-motion` 通杀（tabs.spec 盯着）——新动效自动继承，不另写。
**P5 有一项专项审计**：现存所有 transition/animation 逐条对照，除三个英雄时刻 + sheet 出入场，一律 ≤150ms。

### 3.3 三个英雄时刻（落地参数）

**① 响铃页日出渐变**（`#dlgRemind` 打开时）
- 新增 `.sunrise` 装饰层：打开时 openDlg 加类（触发一次）、关闭摘类（可重播）
- 关键帧：深夜 `#0B1026→#1A1030`（顶部）→ 地平线暖光 `--accent` 微光自下 1/3 渐起，900ms `--ease-out`，停留在"将亮"状态（不循环）
- 对话框本体保持实底（文字对比度不受动画影响）；渐变只做**氛围底**，替换现扁平 scrim
- reduced-motion：直接落终态

**② 列表项左滑删除 / 右滑编辑**（药品页 `.meditem`）
- 跟手： passive 监听，横向意图锁（`|dy|≥|dx|` 让滚动赢，与 tab 滑同一判据）；过 96px 后显控比阻尼（鸿蒙卡片左右滑指引）
- 阈值 64px：左滑露出「删除」（danger 红底白字，在行底右侧），右滑露出「编辑」（accent）
- 回弹/吸附 180ms；松手带速继承（超阈值即使位移不足也吸附）
- **删除不直删**：露出的是按钮，点它 → 走既有 `askConfirm` 确认框 → 才删（破坏性操作必须确认，项目既定纪律；老人手滑不该丢数据）
- 「编辑」= `openSheet(medId)`，与点按同行一致（双入口，不矛盾）
- 点按判定：总位移 <8px 且 <500ms 才认 click（行仍是 button，原生 click 不被拦）
- ⚠️ **不许用 `touch-action`**（swipe.spec 全项目禁令，会废双指缩放）——意图锁方案与 tab 滑动同源，已在真机验证过
- 守卫：浮层开着不滑、单指、落在 dlg/sheet/input 上不滑（复刻 bindSwipe 守卫）

**③ FAB 液态展开**（药品页）
- 药品页右下 FAB：60×60、`--r-fab:20px` 圆角矩形（squircle 感）、accent 实底 + `+` 图标；位于 tabbar 上方 16px、右侧 20px；仅药品页可见（`.view.active` 联动显隐）；页头 `#btnAdd` 图标钮**移除**（FAB 取代）
- 液态展开 = 共享容器补间：点 FAB → FAB 矩形与 sheet 矩形之间做 transform+圆角+尺寸补间 320ms（`--ease-liquid`），sheet 内容延迟 120ms 淡入；关闭反向 240ms
- 不用 View Transitions API（老 WebView / ArkWeb 兼容性），纯 transform+opacity，降级路径 = 直接显示
- 今日页空态的「添加第一个药品」按钮保留原位（它是引导 CTA，不是常驻入口）

### 3.4 防误触改版（响铃页按钮层级）

```
现状（等大同距，竖排两枚一样大）：
[ 已服用  ] ← 实心主色
[ 10分钟后提醒 ] ← ghost

改版（不等大 + 不等距 + 安全失败）：
        （图标 + 药名 + 时间 meta，居中）
[      稍后提醒      ]  ← 全宽 60px、accent 实心、药丸、居中、独占一行
   ↑ 间距 28px ↑
[    已服用    ]        ← 宽 56%（max 220px）、44px、ghost、居中
```

- 「稍后提醒」= 半梦半醒拇指的自然落点 → **误触的后果是 10 分钟后再响**（零伤害，漏服在今日页可见可补记）
- 「已服用」= 误触即静默打卡 → 必须做得远且小；但触摸目标仍守 44px 底线（a11y）
- 两按钮宽度 100% vs 56%、高度 60 vs 44、上下间距 28/20 都不等——从根上消除「等大同距」
- `remindDone` / `remindSnooze` 两个 id 与 onclick 处理体**不动**（remind.spec 盯着 cancelOne），只改 HTML 结构与 class

### 3.5 字号体系（≥42sp 落地）

| 场景 | 现 | 新 |
|---|---|---|
| 下次服药时间（今日页 hero） | 内联 40px | `.time-hero` = `calc(44px * var(--fs))` ✅≥42sp |
| 剂量行时间 `.dose-t` | 15px | 20px 半粗 tabular-nums（行宽 52→64px） |
| h1 / h2 | 28 / 20 | 28→24 收紧（hero 已上位）/ 20 不变 |
| body / meta / micro | 14 / 13 / 12 | 15 / 13 / 12（正文上调，适老） |
| 按钮文字 | 15 | 15（主 CTA 16 半粗） |

### 3.6 形态统一

- 按钮：全部药丸（primary/ghost/accent/text 统一 `--r-pill`）；主 CTA 高 56px
- 卡片：`--r-card:20px`（现 8px）、1px 描边、扁平无底无影（去拟物）
- 输入/chip：chip 药丸；input-wrap `--r-ctl:14px`
- tab 指示胶囊：`--contrast-bg` → 随皮肤改令牌值即可，结构公式不动
- 今日剂量列表 → **每行一张卡**（`.dose` 升为独立卡片，间距 8px，卡片式列表）；`.dose-n` 省略号截断与 `.dose-l` 收缩规则保留（layout.spec 盯着）

---

## 4. Spec 碰撞登记册（精确到断言）

| spec | 断言 | 碰撞? | 策略 |
|---|---|---|---|
| theme.spec §5 | :root 颜色变量亮色块必须逐个重定义、不多不少 | ⚠️ 新增令牌必须亮色对称 | 令牌表成对写，提交前跑一遍即知 |
| theme.spec §6 | CSS 除变量定义外**无裸颜色**；JS/HTML 无硬编码 stroke/fill | ⚠️ 日出渐变的色值必须写成令牌或定义在令牌块 | 渐变色走 `--sunrise-*` 令牌（成对） |
| theme.spec §6b | 扫全部 www/js 的 stroke/fill 硬编码 | ✅ 不新增即可 | 新 SVG 一律 `currentColor`/`var()` |
| theme.spec §7 | 内联主题脚本在 css link 之前、key 一致 | ✅ HTML 头部只加 link 不动脚本 | 4 个 link 加在原 link 位置 |
| tabs.spec §3/§5 | .tab-ind 公式、除数=3、pointer-events、.view 不 absolute、reduced-motion 通杀 | ✅ 只改令牌值不改结构 | — |
| layout.spec | main `overflow-wrap:anywhere`；`.dose-n` nowrap+ellipsis；`.dose-l` min-width:0 | ✅ 保留这三条 | 剂量行改卡片时类名与这三条不动 |
| **swipe.spec** | **CSS/JS 全域禁 `touch-action`**；passive；单指；浮层守卫 | 🔴 **行滑动最大红线** | 不用 touch-action，意图锁复刻 bindSwipe； gesture.js 自带守卫+新 spec |
| fixed.spec | `.time-input` height:44px、`.time-row` 存在 | ✅ 类名保留 | — |
| history.spec | `.dlg-scroll` max-height + overflow-y:auto | ✅ 保留 | — |
| med-fields.spec | `.input-wrap.multi` height:auto/padding:0、textarea 跟 --fs | ✅ 保留 | — |
| back.spec | `[data-med]` 事件委托在；`$$('.meditem').forEach` 不许回来；dlgRemind 不在返回键可关清单 | ⚠️ swipe 动作按钮必须并入同一委托（`data-med-del`），不另绑 forEach | 委托里加 branch |
| remind.spec | remindDone onclick 体含 cancelOne 等；showReminder 护栏 | ✅ 只改按钮 HTML 结构与 class | — |
| sw.spec | 预缓存含 css 文件 | ⚠️ 拆文件后同步 sw.js + 断言 | A1 已列 |
| wizard.spec | 向导 5 步 | ✅ 不动向导 | — |

**结论：无阻塞性碰撞。** 唯一需要"绕"的是 touch-action 禁令（已有同源成熟方案），其余是机械同步。

---

## 5. 真机/OCR 验证预算（硬约束：每会话 3 次）

| 阶段 | 验证内容 | 占用 |
|---|---|---|
| P1/P2（架构期） | 视觉零变化——harness + 源码级断言即可，**不占** OCR 预算 | 0 |
| P3（皮肤） | 深色三页 + 亮色一页 OCR | 1 |
| P4（英雄动效） | 日出 + FAB + 行滑动 OCR | 1 |
| P5（防误触） | 响铃页按钮 OCR | 1 |
| 余量 | 返修缓冲 | 0（刚好用满，超支就拆会话） |

---

## 6. 提交策略

- 分支：**继续当前 `harmony` 分支**（近 6 个 UI 提交都在这条线，fec719b 即最新 UI 修复）；只 stage `www/` 与 `tests/` 与 `docs/` 与 `sw.js`——**`openharmony/` 未提交改动是秦老师的线，绝不碰**
- 每阶段一提交，版本号递增（现 v1.5.30 → P3 起 v1.5.31+）
- 提交前：harness 26/29 关口全绿 + `$env:PYTHONUTF8="1"`（pre-commit GBK 坑）
- 合并回 `main`（Android 线）= 全部阶段完成后的显式步骤，不在本方案内

---

## 7. 工作量与顺序

| 阶段 | 内容 | 估时 | 出口条件 |
|---|---|---|---|
| P0 | 本方案拍板（§8 四项决策） | — | 用户确认 |
| P1 | CSS 四拆分 + sources.js + 6 spec 同步 + sw.js | 中 | harness 全绿 + 视觉零变化 |
| P2 | 90 处内联样式歼灭 + meds.js 硬编码色修复 | 中 | harness 全绿 + 视觉零变化 |
| P3 | 令牌重设计（深色 #121212 + 主色）+ 圆角/字号/卡片化 | 大 | harness + OCR×1 |
| P4 | 三个英雄动效（sunrise/swipe/FAB）+ gesture.js + 新 spec | 大 | harness + OCR×1 |
| P5 | 防误触改版 + 150ms 动效审计 | 小 | harness + OCR×1 |

P1+P2 视觉零变化，可合并为一次提交（架构期不分家）。

---

## 8. 待拍板决策（阻塞 P0）

1. **主色方向**：A 低饱和暖橙琥珀（延续 accent 语义，配日出）/ B 荧光青绿（需重映射漏服语义）/ C 你直接给色值
2. **CSS 拆分幅度**：A 四文件全拆 + 6 spec 机械同步（架构收益最大）/ B 保持单 app.css 内部分区（零测试改动）
3. **滑动映射**：A 左滑露「删除」（走确认框）+ 点按=编辑（老人最安全）/ B 左滑删除 + 右滑编辑双露出
4. **FAB 范围**：A 仅药品页 / B 药品页 + 今日页空态双入口

---

## 9. 拍板记录（2026-10-16）

| # | 决策 | 结论 |
|---|---|---|
| 1 | 主色 | **A 低饱和暖橙琥珀 `#F2A93B`**（深色）/ 亮色加深版 `#B45309` 系。延续 accent=行动/提醒语义，漏服四处语义色不动 |
| 2 | CSS 拆分 | **A 四文件全拆**（tokens/base/components/views）+ sources.js `cssAll()` + 6 spec 同步 + sw.js 同步 |
| 3 | 滑动映射 | **A 左滑露「删除」（danger 底，点击走 askConfirm 确认框才真删）+ 点按=编辑（openSheet）**。不做右滑编辑（与点按重复） |
| 4 | FAB 范围 | **A 仅药品页**（右下、tabbar 上方 16px、右 20px、60×60、`--r-fab:20px`）；今日页空态引导大按钮保留 |

**基线锁定**：本版之后 §2/§3 的参数即实施标准；实施中发现需要偏离的，先在本文档记录原因再改代码。
