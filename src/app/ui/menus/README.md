# menus 上下文菜单

menus 为单只 mascot 生成右键菜单，把引擎中的可用行为和应用级控制暴露给用户。菜单本身不拥有 mascot，也不直接实现关闭/暂停的业务逻辑。

## 文件说明

| 文件 | 作用 |
|---|---|
| ShijimaContextMenu.cc | 组装行为、暂停、管理、检查、关闭等菜单项，连接关闭回调并处理菜单生命周期。 |
| ContextMenuActions.cc | 将菜单动作映射到 behavior manager、pause/call、显示 Manager、打开 inspector 和 dismiss。 |

菜单动作通常回到 ShijimaWidget 或 ShijimaManager；如果新增动作涉及线程、销毁或持久化，应先看 runtime/README.md。
