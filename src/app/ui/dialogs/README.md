# dialogs 对话框

dialogs 是 UI 辅助对话框集合。子目录按用途拆分为导入进度、mascot inspector、商店详情、投稿表单和许可证展示；它们读取 runtime/core 状态，但不拥有核心对象。

| 子目录 | 作用 | 入口文档 |
|---|---|---|
| common | 不可被用户误关的强制进度对话框 | common/README.md |
| inspector | 查看单只 mascot 的窗口、引擎和屏幕状态 | inspector/README.md |
| licenses | 展示构建时生成的第三方许可证 | licenses/README.md |
| settings | Settings 页的紧凑 first-party 颜色编辑器 | settings/README.md |
| store | 展示商店条目详情，并连接安装/登录/提交入口 | store/README.md；由 interface/ManagerStorePage.cc 装配 |
| submission | 提交已验证的 `.mascot` 包及公开元数据；仅负责表单、主题和输入校验反馈，上传由 core/submission 负责 | submission/README.md |
