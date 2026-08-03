# commands 命令协议与业务层

commands 是 CLI、QLocalSocket、HTTP 和 GUI 共用的协议边界。MascotApi 定义 JSON 数据结构，MascotCommandDispatcher 处理通用校验和路由，MascotCommandService 在 GUI 线程调用 ShijimaManager；CodexActivity 负责受限解析外部通知。

## 主流程

传输层得到一个 JSON object → MascotApi 解析请求 → Dispatcher 校验 command/字段 → Service 在 onTickSync 中读取或改变 Manager → 序列化统一响应。

## 文件说明

| 文件 | 作用 |
|---|---|
| MascotApi.cc | MascotInfo、LoadedMascotInfo、MascotPatch、SpawnMascotRequest 等请求/响应的 JSON 序列化、解析和 anchor 校验。 |
| MascotCommandDispatcher.hpp | header-only 的命令路由；覆盖 ping、列表、加载、spawn、label、alter、dismiss、import/remove、stop、show manager 和 Codex 通知，并生成 bad request。 |
| MascotCommandService.cc | Manager 业务适配层；实现模板/会话查询、选择器、CLI 标签、生成/关闭/修改 mascot、导入删除、预览和 Codex 通知。 |
| CodexActivity.cc | 对受限 JSON 做事件识别（完成回合和新会话标题）、标题/描述提取、换行规范化、grapheme 安全的 prefix-first 摘要和长度截断；未知事件可被安全忽略。 |

## 安全与线程

- 请求体、Codex 文本、selector 和脚本评估都有上限；不要在新命令中接受无限长度字符串。
- Service 的 Manager 访问必须通过 GUI 线程同步入口，避免从 IPC/HTTP worker 直接操作 QWidget 或引擎状态。
- selector 匹配、label 解析和 anchor 必须使用现有规范；CLI、HTTP、localipc 不应各自实现一套。
- 修改 JSON 字段时同步更新 cli/README.md、localipc/README.md、http/README.md 和测试。
