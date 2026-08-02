# dialogs/common 通用对话框

| 文件 | 作用 |
|---|---|
| ForcedProgressDialog.cc | 进度任务期间忽略用户 close；只有业务先设置允许关闭/调用程序化 close 才结束，避免异步导入被半途取消。 |

该对话框通常由 runtime/ManagerImportWorkflow 使用。修改关闭语义时要检查异常、取消和主线程回调是否仍能收尾。
