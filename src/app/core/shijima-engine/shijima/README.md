# shijima 引擎核心类型

shijima 目录是引擎的公共类型和状态机实现。可按 parser → mascot → behavior → action → scripting 的顺序阅读；broadcast、animation、hotspot 是跨层基础设施。

## 文件说明

| 文件 | 作用 |
|---|---|
| shijima.hpp | 引擎公共 umbrella include。 |
| xml_doc.hpp | 为 RapidXML 提供带 backing vector 的文档封装和解析所需内存生命周期。 |
| translator.hpp / translator.cc | 将 legacy XML 标签和属性名翻译成引擎内部术语。 |
| math.hpp | vec2、rect 等几何类型、运算、边界和文本解析。 |
| pose.hpp | 单个 pose 的图像名、镜像、声音、anchor、速度和持续时间。 |
| animation.hpp / animation.cc | pose 序列、持续时间、按时间取 pose 和 hotspot 查询。 |
| hotspot.hpp / hotspot.cc | 圆/矩形等 hotspot 的解析、有效性和点包含判断。 |
| parser.hpp / parser.cc | 读取 XML，构造动作/行为/pose/常量，连接引用并清理解析结果。 |
| config.hpp | 编译配置；非 NDEBUG 构建启用引擎日志相关能力。 |
| log.hpp / log.cc | 引擎日志类别和到应用日志桥接。 |

## 子目录

| 子目录 | 作用 | 入口文档 |
|---|---|---|
| action | 可组合的动作状态机实现 | action/README.md |
| behavior | 行为元数据、候选列表和选择管理 | behavior/README.md |
| broadcast | mascot 之间的交互/广播发现和状态 | broadcast/README.md |
| mascot | 环境、状态、tick、manager 和 factory | mascot/README.md |
| scripting | Duktape 上下文、变量和条件表达式 | scripting/README.md |

## 关键契约

- parser 生成的引用必须在进入 factory 前解析完成；运行时 action 不应重新解析 XML。
- animation/pose 的时间单位与 runtime subtick 相关；改变持续时间时检查 tick.hpp 和 action/animation。
- environment 是无 Qt 的几何/交互抽象；屏幕、窗口、缩放数据由 runtime 填充。
- mascot manager 的临时行为预选必须在交互结束时恢复当前 behavior 的 next-list，保持 `Add` 与 `NextBehaviorList` 的 XML 语义；hotspot 查询必须沿复合 action 的当前子节点转发，覆盖真实 mascot 的 Sequence/ActionReference 包装。
