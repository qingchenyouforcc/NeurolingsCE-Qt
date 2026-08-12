# src/app 代码导航

这里是 NeurolingsCE 的应用层实现。代码可以按“入口 → 运行时 → 核心服务 → 引擎 → UI”阅读：

1. main.cc 或 cli_main.cc 创建 Qt 应用、初始化日志和平台能力。
2. ShijimaManager 在 runtime 与 ui 中建立窗口、屏幕环境、模板库和 mascot 会话。
3. core 提供资产/包、命令协议、Codex 集成、IPC/HTTP、声音和更新等边界服务。
4. core/shijima-engine 解析 actions.xml、behaviors.xml，并在每个 tick 中推进动作、行为和广播。
5. ui 将引擎状态转换成 QWidget、托盘、对话框、气泡和用户操作。

## 目录索引

| 目录 | 作用 | 入口文档 |
|---|---|---|
| cli | CLI 参数解析、IPC/独立运行时执行、文本/JSON 输出 | cli/README.md |
| core | 应用核心服务和跨层数据契约 | core/README.md |
| runtime | ShijimaManager 的生命周期、屏幕环境、模板和会话 | runtime/README.md |
| ui | 主窗口、页面、托盘、mascot 控件、菜单和对话框 | ui/README.md |

core 下的引擎目录另有一组更细的导航：

- core/assets：资产加载、裁剪、包导入和安全路径。
- core/audio、core/codex、core/commands、core/http、core/localipc、
  core/mascotstore、core/github、core/submission、core/update：应用边界服务。
- core/shijima-engine：内置 Shijima 模拟引擎；其 action、behavior、broadcast、mascot、scripting 子目录分别说明各自状态机。

## 最重要的数据流

### GUI 启动和 tick

main.cc → ManagerWindowSetup.cc → ManagerLifecycle.cc / ManagerMascotRuntime.cc →
ManagerEnvironmentSync.cc → ShijimaWidget → shijima::mascot::manager。

窗口设置阶段装配依赖并加载模板；生命周期阶段启动 40 ms 的 Qt timer；每个 tick 先更新屏幕环境，再反向遍历 mascot 会话，推进引擎、处理繁殖/自毁、刷新窗口和气泡。

### 外部命令

NeurolingsCE-cli → cli/CommandLineParser.cc → cli/CommandExecutor.cc →
本地 JSONL IPC 或独立运行时 → core/commands/MascotCommandDispatcher.hpp →
MascotCommandService → ShijimaManager。

GUI 运行时还可以通过 core/http 提供 HTTP API。命令协议的 JSON 结构由 core/commands/MascotApi.cc 负责，不要在 CLI、IPC、HTTP 三处各自发明字段。

### Codex 通知与 app-server

Codex 命令或通知脚本 → CodexActivity → MascotCommandService →
ManagerMascotRuntime → ShijimaWidget / SpeechBubbleWidget。

消息长度、事件识别、模板选择和气泡排版分别在 commands、runtime、ui/widgets 中完成。

阶段二 app-server 是独立的 GUI 线程链路：

```text
连接按钮 → CodexAppServerClient/QProcess
  → CodexAppServerProtocol（有界 JSONL/JSON-RPC）
  → ManagerCodexPage（approval、requestUserInput、Plan/reply）
  → 最终摘要 → SpeechBubbleWidget（仅提醒/完成气泡）
```

它只管理 NeurolingsCE 自己创建或显式恢复的一个 thread，不经过 CLI、HTTP 或阶段一
notify，不旁听其他 Codex 会话。`appServerEnabled=false` 时不启动进程；未知 server
request、协议超限、断线和关闭均 fail-closed，pending approval 先 cancel 再终止。

### 测试与翻译边界

阶段二协议与状态回归位于 `src/app/tests/` 的
`NeurolingsCECodexTests`，由 Debug CI 在 Windows/Linux/macOS 构建并运行 CTest；
Linux 使用 `QT_QPA_PLATFORM=offscreen`。Release workflow 保持正式构建，不重复完整
测试。旧的 `NeurolingsCETests` 与 `NeurolingsCEBubbleTests` 不恢复。

GUI 自有文案维护在 `translations/shijima-qt_zh_CN.ts`，构建时由
Qt LinguistTools 编译并嵌入；Qt 标准控件文案从 Qt 安装目录的
`TranslationsPath` 加载。`main.cc` 只为重复实例和早期启动错误短暂安装
bootstrap translator，正常窗口由 Manager 持有 translator。切换语言仍采用重启语义，
以确保一次启动内所有页面使用同一套翻译。CLI 输出、JSON/IPC/HTTP 协议字段、错误代码、日志和包格式
常量是稳定英文接口，不应本地化；GUI 可以只把已知错误代码映射为本地化说明，
同时保留服务器或 Qt 返回的动态详情供诊断。

## Agent 阅读规则

- 先读本文件，再读目标文件所在目录最近的 README.md；需要跨层修改时同时读调用方和被调用方的 README。
- README 的文件表覆盖该目录中的每个源码、头文件和重要 vendored 文件；文件移动、重命名或职责明显变化时同步更新最近的 README。
- README 是导航和当前实现的摘要，不替代源码中的 API/线程安全契约；涉及安全边界、线程切换、生命周期或 JSON 字段时必须回到实现和公共头文件核对。
- core/shijima-engine/rapidxml 与 scripting/duktape 是第三方/嵌入式实现，只做必要集成修改；优先阅读父目录说明，不要对 vendored 文件做无关格式化。

## 本目录文件

| 文件 | 作用 |
|---|---|
| main.cc | GUI 与 CLI runtime 的主入口；负责 Qt 应用、平台、日志、单实例和 Manager 生命周期。 |
| cli.cc | Unicode CLI 参数转换、CLI 模式判定以及 parse → execute → format 总调度。 |
| cli_main.cc | 独立 NeurolingsCE-cli 进程入口；建立 QCoreApplication 后调用 cli.cc。 |
