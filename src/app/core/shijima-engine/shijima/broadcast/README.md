# broadcast mascot 交互广播

broadcast 让 mascot 在共享环境中发现附近的 server、匹配 interaction，并在到达条件满足时交换状态。它不依赖 Qt 网络；runtime 只提供环境和 tick 时机。

## 文件说明

| 文件 | 作用 |
|---|---|
| client.hpp / client.cc | mascot 侧广播 client；查找可用 server、请求交互并跟踪到达/完成状态。 |
| server.hpp / server.cc | 提供可被附近 mascot 匹配的 server 行为和交互入口。 |
| server_state.hpp | server 的共享状态、可用性和生命周期数据。 |
| interaction.hpp / interaction.cc | 描述 client/server 的匹配、距离/位置条件和交互结果。 |
| manager.hpp / manager.cc | 管理当前环境中的广播对象、注册/注销、扫描和 tick 更新。 |

广播对象由 mascot manager/action 创建和销毁；不应在 broadcast 层直接改变 QWidget。修改匹配或可见性规则时要同时查看 environment 和 action/interact。
