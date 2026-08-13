# dialogs/common 通用对话框

| 文件 | 作用 |
|---|---|
| ForcedProgressDialog.cc | 进度任务期间忽略用户 close；只有业务先设置允许关闭/调用程序化 close 才结束，避免异步导入被半途取消。内部以 ElaProgressBar/ElaPushButton 组合实现，并按 ElaTheme 动态应用进度条、按钮和禁用态颜色；保留 runtime 所需的 QProgressDialog 兼容方法。默认约 320–520 × 144，并按当前屏幕可用区域收敛。 |

该对话框通常由 runtime/ManagerImportWorkflow 使用。修改关闭语义时要检查异常、取消和主线程回调是否仍能收尾。保留 QDialog 外壳而不切换 ElaDialog/ElaContentDialog，是因为前者会接管窗口标题栏，后者固定三按钮并吞掉键盘事件，均会破坏强制 close 与现有取消/主线程收尾契约。
长状态文案使用可换行 QLabel，设置文案后按内容高度扩展但不超过屏幕上限；窗口始终只保留状态、进度和取消操作，高 DPI 或窄屏时尺寸受可用几何约束。
