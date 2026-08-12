# 应用层验证目录

# 应用层专项测试

历史 `NeurolingsCETests` 和 `NeurolingsCEBubbleTests` 生成目标仍不恢复。Codex
app-server 阶段使用一个边界清晰的小型目标 `NeurolingsCECodexTests`，仅覆盖
协议 framing/JSON-RPC envelope、原始 request ID、approval/input/Plan reducer、
连接代数和关闭时 fail-closed cancel；不依赖正在运行的 GUI 或真实 Codex 进程。

当前 `CodexAppServerTests.cc` 覆盖字符串/整数 ID、冲突 response、错误 envelope、
四种 decision 序列化、approval/network/requestUserInput/Plan 解析和 16 条 approval
store 上限及最早请求顺序。QProcess 端仍保持边界约束，真实 app-server 版本形状、
跨平台矩阵和 UI 焦点/气泡交互由 Debug CI 与后续手工验收继续覆盖。

目标链接 Qt Core/Network/Widgets，并注册到 CTest。Linux CI 运行时设置
`QT_QPA_PLATFORM=offscreen`；Windows/macOS 使用各自 Qt runner 的默认测试平台。
Debug workflow 必须显式构建 `NeurolingsCE`、`NeurolingsCECli` 和
`NeurolingsCECodexTests`，随后运行：

```text
ctest --test-dir build --output-on-failure
```

测试不得记录用户消息、命令、cwd、diff、审批理由或 Plan 正文；遇到未知 server
request 应断言 `-32601`，协议/大小错误应断言 `Blocked` 或确定性错误，重复/迟到
审批点击只能产生一次响应。Release 工作流只负责正式构建和打包，不重复完整测试。
