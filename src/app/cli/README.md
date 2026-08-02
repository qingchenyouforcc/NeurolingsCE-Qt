# CLI 模块

CLI 模块把用户命令转换成统一的 MascotCommandService 请求，并在必要时启动或连接 GUI runtime。它支持文档化命令（list、summon、close 等）和旧版兼容命令，输出可选人类文本或单行 JSON。

## 主流程

CommandLineParser → CliCommand → CommandExecutor → 本地 JSONL IPC / 独立 runtime → CliExecutionResult → OutputFormatter。

命令执行前会确认 runtime 可用；若没有 GUI 进程，则寻找 NeurolingsCE.exe 或 NeurolingsCE-cli.exe 相关候选并以静默 runtime 启动。独立模式下模板库、loaded mascot 和 CLI 标签仍需保持与 GUI 协议相同的语义。

## 文件说明

| 文件 | 作用 |
|---|---|
| InternalCli.hpp | 定义命令种类、全局选项、解析结果、错误和执行结果等 CLI 内部契约。 |
| CommandLineParser.cc | 识别文档化/legacy 命令，解析全局选项、整数/浮点/anchor/label，并做参数数量与互斥校验。 |
| CommandExecutor.cc | 将 CliCommand 映射到 IPC JSON 请求；负责 runtime 启动等待、响应解析、模板导入/删除和旧版标签兼容。 |
| OutputFormatter.cc | 生成 help、版本、列表、命令成功/失败的文本或 JSON；保持 JSON 模式一行一个对象。 |

## 修改时的注意点

- Parser 只负责语法和本地字段校验，业务状态和权限由 MascotCommandService/Dispatcher 决定。
- 文档命令和 legacy 命令最终应归一到同一个 API 请求，修改字段时先检查 core/commands/README.md。
- CLI 与 GUI 通过本地 IPC 传输，超时、1 MiB 限制和 JSONL 边界不能被输出层绕过。
- Windows 参数必须经过 cli.cc 的 Unicode 转换，避免直接使用窄字符 argv 破坏中文或 emoji。
