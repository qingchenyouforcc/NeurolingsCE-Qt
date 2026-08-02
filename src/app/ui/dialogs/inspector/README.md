# dialogs/inspector mascot 检查器

Inspector 是只读诊断窗口，帮助开发者和用户查看一只 mascot 当前的窗口、引擎动作、anchor、图像和屏幕环境。它不应成为修改运行时状态的第二入口。

## 文件说明

| 文件 | 作用 |
|---|---|
| ShimejiInspectorDialog.cc | 创建检查器窗口、分组和刷新生命周期，绑定当前 ShijimaWidget。 |
| ShimejiInspectorRows.cc | 生成 Window、Anchor、Cursor、Behavior、Image、Screen、Work Area、Active IE 等实时行。 |
| ShimejiInspectorFormatting.hpp | 数值、矩形、anchor、速度和状态的格式化辅助声明。 |

刷新数据来自引擎/runtime 当前快照；如果增加字段，先确认公共数据是否已暴露，避免在 inspector 中直接访问私有状态。
