# action 动作状态机

action 是 mascot 的最小执行单元。行为选择器提供一个 action 根节点，base 负责统一生命周期，animation 负责常见 pose/速度推进，复合 action 负责把多个子动作组合起来；每个具体动作在 tick/subtick 中更新状态并返回完成/继续/失败语义。

## 继承与组合关系

base → animation → animate 等动画动作；
base → instant、reference、sequence、select 等控制动作；
专用动作通过 mascot state/environment、broadcast 或 factory 产生移动、拖拽、繁殖和销毁效果。

## 文件说明

| 文件 | 作用 |
|---|---|
| action.hpp | action 类型的统一入口和公共声明。 |
| base.hpp / base.cc | 所有动作的基类生命周期：init、tick、subtick、finalize，以及变量、脚本和 broadcast 访问。 |
| animation.hpp / animation.cc | 共享 pose 动画、持续时间、速度、边界、拖拽和完成条件的基础实现。 |
| animate.hpp / animate.cc | 使用动画序列推进 pose 的具体动作变体。 |
| breed.hpp / breed.cc | 发起繁殖请求并等待 runtime/factory 处理。 |
| dragged.hpp / dragged.cc | 在用户拖拽期间跟随外部位置，处理拖拽结束后的恢复。 |
| fall.hpp / fall.cc | 应用重力/下落 subtick，直到落地或被边界处理。 |
| instant.hpp / instant.cc | 立即执行一次效果并完成的动作包装。 |
| interact.hpp / interact.cc | 查找并发起附近 mascot 的广播/交互。 |
| jump.hpp / jump.cc | 设置跳跃速度和动画，执行腾空到落地的过程。 |
| look.hpp / look.cc | 调整朝向/观察目标相关状态或 pose。 |
| move.hpp / move.cc | 依据速度、目标或环境边界移动 mascot。 |
| movewithturn.hpp / movewithturn.cc | 在移动的同时根据方向切换朝向。 |
| offset.hpp / offset.cc | 在当前位置/anchor 上施加一次或持续偏移。 |
| reference.hpp / reference.cc | 延迟解析并执行另一个已注册 action 的引用。 |
| resist.hpp / resist.cc | 处理外部移动/拖拽等力量的阻力或恢复。 |
| scanmove.hpp / scanmove.cc | 扫描可行边界/表面并沿环境移动。 |
| select.hpp / select.cc | 从候选子动作中按条件或随机选择一个执行。 |
| selfdestruct.hpp / selfdestruct.cc | 标记当前 mascot 自毁并让 runtime 在 tick 后删除。 |
| sequence.hpp / sequence.cc | 按顺序运行多个子动作，转发生命周期和完成状态。 |
| stay.hpp / stay.cc | 保持当前位置/pose 一段时间或直到条件完成。 |
| transform.hpp / transform.cc | 触发 mascot 模板/外观变换相关动作。 |
| turn.hpp / turn.cc | 改变朝向或转向目标。 |

## 修改时的注意点

- action 不能直接 delete widget；selfdestruct 只发出引擎状态，runtime 在安全遍历阶段处理。
- 任何会跨多个 subtick 的动作都要正确实现暂停、拖拽、reset 和 finalize。
- 复合 action 的子节点所有权/引用关系由 parser 和 manager 管理；不要在 tick 中重复释放。
- 脚本条件和变量有执行预算；action 新增脚本入口时沿用 scripting/context 的限制。
