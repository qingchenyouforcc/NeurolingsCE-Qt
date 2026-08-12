# interface 主窗口页面

interface 目录实现 Manager 主窗口的页面层。Navigation 负责页面切换，其余页面负责自己的控件、设置和信号；模板/会话实际操作通过 Manager 的公共接口交给 runtime。

## 文件说明

| 文件 | 作用 |
|---|---|
| ManagerNavigation.cc | 建立 Home、Create、Combinations、Settings、About 页面并处理侧栏导航。 |
| ManagerCodexPage.cc | Codex app-server 单活动 thread 页面：连接/恢复、Default/Plan 模式、turn 控制、Plan/回复只读区、审批与 requestUserInput 卡片及 pending 导航徽标。 |
| ManagerHomePage.cc | mascot library 主页：列表/详情、spawn/random、导入、刷新、打开目录、响应式布局和主题。 |
| ManagerStorePage.cc | 商店目录浏览与筛选、配置诊断/缓存清理、状态横幅、详情、下载/安装/取消操作、GitHub Device Flow 登录和提交入口；通过 MascotStoreCoordinator 消费核心状态。 |
| MascotStoreUi.hpp | 商店页面 QWidget 指针和可见状态（筛选、空状态、状态横幅、下载进度、索引刷新、登录配置）的集中所有权描述。 |
| ManagerCreatePage.cc | legacy Shimeji archive 检查/转换：异步分析、候选选择、info.json 编辑、校验和选中项导入。 |
| ManagerCombinationsPage.cc | 在 QSettings 中保存当前 mascot 数量组合，展示上次关闭/已保存组合，并恢复或删除组合。 |
| ManagerSettingsPage.cc | 乘数、气泡、点击、窗口推动、Codex managed notify 与 app-server 开关/可执行路径/气泡选项、detach/scale、背景、语言、启动、HTTP、更新/代理等设置 UI。 |
| ManagerAboutSection.cc | 版本、项目链接、许可证、issue 和更新控制的关于页面/对话框。 |

## 页面与后端边界

CreatePage 只负责让用户选择 legacy 候选，实际安全解压/转换在 core/assets；CombinationsPage 只保存受限的描述数据，恢复通过 Manager spawn；SettingsPage 的阶段一 Codex 选项通过 core/codex 修改托管配置，阶段二只保存显式连接开关/可执行路径和最近 thread 元数据，不执行审批；所有审批按钮和 requestUserInput 回答都在 ManagerCodexPage，且不会自动批准。

修改页面时先确认信号连接位于 ManagerWindowSetup 或 ManagerUiActions，再检查是否需要更新翻译、深色主题和可访问性文本。
