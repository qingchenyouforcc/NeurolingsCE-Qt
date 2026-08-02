# core 核心服务

core 是应用层的跨 UI、runtime、CLI 共享服务。它不负责主窗口布局，也不直接决定每只 mascot 的动作；它负责把文件、包、协议、外部进程和配置安全地接入 Manager 与引擎。

## 子目录

| 子目录 | 作用 |
|---|---|
| assets | PNG/音频/包文件加载、默认资源、路径安全和 legacy 导入。 |
| audio | 可选 Qt Multimedia 声音播放；无该组件时提供兼容 no-op。 |
| codex | 管理 Codex notify 配置块、备份和恢复。 |
| commands | JSON API 类型、Codex 活动解析、命令分发和 Manager 业务服务。 |
| http | 基于 cpp-httplib 的 HTTP API 线程服务。 |
| localipc | 本机 QLocalSocket JSONL 服务端和客户端。 |
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
