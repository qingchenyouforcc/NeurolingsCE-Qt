# scripting 脚本与变量

scripting 把 XML 中的条件/变量和受限 Duktape JavaScript 连接到引擎。脚本可以读取/改变允许暴露的 mascot/environment 数据，但不应获得文件系统、网络或 Qt 对象访问。

## 文件说明

| 文件 | 作用 |
|---|---|
| condition.hpp | 表示常量或包含 dollar-brace/hash-brace 占位符的条件表达式，并声明求值入口。 |
| context.hpp / context.cc | 创建 Duktape context，注册 mascot/environment/console bridge，提供 bool/number/string/JSON 求值和执行 deadline。 |
| variables.hpp / variables.cc | 管理 XML 属性与 JS globals 的动态变量，提供类型化读写、默认值和生命周期清理。 |

## 执行模型

parser 生成条件/变量 → action/behavior 请求 context 求值 → context 在 deadline 内运行 JS → 返回值参与行为选择或动作更新。脚本超时/异常必须回到安全失败路径，不能阻塞 GUI tick。

duktape 子目录是嵌入式引擎实现，详见 duktape/README.md。
