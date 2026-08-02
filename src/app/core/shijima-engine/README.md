# shijima-engine 内置模拟引擎

这是项目内集成的 libshijima 风格 C++17 引擎。它不创建 Qt 窗口，也不负责 mascot 包导入；它读取 core/assets 准备好的 actions.xml/behaviors.xml 和图像名称，在独立的环境抽象中推进行为、动作、广播和状态。

上游/历史说明仍保留在 README.libshijima.md，许可证在 LICENSE.libshijima.txt。本 README 说明 NeurolingsCE 当前如何接入它。

## 集成调用链

core/assets/MascotData → shijima/parser → shijima/mascot/factory 注册模板 →
shijima/mascot/manager 为会话创建状态和 behavior manager →
behavior/list/manager 选择行为 → action/* 推进动作 →
mascot/state + environment 更新位置/屏幕/广播 →
runtime/ManagerMascotRuntime → ui/mascot 绘制。

每个 Qt timer tick 还会被拆成固定 subtick，动作在 tick context 中运行；脚本条件和变量通过 scripting/context 访问环境与状态。

## 子目录

| 子目录 | 作用 | 入口文档 |
|---|---|---|
| rapidxml | 嵌入式 RapidXML 头文件和打印/工具辅助；第三方代码 | rapidxml/README.md |
| shijima | 引擎公共类型、XML parser、动作/行为/广播/mascot/scripting | shijima/README.md |

## 本目录文件

| 文件 | 作用 |
|---|---|
| README.libshijima.md | 原有 libshijima 概览和示例说明。 |
| LICENSE.libshijima.txt | 引擎许可证文本。 |

## 修改边界

- 引擎核心不应依赖 QWidget、QLocalSocket、HTTP 或 QSettings；这些适配在 runtime/core/ui。
- 解析 XML、运行脚本、archive 和图像均可能接收外部数据；保留调用方的大小、超时和路径限制。
- action 的完成/失败/下一动作语义由 base 和 manager 管理；新增动作要同时考虑 init、tick、subtick、finalize 和变量/广播生命周期。
- rapidxml、duktape 是嵌入/第三方实现，除必要编译或适配外不要格式化或重写。
