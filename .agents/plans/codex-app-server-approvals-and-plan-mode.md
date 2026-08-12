# NeurolingsCE Codex app-server 审批与 Plan mode 实施计划

## 目标与边界

本阶段为 NeurolingsCE 增加一个由用户显式启动、由 GUI 管理的 Codex
app-server 客户端。它只管理 NeurolingsCE 自己创建或用户明确恢复的一个
app-server thread；不能旁听 ChatGPT Desktop、Codex Desktop 或其他 Codex CLI
任务。阶段一的 `notify → NeurolingsCE-cli → Local IPC → 气泡` 链路保持不变，
app-server 不接入 CLI、HTTP 或 notify 配置。

首版约束如下：

- `codex/appServerEnabled` 只控制功能可见性，默认为 `false`，不会自动启动进程。
- 只有“连接 Codex”按钮启动 `codex app-server --listen stdio://`；异常退出、
  协议越界和应用重启均不自动重连。
- 同时只有一个活动 thread；切换前必须停止活动 turn、清理待审批并取消旧订阅。
- 只支持用户明确的 command、file-change、network approval 和
  `item/tool/requestUserInput`；未知 server request 一律拒绝，不自动批准。
- Plan 与最终回复在管理器 Codex 页面完整查看；桌宠只接收有界的最终摘要，
  不显示流式 delta，也不因 Plan 步骤更新而自动召唤桌宠。
- 不持久化用户输入、助手正文、命令、cwd、diff、审批理由或 Plan 正文；只保存
  可恢复 thread 的 ID 与工作目录。

## 协议与进程实现

### 客户端生命周期

由 GUI 线程拥有的 `CodexAppServerClient` 使用 `QProcess`，通过
`setProgram()`/`setArguments()` 启动，不拼接 shell 命令。可执行文件默认由
`QStandardPaths::findExecutable("codex")` 查找，设置页允许用户指定绝对路径。
Windows 发现 `.cmd`/`.bat` 时提示用户选择真实可执行文件，不隐式包装 shell。

连接顺序固定为：

1. 启动 `codex app-server --listen stdio://`。
2. 发送 `initialize`（client name 为 `neurolingsce`，开启 experimental API，
   关闭不需要的高频 reasoning/raw command output notification）。
3. 成功后发送 `initialized`。
4. 调用 `collaborationMode/list`，从返回 preset 中选择 `Default` 与 `Plan`；
   没有 Plan preset 时禁用 Plan 选择并明确显示“不支持计划模式”，不静默降级。
5. 调用 `configRequirements/read`，确认 `on-request` approval 与
   `workspace-write` sandbox 未被管理员禁止。
6. 用户选择“新建会话”或“恢复最近会话”后调用 `thread/start` 或
   `thread/resume`，再按 turn 状态调用 `turn/start`、`turn/steer` 或
   `turn/interrupt`。

stdout 只解析 UTF-8 JSONL；stderr 是限长诊断流，不能混入 JSON-RPC。解析器必须
支持半包、多个 JSON 行、LF/CRLF 和 EOF 半行，并执行固定上限：单行 4 MiB、
未完成缓冲 8 MiB。超限、非法 JSON、非 object、`result`/`error` 同时出现或缺少
必需 envelope 字段时 fail-closed，停止连接并进入 `Blocked`。

JSON-RPC `id` 保留原始字符串或安全整数，所有响应以 envelope `id` 关联；
`threadId`、`turnId`、`itemId` 只用于 UI 和作用域。未知 notification 可限频记录
元数据后忽略；未知 server request 必须回 `-32601 Method not supported`，已知但
参数非法的 request 回 `-32602 Invalid params`。

### 状态与关闭

客户端状态为 `Stopped → Starting → Initializing → Ready → Running /
NeedsInput → Ready`，错误或中断进入 `Blocked`，退出经过 `Stopping` 回到
`Stopped`。所有到 QWidget 的 signal 使用 GUI queued connection。

关闭顺序：停止新操作和审批按钮 → 进程可写时 best-effort 对 pending request
发送 `cancel` → 清空 approval/input store 和页面 → `terminate()` → 短 grace
period 后 `kill()` → 不阻塞 GUI、不自动重启。旧连接的 request ID 通过连接代数
校验，不能发送到新连接。

## 审批与澄清问题

### Approval store

`CodexApprovalStore` 以原始 JSON-RPC request ID 为键，最多保留 16 条，重复 ID
不覆盖；超过上限立即 fail-closed（优先 `cancel`，否则协议错误）。pending 项
按最早请求优先展示。`serverRequest/resolved`、`item/completed`、
`turn/completed`、`thread/closed` 只能清理对应 thread/turn/item 作用域；已解决、
已过期、旧连接和重复点击都必须拒绝。

四个决定精确映射：

| UI | JSON-RPC decision | 作用 |
| --- | --- | --- |
| 仅本次允许 | `accept` | 当前 request |
| 本会话允许 | `acceptForSession` | 当前 app-server 连接，不写 QSettings |
| 拒绝并继续 | `decline` | 拒绝操作，turn 可继续 |
| 拒绝并停止 | `cancel` | 拒绝操作并中断 turn |

按钮只按 server `availableDecisions` 显示；字段缺失时使用稳定审批类型的四项
默认集合。没有自动批准、倒计时批准或永久 policy amendment；不支持的
`acceptWithExecpolicyAmendment`、永久 network policy、permissions approval、
MCP elicitation 和 dynamic tool call 必须拒绝并将会话置为 `Blocked`。

### 页面与桌宠提醒

新增 `ManagerCodexPage`，显示连接状态、thread 短标识/工作目录、Default/Plan
模式、turn 状态、pending 审批、Plan 步骤、最终 Plan/回复、输入框和发送/中止/按
计划实施/修改计划按钮。动态内容用纯文本或只读代码控件，禁止 HTML、链接、资源
加载；命令、路径和 diff 仅存内存。diff 单项最多 128 KiB、总显示最多 256 KiB，
截断必须明确标记。

审批气泡只作提醒：标题“Codex · 需要确认”、类型、短理由和 pending 数量；点击
后导航到管理器 Codex 页面，不放审批按钮、不抢焦点、不执行决定。初始焦点为
“拒绝并继续”，任何允许按钮都不是默认按钮，Enter 不能误触允许；所有按钮提供
可访问名称与说明。没有运行中的专属桌宠时只更新页面和导航徽标，不召唤桌宠。

`item/tool/requestUserInput` 单次最多三个问题，渲染 header、question、2–3 个
选项、描述和 `isOther` 自由输入；`isSecret` 使用密码回显。提交以原始 request ID
返回 `{answers: {question_id: {answers: [...]}}}`；关闭或 `autoResolutionMs`
到期只回一次 `{answers: {}}`，与 approval decision 分开管理。

## Plan mode 与回复

Plan 只由 `collaborationMode/list` 返回的 `mode == "plan"` preset 选择；不以
Markdown 标题或关键词猜测模式。处理 `turn/plan/updated`、`item/plan/delta`、
`item/agentMessage/delta`、`item/completed` 和 `turn/completed`：

- Plan delta 按 `(threadId, turnId, itemId)` 顺序暂存，最多 32 步、每步 2 KiB；
  最终 `item.type == "plan"` 的 `item.text` 始终覆盖 delta。
- commentary/reasoning 不进入最终回复；只有 `agentMessage` 的
  `final_answer`（或缺省 phase）进入最终文本。
- `turn/completed` 成功回 `Ready`，failed/interrupted 回 `Blocked`。
- 发送空输入拒绝；活动 turn 使用 `turn/steer` 并带 `expectedTurnId`，空闲 thread
  使用当前模式 `turn/start`。发送失败不清空输入。
- “按此计划实施”仅在 Plan turn 完成后可用，用同一 thread 的 Default turn 发送
  固定语义“请按已确认的计划开始实施。”；“修改计划”要求非空输入并按当前 turn
  状态 steer/start Plan turn。
- 中止活动 turn 使用 `turn/interrupt {threadId, turnId}`，无活动 turn 时禁用。

页面内存中的 Plan/回复进入气泡前最多 4096 个 Unicode 字素。最终 Plan 只生成一条
“Codex · 计划已完成”摘要，最终 agent message 只生成一条“Codex · 已完成”摘要，
复用阶段一 Markdown 清洗、360×240、最多 8 行、8–12 秒规则。相同
`(threadId, turnId, itemId)` 最多一条最终气泡；app-server 与 notify 同 turn 通过
最多 64 项、TTL 60 秒的去重缓存合并。通知优先级为
`Approval > requestUserInput > Final Plan > Completion`。

## 设置与持久化

设置页在原有 notify 区域旁增加“Codex 交互”：

| QSettings key | 默认/用途 |
| --- | --- |
| `codex/appServerEnabled` | `false`；仅控制页面可见性 |
| `codex/appServerExecutable` | 空；可选绝对路径 |
| `codex/lastThreadId` | 最近成功创建/恢复的 thread |
| `codex/lastWorkspace` | 最近工作目录 |
| `codex/approvalBubbleEnabled` | `true` |
| `codex/planBubbleEnabled` | `true` |

禁用开关时停止连接、取消 pending 并清空页面；不修改 `$CODEX_HOME/config.toml`
或阶段一 notify 托管块，不建立登录系统，身份验证错误只显示限长诊断。用户输入、
回复、命令、diff、Plan 正文不得写 QSettings、日志、剪贴板或遥测。

## 实施顺序与文件责任

1. **协议/模型/进程**：`include/shijima-qt/CodexAppServerModels.hpp`、
   `CodexAppServerProtocol.hpp`、`CodexAppServerClient.hpp` 及
   `src/app/core/codex/CodexAppServerProtocol.cc`、`CodexAppServerClient.cc`。
2. **运行时装配**：在 `ShijimaManager`、runtime/ui state、窗口 setup、lifecycle
   中持有 client，保证 GUI 线程、关闭顺序、设置开关和连接信号边界。
3. **Codex 页面/设置/气泡**：新增 `ManagerCodexPage`，更新导航、设置、托盘徽标、
   `SpeechBubbleWidget`；不把审批按钮放入 tooltip 气泡。
4. **测试**：恢复小型专项目标 `NeurolingsCECodexTests`（Qt Core/Network/Widgets，
   Linux CI 使用 `QT_QPA_PLATFORM=offscreen`），不恢复已经删除的旧大型测试目标。
5. **文档与 CI**：同步本计划、阶段一 companion 文档、core/runtime/interface/widgets/
   tests/app README、贡献技能和 Debug workflow；Release workflow 仅正式构建。

## 自动测试验收矩阵

### 协议与生命周期

- [ ] initialize → initialized 顺序及 `collaborationMode/list` preset 选择。
- [ ] thread start/resume、turn start/steer/interrupt 序列化。
- [ ] 字符串/整数 request ID 原样往返；半包、多行、LF/CRLF、EOF 半行。
- [ ] 非法 JSON、非 object、result/error 冲突、4 MiB 行/8 MiB 缓冲超限。
- [ ] 未知 notification 忽略；未知 server request 返回 `-32601`。
- [ ] 异常退出/protocol violation 进入 `Blocked`，不自动重启。

### 审批与澄清

- [ ] command、fileChange、networkContext 解析，`item/started` 乱序合并。
- [ ] 四种 decision 精确序列化、availableDecisions 过滤、无永久策略按钮。
- [ ] 同 itemId 不同 thread/turn 不串单；重复/迟到/旧连接点击只响应一次。
- [ ] serverRequest/resolved、turn/thread 完成和断开按作用域清理。
- [ ] 关闭应用 best-effort cancel，绝不 accept；UI 初始焦点不在允许按钮。
- [ ] requestUserInput 的选项、Other、Secret 和空答案关闭流程。

### Plan、文本与回归

- [ ] 多个 plan delta 顺序聚合；最终 plan item 覆盖 delta；步骤更新不覆盖最终文本。
- [ ] commentary/reasoning 不进入最终气泡；100 个 delta 不产生 100 条气泡；同 item 一条。
- [ ] app-server/notify 同 turn 去重；中英文、Markdown、CRLF、组合字符、ZWJ emoji 安全。
- [ ] 现有 360×240、8 行、8–12 秒 Codex 和普通 3 秒规则不回归。

## CI 与手工验收

Debug workflow 在 Windows、Linux、macOS 三平台显式构建 GUI、CLI 和
`NeurolingsCECodexTests`，随后运行 `ctest --output-on-failure`；Linux 测试环境设置
`QT_QPA_PLATFORM=offscreen`。显式 target 名称须与 CMake/CTest 注册一致，失败不得
用 `continue-on-error` 隐藏。Release workflow 维持正式打包，不重复运行完整测试。

本地 Windows 构建沿用：

```powershell
src\tools\build-windows-ninja.cmd build Debug
ctest --test-dir build -C Debug --output-on-failure
```

真实 app-server 手工流程：显式连接前确认无进程；新建 Default/Plan turn；回答一次
requestUserInput；修改计划；按计划实施切到 Default；分别触发 command、fileChange、
network 审批并验证四种决定；同时产生多个 pending 确认不串单；退出应用确认审批
取消、app-server 终止且 runtime/桌宠不会重新拉起。

## 交付门槛与回滚

交付前必须满足：没有自动批准路径；所有响应回显原始 JSON-RPC ID；乱序、重复、断线
和关闭不执行迟到允许；最终 Plan 覆盖 delta；流式更新不频繁唤醒桌宠；无桌宠时只
更新页面；关闭后不启动 runtime/app-server/Codex 消息桌宠；阶段一 notify、CLI/IPC、
普通气泡和 Codex 配置托管兼容。

任何阶段都必须保持 `codex/appServerEnabled=false` 时不启动新功能。若 app-server
不可用或协议不兼容，用户可关闭开关回到阶段一 notify；不回退或改写用户已有的
notify 配置。

## 当前工作树审查（2026-08-13）

截至 2026-08-13，协议模型、JSON-RPC 序列化/解析、QProcess client、approval
store、GUI Manager 装配、Codex 页面/设置、requestUserInput 控件、气泡提醒和
`NeurolingsCECodexTests` 目标已进入工作树。当前静态核对结果：

- JSON-RPC request/notification 要求 object `params`，response 约束互斥的
  `result`/`error` 且保留原始 ID；
- Plan delta 按 `(threadId, turnId, itemId)` 分桶，最终 item 覆盖 delta；
- `item/started` 与 file-change approval 可乱序合并，管理员限制在创建 thread 前
  检查，切换 thread 会 unsubscribe 并清理旧 callbacks；
- `appServerEnabled=false` 不启动进程，关闭先 cancel/terminate，旧连接 signal
  通过 sender/generation 隔离；
- Debug workflow 显式构建 GUI、CLI、专项测试并运行 CTest，Release 不重复全测。

进一步对照当前实现仍需保留以下风险：

- 真实 app-server 版本可能返回不同的 `collaborationMode/list` 或
  `configRequirements/read` 字段形状；客户端会保持保守的 Plan 禁用/策略阻断，需在
  目标版本上验证字段映射。
- file-change 的 `item/started` 乱序合并按 thread/turn/item 组合键工作；若服务端省略
  任一作用域字段，页面只能显示已收到的部分变更，不能推断或补全路径。
- UI 发送 API 仍是异步 `void` 调用；进程断开时按钮被禁用，但服务端拒绝响应不会自动
  重试，也不会持久化消息，用户可在页面中手动重发。
- 本地 Windows 已用仓库统一脚本完成 Debug GUI/CLI/专项测试构建并通过 CTest；
  Linux/macOS 需由 Debug CI 矩阵继续证明，不能以单平台结果替代。
