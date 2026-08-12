# core 核心服务

core 是应用层的跨 UI、runtime、CLI 共享服务。它不负责主窗口布局，也不直接决定每只 mascot 的动作；它负责把文件、包、协议、外部进程和配置安全地接入 Manager 与引擎。

## 子目录

| 子目录 | 作用 |
|---|---|
| assets | PNG/音频/包文件加载、默认资源、路径安全和 legacy 导入。 |
| audio | 可选 Qt Multimedia 声音播放；无该组件时提供兼容 no-op。 |
| codex | 管理 Codex notify 配置块、app-server JSONL/JSON-RPC 客户端、审批和 Plan 数据。 |
| commands | JSON API 类型、Codex 活动解析、命令分发和 Manager 业务服务。 |
| github | GitHub App Device Flow 登录与平台安全凭据存储。 |
| http | 基于 cpp-httplib 的 HTTP API 线程服务。 |
| localipc | 本机 QLocalSocket JSONL 服务端和客户端。 |
| mascotstore | Mascot 商店索引/缓存/网络/下载/安装协调核心。 |
| submission | 投稿上传客户端（multipart、取消、结构化错误）。 |
| shijima-engine | 内置的 XML/JS 驱动 mascot 模拟引擎。 |
| update | GitHub release 更新检查、下载和安装准备。 |

## 本目录文件

| 文件 | 作用 |
|---|---|
| AppLog.cc | 初始化按会话分文件的日志、Qt message handler、级别过滤、崩溃记录和线程安全输出。公共声明在 include/shijima-qt/AppLog.hpp。 |

## 跨模块边界

- core/commands 定义的 API JSON 是 CLI、localipc、HTTP 共用的协议；修改时同时检查三种传输层。
- core/assets 是不可信包和文件系统的安全边界；所有 archive entry、相对路径、图像尺寸和文件大小都要经过限制。
- localipc/http 只负责传输与请求生命周期，实际业务应留在 MascotCommandService。
- 引擎的环境和生命周期由 runtime 提供，UI 通过 ShijimaWidget 消费引擎帧，不要让 core 服务直接依赖 QWidget。
- Codex notify 与 app-server 是两个独立协议边界：阶段一的未知 notify 事件静默
  成功，阶段二的未知 app-server server request 必须回 `-32601`，不能共用一个
  “忽略未知事件”的解析器。
- `CodexAppServerClient` 只由 GUI 线程拥有，通过 `QProcess` 的 stdout 解析有界
  UTF-8 JSONL，stderr 仅用于限长诊断。request ID 保留原始字符串/安全整数；
  approval、requestUserInput、Plan 和回复状态只保存在内存，关闭时先 cancel 再
  terminate/kill，绝不自动批准或自动重启。
