# MedReminder 验证 harness

> 一句话：**把散落各处的检查收进一个入口、一份报告、一个退出码。**
> 让「改了没跑」不再可能悄悄发生。

---

## 为什么需要它

这个项目此前的检查是**散落**的，且**大部分只写在文档里、靠人记得去跑**：

| 检查 | 住在哪 | 以前怎么跑 |
|---|---|---|
| 源码级行为回归（13 套 / 478 条） | `tests/*.spec.js` | `npm test`（沙箱里还会因禁子进程而挂） |
| 交付前内容级验收（106 项） | `tools/verify-apk.py` | 手敲，经常忘 |
| 架构铁律：漏 import | `.workbuddy/tools/check-module-imports.py` | 只在搬运模块时想起来 |
| 出包四项核对 / git 身份 / 记忆体检 | **只写在文档和记忆里** | **靠自觉** |

后果很具体：**「改完不跑」或「只跑最近动过的那一个」**，回归防护形同虚设。
本项目已经因此吃过亏 —— 例如「文档里明明写了要核提交作者，还是漏核」、
「验收脚本写完却没法验证它自己写得对不对」。

harness 就是把这些**收口**：一条命令跑完，一份报告看完，一个退出码判死。

---

## 怎么用

```bash
cd C:/WorkBuddy/med-reminder/med-reminder

# 跑全部关口（推荐：提交前 / 出包前 / 真机反馈改完代码后）
python harness/harness.py

# 只列关口清单（看看有哪些检查，以及"为什么要它"）
python harness/harness.py --list

# 只跑某类关口（id 子串匹配）
python harness/harness.py --only spec      # 只跑全部用例
python harness/harness.py --only photo     # 只跑 photo 相关

# 看某个关口的完整输出（排查失败原因时用）
python harness/harness.py --only syntax --verbose

# 自检 harness 自己（防"汇总算错 → 谎报全绿"）
python harness/harness.py --selftest        # 单元级：判定逻辑
python harness/harness.py --selftest-e2e    # 端到端：注入假失败，确认真的报红

# 输出机器可读结果（给别的脚本消费）
python harness/harness.py --json
```

> ⚠️ **本机必须用 python 绝对路径**：裸调 `python` 会落到 Windows 商店的占位 stub，
> 报 `SyntaxError: Non-UTF-8 code`。用：
> `C:/Users/Qinn/.workbuddy/binaries/python/versions/3.13.12/python.exe harness/harness.py`
> （harness 内部找 node/python 时也**一律用绝对路径**，不依赖 PATH。）

---

## 退出码（含义不同，别混）

| 码 | 含义 | 你该做什么 |
|---|---|---|
| **0** | 全部通过（跳过的**不算**失败，但会在报告里列出） | 可以提交 / 交付 |
| **1** | 有断言失败或关口报红 | 去看是哪个关口、修它 |
| **2** | **环境问题**（命令找不到、子进程被禁、超时）—— **不代表代码有问题** | 换普通终端重跑，或修环境 |

> ★ **三态分离**是刻意设计：本项目踩过「沙箱禁子进程 → `spawnSync` 返回 `EBUSY` →
> 被误当成用例失败」。环境问题必须自成一类，不能污染"代码对不对"的判断。

---

## 两条不可动摇的规矩

### 1. 跳过 ≠ 通过

没跑的关口在报告里显式标 `SKIP` 并说明原因（例如"没有找到 APK，未出包"），
**绝不计入"通过"**。报告末尾会单独列出所有被跳过的关口。

### 2. 假绿必须挡住

spec 报「通过 N / 共 M」时，判定规则是 **M > 0 且 N == M** 才算通过。
- `M == 0`（空集通过）→ **判 FAIL**，不是 PASS。
- 抠不到汇总行 → 判 `CRASH`（套件自身出错），不是 PASS。

这条对应本项目的一句老话：**「把断言写绿就是验证过了」是错觉 —— 断言可能锁住错代码，
也可能假绿（空集通过）。**

### 3. 有假阳性的检查，诚实标"咨询性"，别当闸门

一个**永远红的检查等于没有检查** —— 红久了就没人看了（"狼来了"）。
有已知假阳性的工具（如 `check-module-imports.py`）标为 `ADVISORY`：
执行、留输出、不影响退出码。详见下面关口清单里的说明。

---

## 四种状态（互不等价）

| 状态 | 记号 | 含义 | 计入通过？ |
|---|---|---|---|
| `PASS` | ✓ | 通过 | 是 |
| `FAIL` | ✗ | 断言失败 / 退出码非 0 | 否（决定退出码 1） |
| `CRASH` | ‼ | 套件自身出错（没打出汇总行） | 否（决定退出码 1） |
| `SKIP` | ○ | 主动跳过（如没有 APK） | **否** |
| `ADVISORY` | ℹ | 咨询性检查有提示 | **否**（但也不阻断） |
| `ENVERR` | ⚠ | 环境问题（命令找不到等） | —（立即中止，退出码 2） |

> **`SKIP` 不是 `PASS`。** 报告末尾会单独列出所有被跳过的关口与原因 ——
> 「没跑」绝不允许伪装成「跑过了且没问题」。

---

## 关口清单（16 个）

| id | 检查什么 | 类型 |
|---|---|---|
| `syntax` | 全部 `www/js/**/*.js` + `tests/*.js` 的语法（按 module 语义解析，不用 `node --check` 以免误报 export） | script |
| `imports` | 8 个 app 模块「用了却没声明、也没 import」的标识符 —— **咨询性，不阻断**（工具有已知假阳性，见下） | **advisory** |
| `spec:*` | `tests/*.spec.js` 全部 13 套（478 条）—— 源码级行为回归 | spec |
| `apk` | `tools/verify-apk.py` 内容级验收（106 项）；无 APK 时 `SKIP` | script |

### 关于 `imports` 为什么是"咨询性"而不是"硬闸门"

`check-module-imports.py` 靠"标识符出现但未声明/未 import"来找漏 import，**对正则字面量与
注释里的词有已知假阳性**。当前它报的 5 处经逐条核实**全是假阳性**：

| 报的名字 | 真实来源 |
|---|---|
| `$`（schedule.js） | 正则 `/^(\d{1,2}):(\d{2})$/` |
| `Android`（app.js） | 注释里的「Android 13」 |
| `backup`（backup.js） | 正则 `/^backup-\d{8}\.json$/` |
| `A` / `Za` / `z0` / `cancel` / `exist`（photo.js） | 内嵌 vendor 压缩码 |
| `quot` | 单双引号交替的字符串（`'"': '&quot;'`） |

**如果把它当硬闸门，它每天都会假报红 —— 红久了就没人看了（"狼来了"）。**
所以 harness 把它标为 `ADVISORY`：**永远执行、输出留给人看、但不影响退出码**。
真正要判"漏 import"，请看它的输出 + 人工确认，或靠 `syntax`（能解析）与 `spec`（能跑通）间接兜底。

> 反过来说，这条也是 harness 设计的一部分：**一个永远红的检查等于没有检查。**
> 与其把有假阳性的工具塞进闸门，不如诚实标注它的可信度。

**规格**：关口定义在 `harness/gates.py`，**只描述"跑什么、怎么判"，不自己实现检查逻辑** ——
真正的检查住在 `tests/`、`tools/` 里，那里才是唯一真相。

---

## 怎么加一个新关口

在 `harness/gates.py` 的 `gates()` 里 `G.append({...})` 一条即可：

```python
G.append({
    'id': 'my-check',                       # 短标识（报告里用）
    'name': '我的检查',                      # 中文名
    'kind': 'script',                       # 'spec' | 'script' | 'advisory'
    'why': '一句话说清为什么需要它',           # 写给人看，会有 --list 打印
    'cmd': [py, 'some/script.py'],          # 命令（绝对路径优先）
    'cwd': repo,
    # 可选：
    'skip_if': lambda: ctx['apk'] is None,  # 跳过条件
    'skip_why': lambda: '原因',              # 跳过原因
})
```

- `kind='spec'`：输出必须含「通过 N / 共 M」，按 **M>0 且 N==M** 判通过。
- `kind='script'`：以**退出码**判（0 = 通过）。
- `kind='advisory'`：**永不报红**，只统计提示数 —— 给有假阳性的工具用。

加完记得跑一次 `harness.py --selftest` 与 `harness.py --only my-check` 验证接线。

> **不要**在 harness 里重写检查逻辑。它只做三件事：**调度、汇总、挡假绿**。
> 检查逻辑重写了就变成第二份真相，迟早与第一份不同步。

---

## ★ 自检：凭什么相信 harness 自己

这是整套东西里**最重要**的部分。一个"汇总算错"的 harness 比没有 harness 更危险 ——
它会**谎报全绿**，让没跑过的代码看起来验证过了。本项目对此有明确教训：
**「把断言写绿就是验证过了」是错觉。**

所以 harness 有两层自检，**都必须通过**：

### 1. 单元级（`--selftest`）

验证 `judge()` 这个纯函数的判定规则，12 个用例覆盖：

- spec 全绿 → PASS
- spec 有失败 → FAIL
- **spec 假绿（共 0 条）→ FAIL** ← 关键
- **抠不到汇总行 → CRASH**（不是 PASS）← 关键
- 汇总行乱序/无空格/中文空格混杂 → 仍能正确解析
- script 退出码 0/1/2 → PASS/FAIL/FAIL
- advisory 无论如何都 → ADVISORY

### 2. 端到端（`--selftest-e2e`）

**证明 harness 真的会报红，而不是只会打印绿色。** 做法：

1. 备份 `tests/layout.spec.js`；
2. 往它里面**注入一条必定失败的断言**；
3. 跑 harness，要求：**退出码 != 0** 且该 spec 判 **FAIL** 且**不出现"全部通过"**；
4. `finally` 里逐字节恢复原文件并校验 MD5。

实测结果：

```
✗  spec:layout            FAIL   14 / 15（失败 1）
有失败 ❌（退出码 1）
✓ harness 行为正确：注入的失败被抓住了 ✅
[恢复] layout.spec.js 已逐字节还原 ✅
```

> **改完 harness（尤其是汇总/判定逻辑）后，务必跑这两个自检。**
> `--selftest-e2e` 自带回退，不会污染工作区。

---

## 与既有工具的关系

harness **不替代**任何既有工具，而是**编排**它们：

```
harness/harness.py
  ├─ harness/check_syntax.js            ← 新增（语法自检）
  ├─ tests/*.spec.js                    ← 既有，13 套 478 条
  ├─ tools/verify-apk.py                ← 既有，106 项
  └─ .workbuddy/tools/check-module-imports.py  ← 既有
```

- `npm test`（`tests/run-all.js`）仍然可用，但它**会因沙箱禁子进程而挂**（报 `EBUSY`）；
  harness **逐个起进程**，同样能跑，且**多跑语法/import/APK 三类检查**。
- 想跑单个用例，直接 `node tests/xxx.spec.js` 也行 —— harness 只是让"跑全部"变容易。
