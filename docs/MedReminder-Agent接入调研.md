# MedReminder-Agent 接入调研（HarmonyOS 7 智能体能力）

> 调研日期：**2026-09-29** ｜ 姊妹篇：《MedReminder-OpenHarmony7-特性研究.md》（下称「OH7 研究文档」）
> 研究对象：鸿蒙 7 的**智能体（Agent）能力** —— ArkAF（方舟智能开发框架）/ Agent Framework Kit / 小艺开放平台
> 目的：回答三个问题 —— **① 这套 Agent 能力是什么 ② 官方怎么用（有没有样本）③ 和我们结合以后会有什么结果**
>
> 证据分级（沿用 OH7 研究文档的纪律，勿混用）：
> - 🟢 **本地 SDK 实测** —— 直接读 `D:\Huawei\DevEcoStudio\sdk\...` 的 `.d.ts`（本次在 davik 本机的
>   DevEco Studio 上验的；OH7 研究文档用的是 Qinn 机器的 `D:\Program Files\Huawei\DevEco Studio`，两边都是 API 26）
> - 🔵 **华为官方文档** —— 本地文档镜像（`devecocli docs`，内容与 developer.huawei.com 同源）
> - 🟡 **二手资料** —— 本文**未采纳**任何二手资料

---

## 0. 一句话结论

> **Agent 是鸿蒙 7 给我们的「第四份礼物」**：用户对小艺说「我今天还有什么药没吃？」，
> 小艺直接连接我们 App 里的**端侧智能体**查询排程并播报 —— **不用打开 App、不用看清屏幕**，
> 这是比 TTS 更进一步的适老化形态。
> 但它有**三个硬前置**：① 全部是原生 ArkTS，且**不能放 HAR 包**（Capacitor 插件恰是 HAR）；
> ② 数据必须从 WebView `localStorage` 迁到**原生可见的存储**（与 S-9 卡片同一个前置）；
> ③ **模拟器不支持**（Agent Framework Kit 官方明确），必须真机验证。
> **建议与 S-9 卡片合并为一个「原生化外放」阶段**，一期只做「只读查询」技能，二期再做「语音打卡」。

---

## 1. Agent 能力全景（是什么）

### 1.1 总纲：ArkAF（方舟智能开发框架）

用户交互正在从「打开应用」转向「直接表达需求」。应用把能力**外化**给**系统智能体（小艺）**，
由小艺解析自然语言 → 匹配最佳应用能力 → 调用执行 → 转成自然语言答复用户。
ArkAF 提供三条接入路径（🔵 官方《方舟智能开发框架概述》）：

| 路径 | 核心能力 | 触发方式 | 适用场景 | 起始版本 |
|---|---|---|---|---|
| **意图框架**（Intents Kit） | 功能注册为「意图」，分发到小艺对话/搜索/建议 | 系统智能体唤醒执行 | **单一明确的能力调用**（如播放音乐） | API 11+（装饰器 API 20+） |
| **Skill 框架** | 功能封装为 ArkTS 脚本「技能」上架 | 上架后被识别调用 | 复杂场景功能（如导航回家） | API 20+ |
| **端侧 A2A 框架** ⭐ | 应用内置**端侧智能体**，与小艺**双向会话协商** | 与系统智能体双向通信 | 应用智能体开发（**如跟踪股票信息**） | API 24+ |

工作流四阶段：**开发接入 → 系统注册 → 智能匹配 → 能力执行**。

### 1.2 端侧 A2A 框架（对我们最重要的一条路）

🔵 官方《端侧A2A框架概述》要点（以官方「旅行规划」例子说明机制）：

- **架构**：客户端-服务端。**小艺等系统应用是客户端（Agent 客户端）**，第三方应用是服务端。
  → 我们只能做**被调用方**，方向恰好是对的。
- **基本概念**：
  - **AgentCard（智能体卡片）**：Agent 的「名片」，描述名称、能力、技能列表、输入输出模式，
    配置在 `agent_config.json` 里，注册到系统的 **Agent 管理服务**（这就是「系统注册」阶段）。
  - **AgentSkill（智能体技能）**：Agent 可执行的具体功能，每个技能定义用途、标签、输入输出模式、
    使用示例（`examples` 字段会给大模型看），**支持多技能组合**。
- **运行机制**：小艺自动发现 → 连接我们的 `AgentExtensionAbility` → `onData()` 收请求 →
  `sendData()` 回结果 → 可选 `onAuth()` 双向安全认证 → 可选 `AgentUIExtensionAbility` 在小艺里渲染我们的 UI。
- **API 26（鸿蒙 7）新增**：`AgentAbilityExtension` 支持**标准 A2A 协议**通信 ——
  引入 **Task（任务）/ Message（消息）/ Part（部件）/ Artifact（产物）/ Context（上下文）/ TaskState（任务状态）**
  全套协议语义，支持长任务跟踪与多轮交互（🔵《通过AgentAbilityExtension实现智能体间A2A协议通信》）。

### 1.3 Agent Framework Kit（HMS 侧）—— FunctionComponent

🟢 本地 SDK `sdk\default\hms\ets\kits\@kit.AgentFrameworkKit.d.ts` 全文（13 行）：

```ts
// @since 6.0.0(20)
import { AgentController, BaseOptions, FunctionController, FunctionOptions, ButtonType,
         FunctionComponent } from '@hms.ai.AgentFramework';
import { Role, TaskState, Part, Message, Artifact, TaskStatus, Task,
         OnDataCallback, createA2AServer, ProxySender, Server, RequestContext,
         TaskArtifactParam } from '@hms.ai.A2A';
export { ... };
```

两个模块：
- **FunctionComponent**：**反向**能力 —— 在我们 App 内放一个 UI 控件，**拉起小艺开放平台上创建的云端智能体**
  （「应用 + 智能体」组合服务）。⚠️ 运行时要求：**华为账号已登录 + 联网 + 隐私协议已同意**
  （错误码 1022400011/1022400012/1022400013，🟢+🔵）。
- **A2A Server 全套类型**：`createA2AServer` 等（见 §2.2 示例）。

### 1.4 小艺开放平台（云端智能体）

- 在平台上**创建智能体**（工作流编排）→ **关联应用**（包名/appId 一致并开启关联）→ 上线。
- `FunctionComponent({ agentId: 'agentproxy6548…' })` 用平台的 AgentID 拉起。
- FAQ 实锤（🔵）：agentId 配错或未完成上架/关联 → 「智能体未授权给该应用」。
- 意图框架的「意图注册」也在此平台提交审核（需 App 先上架 AppGallery）。

### 1.5 与实况窗的对比（为什么这条路走得通）

| | 实况窗（OH7 文档 §4.1） | 端侧 Agent |
|---|---|---|
| 场景准入 | 16 类白名单，无一匹配；明确排除单点提醒 | **无场景白名单**（AgentCard 自描述） |
| 月活门槛 | ≥1000 且已上架 | **无**（端侧 A2A 未提及） |
| 模拟器 | 不支持 | 同样不支持，但**没有不可逾越的准入墙** |

---

## 2. 官方示例代码（怎么用）—— 有样本，三套

### 2.1 端侧 A2A 基础版：`AgentExtensionAbility`（API 24+）

🔵 官方《使用AgentExtensionAbility组件实现智能体服务》完整步骤：

**① 工程结构**（在 Module 的 ets 目录下新建）：
```
ets/
└── agentextability/
    └── AgentExtAbility.ets
```

**② 注册到 module.json5**（`type` 必须是 `"agent"`，用 metadata 绑定 AgentCard）：
```json
{
  "module": {
    "extensionAbilities": [
      {
        "name": "AgentExtAbility",
        "icon": "$media:icon",
        "description": "agent",
        "type": "agent",
        "exported": true,
        "srcEntry": "./ets/agentextability/AgentExtAbility.ets",
        "metadata": [
          { "name": "ohos.extension.agent", "resource": "$profile:agent_config" }
        ]
      }
    ]
  }
}
```

**③ 实现生命周期 + 收发数据**（官方示例）：
```ts
import { common, AgentExtensionAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

export default class AgentExtAbility extends AgentExtensionAbility {
  private comProxy: common.AgentHostProxy | null = null;

  onCreate(want: Want) { /* 首次收到请求时创建 */ }
  onConnect(want: Want, proxy: common.AgentHostProxy) { this.comProxy = proxy; }
  onDisconnect(want: Want, proxy: common.AgentHostProxy) { this.comProxy = null; }

  // 接收客户端（小艺）请求 —— data 是字符串，约定 JSON 协议
  onData(proxy: common.AgentHostProxy, data: string) {
    // ……解析 data、执行业务……
    proxy.sendData('reply message');   // 回传结果
  }

  // 可选：双向安全认证
  onAuth(proxy: common.AgentHostProxy, handshakeData: string) {
    proxy.authorize('auth success');
  }

  onDestroy() { }
}
```

**④ agent_config.json**（`resources/base/profile/`，承载 AgentCard）—— 关键字段（🔵 配置文件说明，🟢 SDK `AgentCard.d.ts` 对应）：

| 字段 | 说明 |
|---|---|
| `agentId` | 同一应用内唯一，≤64 字节 |
| `name` / `description` | 展示给用户看的能力描述（**给大模型匹配用的语义就在这里**），≤512 字节 |
| `category` | 类别，官方举例含 **`"health"`** —— 我们直接能用 |
| `defaultInputModes` / `defaultOutputModes` | MIME 类型，如 `["text/plain"]`、`["application/json"]` |
| `skills[]` | **至少一个技能**：`id` / `name` / `description` / `tags[]` / `examples[]`（示例提示语）/ `inputModes` / `outputModes` |
| `capabilities` | 可选能力：流式响应 / 推送通知 / 状态历史 |
| `type` | API 26 起支持：`APP`（默认）/ `ATOMIC_SERVICE`（元服务） |

### 2.2 鸿蒙 7 新版：A2A 协议服务端（API 26+）

🔵 官方《通过AgentAbilityExtension实现智能体间A2A协议通信》示例 ——
比基础版多了**任务状态机 + 产物**，适合「查询今日用药」这类需要结构化交付的场景：

```ts
import { common, Want, AgentExtensionAbility } from '@kit.AbilityKit';
import { RequestContext, createA2AServer, Server, TaskState, Role } from '@kit.AgentFrameworkKit';

const TAG = '=====A2AServer====';
export default class MyAgentExtensionAbility extends AgentExtensionAbility {
  private server: Server | null = null;

  // OnDataCallback：按 method 分发（Execute / Cancel / PerceptionSuggest）
  private agentOnData = (method: string, context: RequestContext) => {
    const agentId: string = context.getAgentId() ?? '';
    const taskId: string = context.getTaskId() ?? '';
    switch (method) {
      case 'Execute':
        // ① 状态 → 工作中
        this.server?.updateStatus(taskId, {
          state: TaskState.WORKING,
          message: { messageId: 'msg1', role: Role.AGENT,
            parts: [{ mediaType: 'text/plain', text: 'Starting work on your request...' }] }
        });
        // ② 生成产物（结构化交付物，区别于普通消息）
        this.server?.addArtifact(taskId, {
          artifactId: 'result-artifact',
          parts: [{ mediaType: 'text/plain',
            text: '{"action": "completed", "result": "Task finished successfully"}' }]
        });
        // ③ 状态 → 已完成
        this.server?.updateStatus(taskId, {
          state: TaskState.COMPLETED,
          message: { messageId: 'msg3', role: Role.AGENT,
            parts: [{ mediaType: 'text/plain', text: 'Task completed successfully!' }] }
        });
        break;
      case 'Cancel':
        break;
      default:
        break;
    }
  };

  async onCreate(want: Want) {
    try {
      const card = this.context.agentCard;          // 读取 agent_config.json 里的 AgentCard
      this.server = createA2AServer(card, this.agentOnData, want);   // 官方示例原文如此
    } catch (error) {
      hilog.error(0x0000, TAG, `Failed to create server: ${error}`);
    }
  }
}
```

TaskState 全集（🔵）：已提交 / 工作中 / 需要用户输入 / 已完成 / 已取消 / 已失败 / 已拒绝 / 需要认证。

### 2.3 反向调用：`FunctionComponent` 拉起云端智能体（API 20+）

🔵 官方 API 参考示例（App 内嵌一个入口，拉起小艺开放平台上创建的智能体）：

```ts
import { BusinessError } from "@kit.BasicServicesKit";
import { common } from '@kit.AbilityKit';
import { FunctionComponent, FunctionController } from '@kit.AgentFrameworkKit';

@Entry
@Component
struct AgentDemo {
  private functionController: FunctionController = new FunctionController();
  private agentId: string = 'agentproxy65481da1fa2293a8482d45'; // 平台创建智能体时分配
  @State isAgentSupport: boolean = false;

  aboutToAppear() {
    void this.checkAgentSupport();
  }

  async checkAgentSupport() {
    // 先探可用性（华为账号已登录 + 联网 + 隐私协议同意，三者缺一不可）
    let context = this.getUIContext()?.getHostContext() as common.UIAbilityContext;
    this.isAgentSupport = await this.functionController.isAgentSupport(context, this.agentId);
  }

  build() {
    Column() {
      if (this.isAgentSupport) {
        FunctionComponent({
          agentId: this.agentId,
          onError: (err: BusinessError) => { /* 1022400010~14 */ },
          options: { title: '智能创建', queryText: '创建一个新的模式', isShowShadow: true }
        })
      }
    }
  }
}
```

### 2.4 意图框架（对照参考）：装饰器开发（API 20+）

🔵 `@InsightIntentLink / @InsightIntentPage / @InsightIntentFunction / @InsightIntentEntry / @InsightIntentForm / @InsightIntentEntity`
装饰器家族 —— 把 URI / 页面 / 静态函数 / 卡片定义为「意图」。官方示例（音乐 App）核心参数：

```ts
@InsightIntentLink({
  intentName: 'PlayMusic',
  domain: 'MusicDomain',
  displayName: '播放歌曲',
  llmDescription: '支持传递歌曲名称，播放音乐',   // ← 给大模型看的功能描述
  keywords: ['音乐播放', '播放歌曲', 'PlayMusic'],
  parameters: { /* JSON Schema：入参结构 */ },
  result: { /* JSON Schema：返回结构 */ },
  uri: '',
  paramMappings: [{ paramName: 'songName', paramMappingName: 'music',
                    paramCategory: LinkParamCategory.LINK }]
})
export class ClassForLink { /* … */ }
```

> 对我们的启示：`llmDescription` + `keywords` + `examples` 就是**我们写给小艺的「说明书」**，
> 写得越贴近老人的口语（「吃什么药」「打卡」），匹配越准。

---

## 3. MedReminder 定制样例（如果接，大概长这样）

> ⚠️ 以下为**方案示意**（基于 §2 官方样例改写），未编译未实测 —— 真正写码前须过 §5 的架构前置。

### 3.1 agent_config.json —— 我们的名片

```json
{
  "agentCards": [{
    "agentId": "med-reminder-agent",
    "name": "MedReminder 用药助手",
    "description": "查询今天的用药计划、还剩哪些药没吃、下次用药时间，并支持服药打卡",
    "version": "1.0.0",
    "category": "health",
    "iconUrl": "https://…/icon.png",
    "defaultInputModes": ["text/plain"],
    "defaultOutputModes": ["text/plain", "application/json"],
    "skills": [
      {
        "id": "query-today-meds",
        "name": "查询今日用药",
        "description": "查询今天还剩哪些药没吃、下次用药是几点",
        "tags": ["medication", "health", "query"],
        "examples": ["我今天还有什么药没吃", "下一顿药是什么时候", "今天的药都吃完了吗"]
      },
      {
        "id": "check-in-med",
        "name": "服药打卡",
        "description": "记录用户已经服用了某次药",
        "tags": ["medication", "checkin"],
        "examples": ["我把晚上的降压药吃了", "帮我打卡早上那顿"]
      }
    ]
  }]
}
```

### 3.2 对话效果（结合后的样子）

```
用户（对手机/手表说）：小艺，我今天还有什么药没吃？
小艺：正在查询 MedReminder……
  → 连接 AgentExtAbility → onData("query-today-meds")
  → 读原生存储中的今日排程 → addArtifact({"pending":["苯磺酸氨氯地平 20:00"]})
小艺（播报）：您晚上 8 点还有一次苯磺酸氨氯地平没有吃。
```

---

## 4. 结合以后的结果（产品愿景）

### 4.1 五个具体场景

| # | 场景 | 用户体验 | 技术路径 | 价值 |
|---|---|---|---|---|
| **A** | **语音查询用药** | 「我今天还有什么药没吃？」→ 小艺直接播报剩余药次与下次时间 | 端侧 A2A（§2.1/2.2），只读技能 | ⭐⭐⭐ 不打开 App、不看屏幕 —— 适老化质的飞跃 |
| **B** | **语音打卡** | 「我把晚上的降压药吃了」→ 已记录，今日依从率 100% | 端侧 A2A，**写技能**（见 §5.2 分期） | ⭐⭐⭐ 打卡从 4 步（解锁→开 App→找药→点按钮）变 1 句话 |
| **C** | 小艺建议主动分发 | 系统学会习惯后，负一屏/桌面主动给「该吃降压药了」卡片，点击直达对应药页 | 意图框架 + 小艺开放平台注册审核（**需已上架**） | ⭐⭐ 桌面级免费曝光 |
| **D** | App 内嵌「用药助手」 | App 里加「问一问」入口，问「这个药饭前吃还是饭后吃」 | FunctionComponent + 云端智能体（§2.3） | ⭐ 需华为账号+联网；⚠️ 合规见 §6.4 |
| **E** | 适老化外放矩阵 | 通知到点响、桌面卡片常驻看、随口一问就答 | **代理提醒 + S-9 卡片 + Agent 三件套** | ⭐⭐⭐ 三个能力共享同一个数据层 |

### 4.2 与既有能力的关系（一张图）

```
              ┌─ 代理提醒（OH7 文档 §2）── 到点推：系统通知，杀进程也可靠
原生数据层 ───┼─ 服务卡片（S-9）──────── 常驻看：桌面一眼下次用药
（前置）      └─ 端侧 Agent（本文）────── 开口问：查询/打卡一句话完成
```

**关键洞察：三者瓶颈是同一个 —— 数据层迁移（§5.1）。做一次，三份产出。**

### 4.3 对上架审核的反哺

OH7 文档 §4.3 最大的驳回风险是「单一 H5/Web 页面（套壳）」。Agent 是**最强的反证材料**：
`AgentExtensionAbility`（独立原生进程）+ 代理提醒 + 相机打卡，构成不可辩驳的原生能力证据。
且「无账号 / 数据不出手机」定位下，**端侧 Agent 处理数据全在设备本地**，仍是合规加分项。

---

## 5. 架构改造（四个前置，缺一不可）

### 5.1 前置①：数据层迁移（与 S-9 共享，最关键）

我们的排程/打卡数据在 **WebView 的 `localStorage`** 里；`AgentExtensionAbility` 是**独立进程**，读不到。
沿用 OH7 文档 §3.2 已预研的三方案：
- 方案 1：换 `@capacitor/preferences`（数据落到原生侧）
- 方案 2：写插件，web 层每次变更同步给原生
- 方案 3：卡片/Agent 只读展示，数据由 web 推送

**倾向**：方案 1 或 2 —— 因为 Agent 不只要「读」（查询），二期还要「写」（打卡），纯推送(3)撑不住。

### 5.2 前置②：打卡逻辑的位置 —— 建议分期

`checkIn()` 及整套滚动排程逻辑（`core/schedule.js`，36 个导出）是 **web 层 JS**，Agent 进程跑不了。两个选择：

| 方案 | 做法 | 代价 |
|---|---|---|
| 下沉 | 核心排程/打卡逻辑用 ArkTS 重写一遍 | **双实现**，两处维护，违背我们「单一事实来源」纪律 |
| 转发 | Agent 收到打卡请求 → 拉起主 Ability → WebView 执行 JS → 结果经数据层回传 | 有界面闪现感；但逻辑仍单实现 |

**建议分期**：**一期只做只读技能**（查询今日/下次用药 —— 数据层迁移后纯读即可支撑）；
**二期再评估写技能**（打卡，届时按方案 2 做事件转发，或视收益决定是否下沉）。

### 5.3 前置③：工程结构 —— 不能放 HAR（🟢 SDK 实测的重要发现）

SDK 原文（`@ohos.app.agent.AgentExtensionAbility.d.ts` 第 24 行）：

> *The class of agent extension ability. **This class cannot be used in Harmony Archive(HAR).***

**直接后果**：Capacitor 鸿蒙插件是 HAR 形态 → **Agent 能力不能做成 Capacitor 插件**，
必须直接放在**应用主模块（entry）**的 `ets/agentextability/` 下。好在这对我们是**利好** ——
web 层一行不用改，Agent 是纯增量原生代码。

### 5.4 前置④：必须真机

- Agent Framework Kit 官方约束（🔵《Agent Framework Kit简介》）：**仅 Phone/Tablet、仅中国大陆（港澳台除外）、不支持模拟器**。
- 端侧 A2A 的客户端是**小艺** —— 模拟器上没有完整小艺，就算编译通过也无法端到端验证。
- ⚠️ 与代理提醒相反：代理提醒是**模拟器就拦**（1700002），Agent 是**模拟器放行但验不了真效果**。
  **凡 Agent 结论，一律真机说了算。**

---

## 6. 约束与风险

| # | 风险 | 等级 | 说明与对策 |
|---|---|---|---|
| 6.1 | 模拟器不可验 | 🔴 | 见 §5.4。排期上把 Agent 全部验证压到真机阶段 |
| 6.2 | 意图注册需已上架 | 🟠 | 场景 C 依赖 AppGallery 上架 + 小艺开放平台审核；但场景 A/B（端侧 A2A）文档未提上架前置 —— **待真机实测确认** |
| 6.3 | HAR 限制 | 🟠 | 见 §5.3，放 entry 模块即可 |
| 6.4 | 医疗合规 | 🟠 | 场景 D 云端智能体：药名会出网，与「数据不出手机」定位冲突 → 需用户明确同意或默认关闭；回答定位为「说明书查询」而非医嘱，加免责声明。场景 A/B 端侧处理，无此问题 |
| 6.5 | 小艺调度效果未知 | 🟡 | AgentCard 写得再好，小艺实际「愿不愿意」把「吃什么药」路由给我们，**真机见分晓** |
| 6.6 | `exported: true` 的安全面 | 🟡 | Agent 对外暴露，`onData` 入参必须当不可信输入处理（JSON 校验后再用）；`onAuth` 可做双向认证 |

---

## 7. 对排期的建议

在 OH7 文档 §6 的队列上修订（原：① 代理提醒 → ② 手表 → ③ TTS → ④ 卡片）：

```
① 代理提醒 AGC 申请（不变 —— 生死线，8 个工作日，先排队）
② 数据层迁移预研（localStorage → 原生可见）★新增：卡片 + Agent 的共同前置
③ TTS（不变，S-10）
④ 手表通知同步实测（不变，可能白捡）
⑤ 「原生化外放」阶段 = S-9 卡片 + Agent 一期（只读技能）★合并立项
⑥ Agent 二期（写技能：语音打卡）+ 意图注册（需已上架）★后置
```

理由：⑤ 里两个能力共享数据层与真机验证窗口，分开做是重复劳动；
⑥ 的写技能牵动核心逻辑位置决策（§5.2），在一期真机反馈（小艺调度效果、老人实际会不会用）之前不宜押注。

---

## 8. 待实测清单（全部真机）

| # | 要验什么 | 前置 |
|---|---|---|
| 1 | 真机小艺能否发现并连接我们的 `AgentExtensionAbility`（「我今天还有什么药没吃」端到端） | 数据层迁移（只读即可） |
| 2 | AgentCard 的 `name/description/skills.examples` 对小艺路由命中率的影响（口语化 vs 书面化 A/B） | #1 通 |
| 3 | **端侧 A2A 是否真的不需要上架**（文档未提，但需实锤） | #1 通 |
| 4 | `AgentExtensionAbility` 与主进程（WebView）同时存活的内存/耗电表现 | #1 通 |
| 5 | `onData` 请求的实际 JSON 结构（官方文档未给出协议样例，只能抓日志看） | #1 通 |
| 6 | 意图装饰器在 Capacitor 工程 entry 模块能否正常注册并被小艺建议识别 | 已上架 |
| 7 | FunctionComponent 拉起云端智能体全流程（小艺开放平台创建→关联→真机拉起） | 平台账号 |

---

## 9. 来源与可靠性

| 结论类别 | 来源 | 等级 |
|---|---|---|
| Agent Framework Kit 存在性、导出清单（FunctionComponent 全套 + A2A Server 全套）、@since 6.0.0(20) | 本地 SDK `sdk\default\hms\ets\kits\@kit.AgentFrameworkKit.d.ts`（13 行全文读过） | 🟢 |
| `AgentExtensionAbility` @since 24、六个生命周期回调、**不能用于 HAR** | 本地 SDK `sdk\default\openharmony\ets\api\@ohos.app.agent.AgentExtensionAbility.d.ts` | 🟢 |
| `AgentCard` / `AgentHostProxy` / `AgentExtensionContext` / `AgentUIExtensionAbility` 存在性 | 本地 SDK `...openharmony\ets\api\` 与 `...api\application\`（同名 .js 版亦存在） | 🟢 |
| ArkAF 三路径架构与对比表 | 官方《方舟智能开发框架概述》 | 🔵 |
| 端侧 A2A 概念/运行机制、AgentExtensionAbility 开发步骤、agent_config.json 字段 | 官方《端侧A2A框架概述》《使用AgentExtensionAbility组件实现智能体服务》《AgentExtensionAbility配置文件说明》 | 🔵 |
| API 26 A2A 协议通信（Task/Message/Artifact/TaskState、createA2AServer 示例） | 官方《通过AgentAbilityExtension实现智能体间A2A协议通信》 | 🔵 |
| FunctionComponent 用法、错误码 1022400010~14（华为账号/联网/隐私协议） | 官方《FunctionComponent（功能组件）》API 参考 | 🔵 |
| 意图装饰器家族与示例 | 官方《@ohos.app.ability.InsightIntentDecorator》API 参考 | 🔵 |
| 仅 Phone/Tablet、仅中国大陆、不支持模拟器 | 官方《Agent Framework Kit简介》约束与限制 | 🔵 |
| 小艺开放平台关联/授权/审核 FAQ | 官方 FAQ（智能体框架、意图框架若干篇） | 🔵 |

> ⚠️ **本文的诚实边界**：
> - 全部为**文档与 SDK 层面**的调研，**零真机实测** —— §8 清单是下一步的全部未知数。
> - `onData` 收到的请求 JSON 具体结构，官方文档**没有给样例**，§3 的对话效果是基于协议语义的合理推演。
> - §3「MedReminder 定制样例」未编译未验证，仅作方案讨论用。
