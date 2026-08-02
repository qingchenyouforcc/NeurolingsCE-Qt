# behavior 行为选择

behavior 描述“下一步做什么”，action 描述“这一步怎样执行”。parser 从 XML 构造带名称、频率、隐藏标志、条件和 action 列表的行为对象；manager 根据环境与条件选择候选，再交给 mascot manager 执行。

## 文件说明

| 文件 | 作用 |
|---|---|
| behavior.hpp | behavior 类型和公共入口。 |
| base.hpp / base.cc | 行为元数据、频率/隐藏状态、条件和 action 引用的基础对象。 |
| list.hpp / list.cc | 维护行为列表，展开 conditional/unconditional 候选并处理 next 行为关系。 |
| manager.hpp / manager.cc | 按频率、条件和当前状态选择下一个行为，负责切换和重置。 |

行为选择发生在引擎 tick 中，脚本条件异常或超时应按安全失败处理；修改候选排序/随机策略时同步检查测试和重放行为。
