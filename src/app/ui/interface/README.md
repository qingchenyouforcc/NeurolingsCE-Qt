# interface 主窗口页面

interface 目录实现 Manager 主窗口的页面层。Navigation 负责页面切换，其余页面负责自己的控件、设置和信号；模板/会话实际操作通过 Manager 的公共接口交给 runtime。

## 文件说明

| 文件 | 作用 |
|---|---|
| ManagerNavigation.cc | 建立 Home、Create、Combinations、Codex、Settings、About 页面并处理侧栏导航。 |
| ManagerCodexPage.cc | Codex app-server 单活动 thread 页面：滚动卡片式连接/恢复、ElaComboBox 模式选择、turn 控制、工作区与短标识状态、Plan/回复只读区、审批与 requestUserInput 卡片及 pending 导航徽标；ElaScrollArea/ElaScrollBar/ElaPlainTextEdit/ElaRadioButton 和主题色、键盘焦点、空状态跟随 ElaWidgetTools。 |
| ManagerHomePage.cc | mascot library 主页：列表/详情、spawn/random、导入、刷新、打开目录、响应式布局和主题；页面表面/标签跟随 ElaThemeColor，保留 QListWidget 的键盘选择和滚动条主题。 |
| ManagerStorePage.cc | 商店目录浏览与筛选、ElaComboBox 标签选择和 ElaScrollBar 列表滚动、配置诊断/缓存清理、状态横幅、详情、下载/安装/取消操作、GitHub Device Flow 登录和提交入口；通过 MascotStoreCoordinator 消费核心状态。 |
| MascotStoreUi.hpp | 商店页面 QWidget 指针和可见状态（筛选、空状态、状态横幅、下载进度、索引刷新、登录配置）的集中所有权描述。 |
| ManagerCreatePage.cc | legacy Shimeji archive 检查/转换：ElaScrollArea/ElaScrollBar 滚动容器、ElaPlainTextEdit 元数据/结果编辑器、异步分析、候选选择、info.json 编辑、校验和选中项导入。 |
| ManagerCombinationsPage.cc | 在 QSettings 中保存当前 mascot 数量组合，展示上次关闭/已保存组合，并恢复或删除组合；面板、列表选中/禁用状态和内嵌滚动条使用 ElaThemeColor 并在 `themeModeChanged` 即时刷新。 |
| ManagerSettingsPage.cc | 乘数、气泡、点击、窗口推动、Codex managed notify 与 app-server 开关/可执行路径/气泡选项、detach/scale、背景、语言、启动、HTTP、更新/代理等设置 UI；所有实际组合选择、数字/滑块/单选/文本输入和滚动容器实例均使用 ElaWidgetTools 控件（spin boxes 使用 PMSide 按钮模式），页面 root、viewport、content 和卡片表面均显式同步 ElaTheme palette；内联编辑统一使用内容驱动的 ElaContentDialog，自然高度后按屏幕限制，背景颜色使用 first-party `CompactFluentColorDialog`（HEX/RGB、色板、自定义色），动态 palette 覆盖 light/dark、focus/hover/pressed/disabled 与高对比语义。 |
| ManagerAboutSection.cc | 嵌入 Manager 中的 About 导航页：持久化 ElaScrollArea 和居中的单列身份区、版本/更新/项目支持可展开卡片；卡片标题支持鼠标、Enter 和 Space，更新/许可证/issue 操作复用现有服务，按钮在窄宽度下流式换行，主题令牌和键盘焦点随 ElaTheme 即时刷新。 |

## 页面与后端边界

CreatePage 只负责让用户选择 legacy 候选，实际安全解压/转换在 core/assets；其卡片、
滚动容器、文本字段、列表和校验状态使用 ElaThemeColor 令牌，并在
`themeModeChanged` 后立即重刷，避免深色模式下出现黑字/白底。CombinationsPage 只保存
受限的描述数据，恢复通过 Manager spawn；其操作/列表/详情面板和禁用/选中状态同样
跟随 ElaThemeColor 令牌即时刷新。SettingsPage 的阶段一 Codex 选项通过 core/codex
修改托管配置，阶段二只保存显式连接开关/可执行路径和最近 thread 元数据，不执行审批；
所有审批按钮和 requestUserInput 回答都在 ManagerCodexPage，且不会自动批准。

修改页面时先确认信号连接位于 ManagerWindowSetup 或 ManagerUiActions，再检查是否需要更新翻译、深色主题和可访问性文本。
