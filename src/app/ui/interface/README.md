# interface 主窗口页面

interface 目录实现 Manager 主窗口的页面层。Navigation 负责页面切换，其余页面负责自己的控件、设置和信号；模板/会话实际操作通过 Manager 的公共接口交给 runtime。

## 文件说明

| 文件 | 作用 |
|---|---|
| ManagerNavigation.cc | 建立 Home、Create、Combinations、Settings、About 页面并处理侧栏导航。 |
| ManagerHomePage.cc | mascot library 主页：列表/详情、spawn/random、导入、刷新、打开目录、响应式布局和主题。 |
| ManagerCreatePage.cc | legacy Shimeji archive 检查/转换：异步分析、候选选择、info.json 编辑、校验和选中项导入。 |
| ManagerCombinationsPage.cc | 在 QSettings 中保存当前 mascot 数量组合，展示上次关闭/已保存组合，并恢复或删除组合。 |
| ManagerSettingsPage.cc | 乘数、气泡、点击、窗口推动、Codex managed notify、detach/scale、背景、语言、启动、HTTP、更新/代理等设置 UI。 |
| ManagerAboutSection.cc | 版本、项目链接、许可证、issue 和更新控制的关于页面/对话框。 |

## 页面与后端边界

CreatePage 只负责让用户选择 legacy 候选，实际安全解压/转换在 core/assets；CombinationsPage 只保存受限的描述数据，恢复通过 Manager spawn；SettingsPage 的 Codex 选项通过 core/codex 修改托管配置，并不执行 Codex 审批。

修改页面时先确认信号连接位于 ManagerWindowSetup 或 ManagerUiActions，再检查是否需要更新翻译、深色主题和可访问性文本。
