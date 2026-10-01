# GitHub 工具链配置指南（WorkBuddy 环境）

> 生成时间：2026-09-13（**在旧机上写的**）｜ 环境：Windows / AMD64 ｜ 目的：让 AI 具备 GitHub MCP + gh CLI 能力

> ⚠️ **迁移说明（2026-09-23 补）**
> 本文写在旧机（系统账号 `davik`）上。换到新机（账号 `Qinn`）后，文中所有形如
> `C:\Users\davik\...` 的**绝对路径，一律按 `%USERPROFILE%\...` 理解**，不要照抄。
>
> 新机现状：
> - `mcp.json`（含 PAT）已由备份恢复 → `C:\Users\Qinn\.workbuddy\mcp.json`
> - 所以**第三节的结论在新机上同样成立**：首次启用 `mcp__github-pat__*` 前，
>   必须**完整重启 WorkBuddy**（不是关窗口），因为 server 列表是会话加载那一刻的快照。
> - 新机 **gh CLI 未安装**，且无 winget（不在 PATH）、无可用代理 —— 第四节步骤在新机大概率不可行。

---

## 一、诊断结论（实测，非推测）

### 1. GitHub MCP —— 本来就能用，此前判断有误

之前我说"MCP 工具没暴露给你"是**错的**，实测证据：

| 测试 | 结果 |
|---|---|
| `mcp__github__get_me` | ✅ 返回 `DaviKee`(id 327536055，注册于 2026-09-10) |
| 读公开仓库 `Futsch1/medTimer` 目录 | ✅ 完整返回 34 个条目 |
| 读私有仓库 `DaviKee/med-reminder` | ❌ **404 Not Found** |
| 搜索 `user:DaviKee` | ⚠️ 只返回公开仓库 `David-Kee`，**私有仓库不出现** |

**结论**：内置连接器**已授权身份，但只有公开仓库读取权限**，没有私有仓库的授权范围。这才是真正的问题所在。

内置连接器的定义位于：
```
%USERPROFILE%\.workbuddy\connectors\default\mcp.json   →   "connector:github"
{
  "timeout": 600000,
  "url": "https://api.githubcopilot.com/mcp/",
  "disabled": true
}
```
它接的实际就是**官方 GitHub MCP server**（`api.githubcopilot.com/mcp/`）。

### 2. gh CLI —— 确实没装，但机器具备安装条件

| 检查项 | 结果 |
|---|---|
| `winget` | ✅ `C:\Users\davik\AppData\Local\Microsoft\WindowsApps\winget.exe` |
| `choco` / `scoop` | ❌ 未安装 |
| `gh.exe`（全盘 + PATH） | ❌ 未找到 |
| `C:\Program Files\GitHub CLI\` | ❌ 不存在 |
| `~/.workbuddy/mcp.json` | ❌ **原本不存在**（说明此前只用内置连接器，未配过自定义 MCP） |

---

## 二、已完成的动作

新建 `%USERPROFILE%\.workbuddy\mcp.json`，接入官方 GitHub MCP 并用你自己的 PAT 鉴权：

```json
{
  "mcpServers": {
    "github-pat": {
      "type": "streamableHttp",
      "url": "https://api.githubcopilot.com/mcp/",
      "headers": {
        "Authorization": "Bearer ghp_****（你的 PAT，明文存储，注意保管）"
      },
      "timeout": 600000,
      "disabled": false
    }
  }
}
```

已通过 JSON 合法性校验：

```
EXISTS: True
JSON_VALID: true
SERVERS: github-pat
  [github-pat] url=https://api.githubcopilot.com/mcp/ type=streamableHttp disabled=False hasAuth=True
```

PAT 有效性实测：

```
STATUS: HTTP/1.1 200 OK
X-OAuth-Scopes: repo, workflow      ← 含私有仓库读写权限
长度: 40 字符（完整 classic PAT）
```

> 命名为 `github-pat` 而非 `github`，是为了**避免与被内置连接器占用的 `mcp__github__*` 工具命名空间冲突**。

---

## 三、你需要做的下一步（关键）

> **更新（2026-09-13 22:45 实测）**：Trust 已经点过了（`mcp-approvals.json` 里已有 `github-pat` 记录），但工具仍未生效。根因已定位，见下方"三之补充"。**结论：必须完整重启 WorkBuddy。**

原步骤（已完成）：

1. 打开 **连接器管理页**
2. 点右上角 **「自定义连接器」** 入口
3. 在列表里找到新增的 **`github-pat`**，点 **「信任 / Trust」** 启用

### 三之补充：为什么 Trust 完还是用不了

**根因：MCP server 列表在「会话加载」时被快照固定，之后新增/启用的 server 不会注入已加载的会话。**

证据链：

| 时间（本地） | 事件 | 来源 |
|---|---|---|
| 22:09:02 | WorkBuddy 最后一次启动 | `AppStartup.log` |
| 22:09:49 | 会话加载：`params={"cwd":"…","mcpServers":[]}` ← **空** | `daemon.log` |
| 22:38:11 | `mcp.json` 写入 + `github-pat` 被 Trust | 文件 mtime / `mcp-approvals.json` |
| 22:38:25 | `mcp:toggle` RPC 执行成功（耗时 14.8s） | `daemon.log` |
| 22:45 | 端点 `initialize` → **HTTP 200** + `mcp-session-id` | 本地 curl 实测 |

也就是说：**配置读到了、Trust 记录了、toggle 跑了，但这个会话的 server 列表是 22:09:49 那一刻的快照，里面是空的。** 发新消息不会刷新它。

**正确操作：完整退出 WorkBuddy（不是关窗口，确保进程结束）后重新打开。** 重启后再开一个对话，`mcp__github-pat__*` 工具就会出现。

端点自检（可选，用来确认配置本身没写错）：

```powershell
curl.exe -k -X POST "https://api.githubcopilot.com/mcp/" `
  -H "Authorization: Bearer <你的PAT>" `
  -H "Content-Type: application/json" `
  -H "Accept: application/json, text/event-stream" `
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"probe","version":"1.0"}}}'
# 期望：HTTP 200，响应头带 mcp-session-id，body 是 event: message + result.capabilities
```

---

## 四、可选的 gh CLI 安装（本次未执行）

如果还想在命令行里直接跑 `gh api` / `gh pr` / `gh run`：

```powershell
winget install --id GitHub.cli -e
```

装完后需要：

1. **加入 PATH**（winget 通常自动做，若未生效，新开一个终端）
2. **登录**：
   ```powershell
   gh auth login
   ```
   选 `GitHub.com` → `HTTPS` → 用浏览器授权，或粘贴同一个 PAT。

> ⚠️ 本沙箱环境下 `gh` 可能仍会受**代理/证书**限制（历史上沙箱只稳定放行 `api.github.com`）。如果只是想让 AI 操作 GitHub，**MCP 已经足够，gh 属可选**。

---

## 五、备选方案：不开自定义 MCP，直接修内置连接器

如果你**不想让 PAT 以明文躺在磁盘上**，可以走这条路：

- 在连接器管理页找到内置的 **GitHub** 连接器 → **重新授权 / 管理授权**
- 在 GitHub 授权页确认勾选**私有仓库**访问权限

这样直接扩容内置连接器的权限，磁盘上不落任何 token。代价是授权范围由连接器的 OAuth 应用决定，未必能精确控制到 `repo` 粒度。

**两条路对比**：

| | 自定义 MCP（本次已做） | 重新授权内置连接器 |
|---|---|---|
| 私有仓库权限 | ✅ 由你的 PAT 决定，`repo` 全权限 | ⚠️ 取决于连接器的 OAuth 范围 |
| 可控性 | ✅ 高，可加工具白名单、超时 | ⚠️ 中 |
| 磁盘留 token | ❌ 明文存于 `mcp.json` | ✅ 不留 |
| 需要手动操作 | 点一次 Trust | 走一次 OAuth 授权 |

---

## 六、WorkBuddy MCP 配置 schema 参考

从 `connectors/default/mcp.json` 反推出的可用字段（以后加任何 MCP server 都能照抄）：

```json
{
  "mcpServers": {
    "<server-name>": {
      "type": "streamableHttp | sse | http | stdio",
      "url": "https://...",                    // HTTP/SSE 类必填
      "headers": { "Authorization": "Bearer ${ENV_VAR}" },   // 支持 ${VAR} 占位符
      "staticHeaders": { "X-Client": "workbuddy" },
      "command": "npx",                        // stdio 类必填
      "args": ["-y", "some-mcp@latest"],
      "env": { "API_KEY": "${API_KEY}" },
      "runtime": { "type": "node", "version": ">=20" },
      "timeout": 600000,
      "disabledTools": ["tool_to_hide"],
      "disabled": false
    }
  }
}
```

注意：
- HTTP/SSE 类用 `url` + `type`；stdio 类用 `command` + `args` + `env`
- 密钥可以用 `${VAR_NAME}` 引用环境变量，避免明文
- `~/.workbuddy/mcp.json` 是**用户级**自定义 MCP；`connectors/default/mcp.json` 是**内置连接器**目录，不要手改后者

---

## 七、验证清单

配置生效后，逐条验证：

- [ ] 我能否读到 `DaviKee/med-reminder`（私有仓库）的文件列表
- [ ] 我能否列出自定义 MCP 暴露的工具（应出现 `mcp__github-pat__*`）
- [ ] 我能否用 MCP 直接向私有仓库提交文件（`create_or_update_file` / `push_files`）
- [ ] 若装了 gh：`gh auth status` 是否显示已登录

---

## 八、安全注意事项

1. **PAT 明文存储**：`~/.workbuddy/mcp.json` 中的 token 是明文。别把该文件提交到任何仓库，别截图外发。
2. **PAT 权限**：实测为 `repo, workflow`。`workflow` 允许修改 GitHub Actions 工作流文件；若不需要，建议降级重建一个仅 `repo` 的 PAT。
3. **轮换建议**：这个 PAT 已在本次对话中出现过，建议用完后到 GitHub → Settings → Developer settings → Personal access tokens **重新生成一个并吊销旧的**。
4. **最小权限**：可改用 fine-grained PAT，只授权 `med-reminder` 等特定仓库，而不是全账号 `repo`。
