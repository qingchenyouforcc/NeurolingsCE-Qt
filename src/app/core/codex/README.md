# codex 配置与 app-server 协议

本目录维护两条明确分开的 Codex 边界：阶段一的 notify 配置托管，以及阶段二由
用户显式连接的 app-server stdio JSONL 客户端。notify 配置只改写用户选择托管的
配置片段，并尽量保存既有 notify 命令；app-server 解析 JSON-RPC、管理一个活动
thread、审批/澄清请求和 Plan reducer，但不负责主窗口布局或气泡排版。

## 主流程

阶段一：读取 CODEX_HOME/config.toml → 定位 managed begin/end 标记 → 检查冲突或读取备份标记 → 用 QSaveFile 原子写入并生成备份 → 启用、禁用、恢复或测试 notify 命令。

阶段二：用户点击连接 → `QProcess(codex app-server --listen stdio://)` →
`initialize`/`initialized` → `collaborationMode/list` 与
`configRequirements/read` → `thread/start`/`thread/resume` → `turn/start`、
`turn/steer` 或 `turn/interrupt`。stdout 只接受有界 UTF-8 JSONL；stderr 只保留
限长诊断。解析/状态更新在 client 内完成，页面通过 queued signal 消费结构化状态。

## 文件说明

| 文件 | 作用 |
|---|---|
| CodexConfigManager.cc | 计算配置路径，读取/写入托管块，保存原有命令的 base64 标记，创建时间戳备份，处理冲突并恢复/移除配置。 |
| CodexAppServerProtocol.cc | 解析/序列化有界 JSON-RPC envelope、approval、requestUserInput 和 Plan 事件；保留原始 request ID，未知 server request 返回 `-32601`。 |
| CodexAppServerClient.cc | GUI 线程拥有的 QProcess 生命周期、JSONL framing、请求关联、连接代数、审批/input store、Plan reducer、关闭时 cancel 和 fail-closed 状态。 |

## Agent 注意点

- 默认配置目录来自 CODEX_HOME，否则是用户的 .codex；不要把仓库目录当成配置目录。
- 禁用时只能删除由本程序托管的精确片段，发现非托管冲突应报告错误而不是静默覆盖。
- 文件写入使用 QSaveFile；修改配置逻辑后重点检查备份、重复启用、恢复旧命令和并发失败路径。
- CodexActivity 的事件识别在 core/commands，不要把配置文本解析和事件 JSON 解析混在一起。
- app-server 不通过 shell 拼接命令，不自动重启，不观察其他 Codex 会话；`appServerEnabled`
  默认关闭，只有显式连接动作才启动进程。
- stdout 单行上限 4 MiB、未完成缓冲上限 8 MiB；超限、非法 envelope、断线或未知
  approval 类型都 fail-closed。pending approval 最多 16 条，重复/迟到/旧连接按钮
  必须拒绝；退出顺序为 best-effort cancel → 清空内存状态 → terminate/kill。
- 不记录或持久化用户输入、助手正文、命令、cwd、diff、理由或 Plan 正文；
  `acceptForSession` 仅限当前内存连接，不能写 QSettings。
