# mascot 引擎运行时

mascot 子目录保存一只运行中 mascot 以及它所在环境的无 UI 状态。factory 管理模板注册和产品创建，manager 管理行为/action 生命周期，state 保存位置/速度/朝向等可变数据，environment 提供屏幕/边界/光标/广播输入。

## 文件说明

| 文件 | 作用 |
|---|---|
| environment.hpp | 环境抽象：屏幕几何、work area、floor/ceiling、cursor、active window、scale、随机数和 broadcast manager。 |
| state.hpp / state.cc | mascot 可变状态：位置、速度、朝向、当前 pose、落地状态、光标平滑和 on_land 等状态转换。 |
| tick.hpp | tick/subtick 上下文、初始化信息和本次 tick 的状态覆盖/临时数据。 |
| manager.hpp / manager.cc | 绑定 behavior/action、推进生命周期、reset/detach/hotspot、临时 next 行为预选及恢复、预后 tick 和当前状态查询。 |
| factory.hpp / factory.cc | 注册/注销模板，依据模板生成 mascot product，处理 breeding/实例创建。 |

## 与应用层的边界

factory 的模板注册由 runtime/MascotTemplateStore 管理；manager 的 tick 由 runtime/ManagerMascotRuntime 驱动；ShijimaWidget 读取 pose/state 并负责实际窗口。引擎只返回状态和请求，不操作 Qt 对象。

修改位置、尺度或屏幕规则时优先从 environment/state 进入，再检查 runtime/ManagerEnvironmentController；修改创建/删除时检查 factory、SessionStore 和生命周期顺序。
