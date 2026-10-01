# MedReminder-数据层迁移预研 —— localStorage → 原生进程可见

> 调研日期：2026-09-30 ｜ 执行：davik
> 状态：**调研完结，方案已定推荐项**，待排期确认后进 spike 实测
> 前置阅读：《MedReminder-Agent接入调研.md》§5.1（前置①，本文即其展开）、
> 《MedReminder-OpenHarmony7-特性研究.md》§3.2（卡片 FormKit，数据共享问题的提出处）
> 证据分级：🟢 本地 SDK 实测 / 🔵 华为官方文档 / 🟡 二手资料（采纳需谨慎）

## 0. 一句话结论

**推荐「影子状态文件」方案**：web 层在 `save()` 之后，经**已有的 filesystem 插件**把主状态
JSON 原子写入 `Directory.Data`（鸿蒙侧 = 应用沙箱 `filesDir`），卡片 / Agent 进程在共享沙箱里
用 fileIo 直接读。**不用新插件、不改依赖方向、无平台分支、多进程天然安全（单写多读）**。
原三方案里的 preferences 路线被 SDK 官方多进程警告否决；复用备份文件的「零成本方案」在鸿蒙上
不成立（备份落点在沙箱外）。

## 1. 问题重述（为什么需要迁移）

S-9 服务卡片与 Agent 一期都要求：**原生进程**（FormExtensionAbility / AgentExtensionAbility
各自独立进程）能读到 web 层（Capacitor WebView）里的用药状态，才能展示「下一次吃药」或回答
「现在该吃什么药」。

而现状是：全部状态锁在 WebView 的 localStorage 里 —— **原生进程不可见**。

所以问题不是「换一个存储」，而是「让原生进程读到一份状态副本」。这个定位决定了下面所有
方案的取舍：**读路径的可达性 > 写路径的优雅性**。

## 2. 现状盘点（🟢 代码实测）

### 2.1 localStorage 全部键面

| 键 | 内容 | 大小量级 | 卡片/Agent 需要？ |
|---|---|---|---|
| `medreminder.v1` | **主状态**：`{meds, doses, notified}` 整存单键 | KB 级 | ✅ 核心 |
| `medreminder.fs` / `medreminder.fsHint` | 字号 + 可访问性提示 | <100 B | ✅ 卡片要（字号） |
| `medreminder.pending`（×2） | 拍照留痕 / 通知 pending 台账 | KB 级 | ❌ |
| `medreminder.idmap` | 通知 id ↔ dose 映射 | KB 级 | ❌ |
| `medreminder.backupStatus` | 备份状态 | <100 B | ❌ |
| `medreminder.photoHash` | 照片去重哈希 | KB 级 | ❌ |

代码位置：`www/js/core/store.js`（主键与 save()/saveHooks）、`www/js/backup.js`（备份状态）等。

→ **原生侧需要的只是「主状态 + 字号」，不是全部键**。数据量 KB 级，任何方案都没有容量压力。

### 2.2 save() 与 saveHooks（现成的扩展点）

- `store.save()` 是**唯一落盘出口**；上行编排在 `saveHooks`（现有 `refresh` / `notify` /
  `backup` 三个钩子）注入式完成 —— 新增一个 `shadow` 钩子即可挂入，**不动依赖方向**
  （util ← store ← schedule ← app，影子模块与 backup 同层）。
- backup 模块的工程纪律可直接复用：防抖（3s）、串行化（busy/dirty/pending）、
  flush（切后台立即写）、失败不影响打卡（全程 catch）。

### 2.3 现有备份管道 ≠ 现成的读路径（关键发现 ⚠️）

backup.js 写 `latest.json`（常驻最新）+ `backup-YYYYMMDD.json`（日轮转 ×14），目录链
`DOCUMENTS → EXTERNAL → DATA` 依次尝试。**鸿蒙上 DOCUMENTS 会优先命中**（spike 实测落
用户文档目录，用户可见、卸载不删）—— 也就是说：

**备份文件在鸿蒙上落在应用沙箱之外，卡片/Agent 进程没有现成的合法读路径。**

「直接读备份文件」的零成本方案（Agent 调研 §5.1 曾列的方向之一）在鸿蒙上**不成立**。
且备份本身还有 3s 防抖滞后 + 失败静默 + 日轮转三个不适合做读源的性质。

## 3. 鸿蒙侧关键事实（证据）

| # | 事实 | 证据 | 级别 |
|---|---|---|---|
| 1 | **应用沙箱按应用（bundle）划分，不按进程**：同一应用的所有进程（UIAbility 主进程、FormExtensionAbility、AgentExtensionAbility）共享同一应用文件目录（`filesDir` 等） | 官方文档《应用沙箱目录》《应用文件概述》 | 🔵 |
| 2 | **preferences 明确警告多进程**：`@ohos.data.preferences` d.ts 原文「多进程场景…会导致文件损坏、数据丢失等未知问题，禁止多进程使用」 | 本地 SDK `@ohos.data.preferences.d.ts` 36-37 行 | 🟢 |
| 3 | `@capacitor-ohos/preferences` 8.0.1 存在，落点就是 `@ohos.data.preferences`（store 名 `CapacitorStorage`，落 preferencesDir）→ 继承事实 #2 的全部风险 | npm 下载解包源码实测 | 🟢 |
| 4 | RDB（relationalStore）基于 SQLite，普通应用沙箱内**多进程可访问**；仅「dataGroupId 沙箱 + 加密库」组合不支持多进程 | 本地 SDK `@ohos.data.relationalStore.d.ts` 283-329 行 | 🟢 |
| 5 | `@capacitor-ohos/filesystem` 8.0.2 API 面齐全：writeFile / readFile / **rename** / copy / delete / stat / readdir / getUri —— **原子替换（写临时文件 + rename）可做** | npm 下载解包源码实测（OHRename 存在） | 🟢 |
| 6 | `Directory.Data` 在鸿蒙侧映射为应用沙箱 `filesDir`；Android 侧同样是应用私有目录 | 《MedReminder-鸿蒙移植调研.md》§2.2 实测 | 🟢 |
| 7 | preferences 单键值上限 16MB —— 容量对我们无压力，**卡脖子的从来不是容量而是多进程** | 本地 SDK d.ts | 🟢 |

## 4. 方案对比

| 维度 | A. 复用备份文件 | **B. 影子状态文件（推荐）** | C. preferences 插件 | D. RDB |
|---|---|---|---|---|
| 思路 | 卡片/Agent 读 backup 的 latest.json | save() 后另写一份专用 JSON 到 `Directory.Data` | 换 `@capacitor-ohos/preferences` 存主状态 | 主状态进 SQLite |
| 原生进程可读 | ❌ 鸿蒙落点在沙箱外（§2.3） | ✅ 共享沙箱 filesDir | ⚠️ 技术上同沙箱可达，但见下行 | ✅ 多进程安全 |
| 多进程安全 | —（读不到，无须比） | ✅ **单写多读** + 原子替换，无损坏风险 | ❌ **官方明文禁止**（§3-2），卡片+主进程+Agent 三进程同开是禁区 | ✅ SQLite 并发成熟 |
| web 层改造 | 0 | **~50 行**（一个新钩子/模块） | 大（主状态读写全换插件，8+ 处调用面） | 最大（SQL 化改造） |
| 平台分支 | — | **无**（filesystem 插件双端通用；Android 写 DATA 也是沙箱，无害） | 无 | 无 |
| 依赖方向 | — | ✅ 不动（新钩子与 backup 同层） | ✅ | ✅ |
| 新依赖 | 0 | **0**（复用已有 filesystem 插件） | +1 插件 | +1 插件（或自研） |
| 二期（Agent 写打卡）演进 | — | ✅ 平滑：保持主进程唯一写者（Agent 打卡走「拉起主进程执行」或收件箱文件），见 §6.3 | ❌ 多进程死路 | ✅ 天然多写者 |
| 审核证明材料 | — | 沙箱内私有文件，无权限诉求 | 无权限诉求 | 无权限诉求 |
| 结论 | **否决**（鸿蒙上根本读不到） | **✅ 推荐** | **否决**（官方多进程警告） | **备选**（过重；二期若出现真·多写者再上） |

> C 的否决值得强调：它是「看起来最正统」的方案（官方 Capacitor 插件、落官方存储 API），
> 但 🟢 d.ts 白纸黑字的多进程警告让它在我们的场景里是**架构级死路** —— 卡片进程是必然存在的
> 第二进程。这也提醒：插件「存在」≠「能用」，落点的限制要查到底层 API。

## 5. 推荐方案细化（B：影子状态文件）

### 5.1 目录与命名

- 路径：`Directory.Data` + `shadow/state.json`（**与备份的 `MedReminder/` 分开**：
  备份要用户可见性，影子要进程可达性，语义不同不混放）。
- 内容：主状态 JSON 原样 + 头部元数据（`{schema: 'medreminder.shadow.v1', savedAt, state}`），
  读方可校验版本与新鲜度。
- 字号：随影子文件一起写（KB 级，不值得单独一个文件）。

### 5.2 写入协议（web 侧）

```
save() → saveHooks.shadow（新增第四钩子）
       → 防抖（可与 backup 共用调度，窗口内多次 save 合并）
       → writeFile(state.json.tmp) → rename(tmp → state.json)   ← 原子替换
       → 失败全程 catch，不影响打卡（沿用 backup 纪律）
```

- **原子替换是本方案的安全核心**：读方任何时候看到的要么是旧完整文件、要么是新完整文件，
  不存在半截 JSON。
- rename 实测存在于 ohos filesystem 8.0.2（§3-5）；若个别路径异常，回退顺序
  delete + write（读方容忍瞬时 ENOENT，用兜底文案即可）。
- 切后台 flush：与 backup 同机同步触发（两个写盘动作共用一个 flush 时机）。
- 实现位置建议：独立小模块 `core/shadow.js`（与 backup.js 姊妹篇），而不是塞进 backup 的
  doWrite —— 备份的 DIRS 回退链、状态上报逻辑都不该被影子文件污染；但防抖/串行化模式照抄。

### 5.3 读取协议（卡片 / Agent 侧，ArkTS）

```
fileIo 读 filesDir/shadow/state.json
→ 校验 schema 字段 → JSON.parse → 校验 {meds, doses, notified} 形状
→ 失败（不存在/损坏/版本不符）→ 渲染兜底文案（"打开 App 查看用药计划"），不崩溃
```

- 卡片进程生命周期短（刷新窗口秒级）：读几 KB JSON 是微秒级操作，无性能顾虑。
- 解析结果按「读时快照」用，不做跨进程实时同步 —— 卡片每次刷新重读，Agent 每次会话重读。

### 5.4 一致性语义（向用户/验收交代）

- 原生侧看到的是「**最近一次成功写盘的完整状态**」，滞后 ≤ 一个防抖窗口（3s 级）。
- 对「下一次吃药时间」的展示/播报，3s 滞后无感；打卡后立刻看卡片可能滞后一拍
  —— 验收时按此口径，不算缺陷。

### 5.5 成本估计

| 项 | 量级 |
|---|---|
| web 层（core/shadow.js + saveHooks 注册 + 测试） | ~50-80 行 |
| 卡片读侧（ArkTS） | ~30 行 |
| Agent 读侧（ArkTS，一期只读技能） | ~30 行 |
| 新插件 / 新权限 / 平台分支 | **0** |

前置依赖一项：主工程 Capacitor 6 → 8 升级（ohos filesystem 8.0.2 对应 Capacitor 8 线）——
本就在移植工程搭建的必经路径上，无额外成本。

## 6. 演进与边界

### 6.1 一期（S-9 卡片 + Agent 只读技能）
单写多读，本方案一步到位。

### 6.2 卡片点击跳转
卡片点击拉起主应用（标准 FormKit 能力）→ 编辑仍在 web 层完成 → 数据流保持单向。
不需要卡片侧写。

### 6.3 二期（Agent 语音写打卡）
**保持主进程唯一写者**是本方案的演进底线，两条路（与 Agent 调研 §5.2 分期建议衔接）：
1. **拉起执行**（推荐先试）：Agent 进程把打卡意图发给主进程执行（AgentExtensionAbility
   拉起 UIAbility / 发事件），写盘仍走 save() → shadow 全链路，零新增协议；
2. **收件箱文件**：Agent 写 `shadow/inbox.json`（自身也是原子替换），主进程启动/回前台时
   消费。引入了「第二写者」但写不同文件，仍无同文件竞争。

若二期需求进一步复杂化（多写者、事务、增量查询），届时升级 RDB（方案 D）—— 本方案的
state.json 结构与 RDB 行结构可平滑映射，不是一次性赌注。

### 6.4 不做的事
- ❌ 不迁移 localStorage 本体（web 层继续用 localStorage，影子只是副本 —— 双写但单一真相源在 web 层）；
- ❌ 不给卡片/Agent 申请任何存储权限（沙箱内文件，零权限）；
- ❌ 不在 web 层写任何平台判断。

## 7. 待实测清单（spike，模拟器可做，不依赖 AGC 审批）

| # | 验证点 | 方法 | 级别 |
|---|---|---|---|
| 1 | **沙箱共享假设**：ExtensionAbility 进程能读到 web 层经 filesystem 插件写入的 `Directory.Data` 文件 | spike 工程加一个测试用 ExtensionAbility（或直接用卡片，模拟器支持卡片），web 侧写、原生侧读，比对内容 | 🟢 待测（假设依据是 🔵 官方文档） |
| 2 | **rename 原子替换**在 ohos filesystem 插件上工作正常 | web 侧连续 tmp+rename 100 次，原生侧并发读，断言无损坏 | 🟢 待测 |
| 3 | 写入滞后实测：save() → 影子文件更新延迟 | 时间戳比对 | 🟢 待测（预期 <100ms + 防抖窗口） |
| 4 | `Directory.Data` 落点复核（getUri 实际路径） | 移植调研 §2.2 已有结论，spike 复测一遍防版本漂移 | 🟢 待测 |
| 5 | 卡片刷新全链路：web 改数据 → 影子更新 → 卡片 onFormUpdate 读到新值 | 端到端 | 🟢 待测 |

> #1 是本方案唯一的核心假设，其余都是工程细节。官方文档明确沙箱按应用划分（§3-1），
> 风险低，但按纪律必须 🟢 实测落锤。

## 8. 对排期的影响

- **⑤「原生化外放」（卡片 + Agent 一期）的前置工作就此明确**：影子状态文件是第一步，
  卡片与 Agent 共用同一份读侧代码。
- 与既有队列的接口：② 数据层预研（本文）→ ③ TTS（无依赖，可并行）→ ⑤ 卡片+Agent 一期
  （消费本文方案）。spike 实测清单（§7）建议挂进 ⑤ 的开工条件。
- Capacitor 6→8 升级（移植工程搭建的一部分）是唯一硬前置。

## 9. 来源与可靠性

| 来源 | 用途 | 级别 |
|---|---|---|
| 本地 SDK `@ohos.data.preferences.d.ts`（36-37 行多进程警告、16MB 上限） | 方案 C 否决依据 | 🟢 |
| 本地 SDK `@ohos.data.relationalStore.d.ts`（283-329 行 StoreConfig/dataGroupId 说明） | 方案 D 定位 | 🟢 |
| `@capacitor-ohos/preferences` 8.0.1 源码（npm 下载解包） | 落点实证 | 🟢 |
| `@capacitor-ohos/filesystem` 8.0.2 源码（npm 下载解包，OHRename 等 13 个导出函数） | 方案 B 可行性 | 🟢 |
| 华为官方文档《应用沙箱目录》《应用文件概述》（devecocli docs） | 沙箱按应用划分 | 🔵 |
| `www/js/core/store.js`、`www/js/backup.js` 源码 | 现状盘点 | 🟢 |
| 《MedReminder-鸿蒙移植调研.md》§2.2 | Directory.Data → filesDir 映射 | 🟢（spike 实测） |
| 《MedReminder-OpenHarmony7-特性研究.md》§2.5、§3.2 | 卡片场景与约束 | 🟢/🔵 混合 |

**诚实边界**（未实测前不要当成定论的部分）：
- §7-1 沙箱共享假设是 🔵 级推理，待 spike 落锤；
- rename 的原子性语义在 ohos 插件封装层的实际行为（是否真正不撕裂）未测；
- AgentExtensionAbility 进程读文件这一步模拟器验不了（Agent Kit 不支持模拟器），
  需真机，但读侧代码与卡片同构，卡片实证可大幅降低风险。
