# ui 用户界面

ui 把 Manager 和 Shijima 引擎状态呈现为 Qt 窗口、页面、托盘、对话框、右键菜单、mascot 窗口和 speech bubble。UI 应通过 runtime/core 的现有接口改变状态，不应自己维护另一份模板或会话真相。

## 子目录

| 子目录 | 作用 |
|---|---|
| interface | 主窗口导航、主页、创建/转换、组合、设置和关于页面。 |
| mascot | 单只 mascot 的 QWidget 生命周期、渲染和鼠标交互。 |
| menus | mascot 右键菜单和上下文动作。 |
| widgets | speech bubble、Codex Markdown 安全清洗/渲染、文本截断和排版。 |
| dialogs | 强制进度、检查器和许可证对话框。 |

## 本目录文件

| 文件 | 作用 |
|---|---|
| ManagerWindowSetup.cc | 装配 Manager UI/runtime、读取设置、建立屏幕环境、加载模板、启动 timer、tray、IPC/HTTP 和更新检查。 |
| ManagerUiState.hpp | 保存 Manager 的 QWidget、页面、列表、设置控件和 UI 状态指针。 |
| ManagerUiHelpers.hpp | UI action 使用的颜色、主题、列表和主线程辅助函数声明。 |
| ManagerUiActions.cc | 导入/删除/退出、sandbox、语言切换、显示/隐藏、主题、关闭动画和重新翻译等交互。 |
| ManagerTrayController.hpp | 托盘控制器接口和菜单动作的声明。 |
| ManagerTrayIcon.cc | 创建/更新/销毁系统托盘图标，连接显示、随机 spawn、关闭 mascot、退出和通知回调。 |

## 关键 UI 数据流

- ManagerWindowSetup 负责“组装”，ManagerUiActions 负责“用户动作”，runtime 负责“状态改变”。
- ShijimaWidget 从 runtime 会话获得引擎状态；Rendering 只画当前帧，Interaction 只处理输入和上下文动作。
- 任何耗时导入、更新或网络操作应离开 GUI 线程，并通过已有进度/回调回到页面。
- 主题、语言和可访问性改变后，页面和 speech bubble 都要通过既有 retranslate/theme 路径刷新。
