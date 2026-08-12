# localipc 本地 JSONL IPC

本目录提供 CLI 与 GUI runtime 之间的本机 QLocalSocket 通道。协议是一行一个 JSON object，服务端负责连接、超时、大小限制和线程生命周期，命令业务集中在 core/commands。

## 主流程

CLI client 连接固定 server name → 写入一行 JSON → server 读取到换行或超时 → Dispatcher/Service 处理 → 写回一行 JSON → client 读取并解析。

服务端启动时会先 ping 旧 socket；只有确认是 stale endpoint 才移除，避免误删仍在运行的实例。停止时通过唤醒连接让监听线程退出。

## 文件说明

| 文件 | 作用 |
|---|---|
| ShijimaLocalApi.cc | QLocalServer worker 线程、JSONL 读取/写回、连接超时、1 MiB 限制、stale socket 恢复和 stop 生命周期。 |
| ShijimaLocalApiClient.cc | QLocalSocket 客户端；连接、发送单行 JSON、读取单行响应，并提供 ping/show manager 等快捷调用。 |

## Agent 注意点

- JSONL 的换行是消息边界；不要把多行 pretty JSON 写到 socket。
- IPC 错误应区分无法连接、超时、非法 JSON、业务错误和 runtime 未启动。
- worker 与 GUI 的边界由 Dispatcher/Service 维护；不要从 socket 线程访问 QWidget。
- 安全上限见 include/shijima-qt/SecurityLimits.hpp；新增命令要补 CLI/IPC
  smoke verification and keep the JSON contract documented at the dispatcher boundary。
