# GitHub 私有仓库仍读不到 —— 诊断结论与修复

> 时间：2026-09-13 22:45 ｜ 结论：**配置没错，Token 没错，只差一次重启。**
> **✅ 已于 22:56 重启后验证通过**（见文末「验证结果」）。

---

## 一句话结论

`github-pat` 这个自定义 MCP server **已经配好、已经 Trust、端点也已验证可通**，但它没能进入**当前这个会话**——因为会话的工具/服务器列表是**会话加载那一刻的快照**，而配置是在那之后才写入的。

**修复：完整退出 WorkBuddy（确保进程结束）→ 重新打开 → 新开一个对话。** 之后 `mcp__github-pat__*` 工具就会出现，私有仓库即可读。

---

## 证据链（全部实测）

| 时间（本地） | 事件 | 来源 |
|---|---|---|
| 22:09:02 | WorkBuddy 最后一次启动 | `AppStartup.log` |
| 22:09:49 | 会话加载，参数 **`"mcpServers":[]`**（空的） | `daemon.log` |
| 22:38:11 | 写入 `~/.workbuddy/mcp.json` + `github-pat` 被 Trust | 文件 mtime / `mcp-approvals.json` |
| 22:38:25 | `mcp:toggle` RPC 执行（耗时 14.8s，成功） | `daemon.log` |
| 22:45 | 端点 `initialize` 返回 **HTTP 200** + `mcp-session-id` | 本地 curl 实测 |

决定性的一行是这条（`daemon.log`）：

```json
{"timestamp":"…14:09:49.357Z","scope":"daemon-server:daemon-bootstrap",
 "message":["[Daemon] LOAD session: sessionId=ebc4d468…,
   params={\"cwd\":\"D:\\\\WorkBuddy\\\\MedReminder\",\"mcpServers\":[], …}"]}
```

会话是在 **22:09:49** 加载的，那时自定义 server 列表是空的；`mcp.json` 到 **22:38:11** 才出现。中间那 29 分钟里没有任何 `github-pat` 的加载记录（main / daemon / renderer 三份日志全查过）。

也就是说：**Trust 只是记录授权，不会热加载进已存在的会话。发新消息也不会刷新。**

---

## 端点自检：证明配置本身完全正确

直接用你的 PAT 打官方 GitHub MCP 端点：

```powershell
curl.exe -k -X POST "https://api.githubcopilot.com/mcp/" `
  -H "Authorization: Bearer <你的PAT>" `
  -H "Content-Type: application/json" `
  -H "Accept: application/json, text/event-stream" `
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"probe","version":"1.0"}}}'
```

实测返回：

```
HTTP/1.1 200 OK
mcp-session-id: 15d5ef0d-e7a7-422e-aa09-c5ba7a8d6880
Server: github.com
Content-Type: text/event-stream

event: message
data: {"jsonrpc":"2.0","id":1,"result":{"capabilities":{…},
       "instructions":"The GitHub MCP Server provides tools to interact with GitHub platform…"}}
```

→ **URL、协议、`headers.Authorization`、Token 权限，全部正确。** 所以重启后应当直接可用，无需改配置。

---

## 重启后怎么验证

重启并新开对话后，逐条确认：

- [ ] 工具列表里出现 `mcp__github-pat__*`（而不是只有 `mcp__github__*`）
- [ ] 读到 `DaviKee/med-reminder` 的根目录文件列表（当前会 404）
- [ ] 能列出 `master` 上的提交 `47c1a694`（66 个文件）

---

## ✅ 验证结果（2026-09-13 22:56，用户重启后）

| 检查项 | 结果 |
|---|---|
| `mcp__github-pat__*` 命名空间是否存在 | ✅ 已出现，可正常加载 |
| `github-pat get_me` | ✅ 返回 `DaviKee`（id 327536055） |
| 读私有仓库根目录 | ✅ 返回 11 个顶层条目：`.gitignore`、`README.md`、`android/`、`build-apk.sh`、`capacitor.config.json`、`install.html`、`package-lock.json`、`package.json`、`qrcode.svg`、`serve-apk.py`、`www/` |
| 读文件内容 | ✅ `package.json` 正常返回（blob SHA `a64687fe…`） |
| 分支与提交 | ✅ `master @ 47c1a69490fb84ea9a4eea2106220103340a769e`（与 API 上传的初始提交一致） |
| 对照：内置连接器 `mcp__github__get_file_contents` | ❌ 同一仓库**仍 404**（其 token 仅公开读） |

**结论：修复动作（完整重启）有效，`github-pat` 已可用，私有仓库可读写。**

---

## 如果重启后仍然不行

按这个顺序排查：

1. **确认工具命名空间**：让 AI 列一次已连接的 MCP server（`ListMcpResources`），看 `github-pat` 是否在其中。
2. **看加载日志**：`%USERPROFILE%\.workbuddy\logs\daemon.log` 搜 `LOAD session`，确认 `mcpServers` 字段里有 `github-pat`。
3. **看 toggle 日志**：`main.log` 搜 `mcp:toggle`，若报错会带原因。
4. **备选方案**：改走「重新授权内置 GitHub 连接器」——在连接器管理页对内置 **GitHub** 点重新授权，在 GitHub 授权页勾上私有仓库权限。好处是磁盘不落 token（见配置指南 §五）。

---

## 顺带说明：`gh` CLI

本次仍**未安装**（你当时选了只走自定义 MCP）。要装就一行：

```powershell
winget install --id GitHub.cli -e
```

装完 `gh auth login`。但注意：本沙箱历史上只稳定放行 `api.github.com`、会拦 `github.com:443`，`gh` 在这里可能仍受限。**只为了让 AI 读私有仓库的话，MCP 已经够用。**

---

## 安全提醒（重复一次，重要）

1. PAT 以**明文**存在 `C:\Users\davik\.workbuddy\mcp.json`，别提交、别外发、别截图。
2. 该 PAT 实测权限为 `repo, workflow`，且已在对话中出现过 —— 建议用完后去 GitHub → Settings → Developer settings **吊销并重建**；换成 **fine-grained PAT** 只授权 `med-reminder` 一个仓库更稳。
