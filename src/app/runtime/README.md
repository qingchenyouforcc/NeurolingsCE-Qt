# runtime 运行时编排

runtime 是 ShijimaManager 的实现核心，负责把模板库、mascot 会话、屏幕环境、导入任务、Qt timer 和关闭流程组织起来。它不定义具体页面布局，也不实现引擎动作；它把 core 和 ui 接到 libshijima engine。

## 主流程

ManagerWindowSetup → 创建 ManagerRuntimeState、模板/会话 store 和屏幕环境 →
ManagerLifecycle 启动/停止 timer → 每 40 ms 进入 ManagerMascotRuntime tick →
ManagerEnvironmentSync 更新环境 → MascotSessionStore 反向遍历 ShijimaWidget →
引擎推进、繁殖/自毁/刷新 → UI 状态和托盘更新。

导入走 ManagerImportWorkflow；Codex 通知和 CLI/IPC 命令最终通过 MascotCommandService 回到这里。

## 文件说明

| 文件 | 作用 |
|---|---|
| ManagerRuntimeState.hpp | 保存运行时组合状态：环境控制器、timer、路径、模板/会话 store、刷新集合、计数器和关闭/CLI 标志。 |
| ManagerRuntimeHelpers.hpp | 提供引擎 subtick 常量（4）和把回调切到 GUI 线程的通用辅助函数。 |
| ManagerEnvironmentController.hpp | 声明每屏幕 shijima::mascot::environment 的创建、更新、尺度、拖离阈值、繁殖和活动窗口设置。 |
| ManagerEnvironmentController.cc | 实现屏幕几何、工作区/地板/天花板/光标、active window、detach、scale 和多屏环境更新。 |
| ManagerEnvironmentSync.cc | Manager 层包装；增删屏幕、切换 windowed sandbox、重建 mascot widget、恢复 inspector 与位置。 |
| MascotTemplateStore.hpp | 模板所有权、名称/id 兼容索引及引擎 factory 注册接口。 |
| MascotTemplateStore.cc | 同步注册/注销 factory、加载/替换模板、维护 native package 与默认 @ 模板。 |
| MascotSessionStore.hpp | 稳定的 mascot widget 列表、id 映射、待删除标记、CLI label 和临时 label 生命周期。 |
| MascotSessionStore.cc | 创建/查询/删除/遍历会话，按 id/name/label 解析目标，并在 tick 后提交延迟销毁。 |
| ManagerMascotRuntime.cc | 加载/刷新/删除模板，spawn、随机 spawn、Codex 通知、主 tick、繁殖请求和兼容 facade。 |
| ManagerImportWorkflow.cc | 同步或 QtConcurrent 异步导入 mascot 文件、模板目录和 legacy archive，处理窗口拖放、进度对话框、主线程回调和首次显示延迟导入。 |
| ManagerLifecycle.cc | singleton 构造/终止、timer、tray/API 关闭、保存组合、顶层 mascot 清理和 GUI 线程同步入口。 |

## 生命周期与线程边界

- Manager、ShijimaWidget、引擎 manager 和 QSettings 归 GUI 线程所有。
- IPC/HTTP/导入 worker 只能通过 dispatchToMainThread 或 onTickSync 请求 GUI 操作。
- 关闭顺序通常是停止新请求和 timer → 停止 tray/API → 删除 mascot 会话/窗口 → 注销模板和引擎资源 → 释放 UI。
- MascotSessionStore 的延迟销毁是为了避免在当前 tick 遍历中立即 delete；修改删除逻辑必须保留反向遍历和提交阶段。

## 修改导航

- “加载/导入/删除包”：先看 MascotTemplateStore、ManagerMascotRuntime，再看 core/assets 和 ManagerImportWorkflow。
- “屏幕/窗口/拖离”：先看 ManagerEnvironmentController，再看 EnvironmentSync 和 ui/mascot。
- “启动/关闭/单实例”：看 ManagerLifecycle 与 ui/ManagerWindowSetup。
- “CLI/HTTP 改 mascot”：看 core/commands，确认最终调用的是本目录的 GUI 线程路径。
