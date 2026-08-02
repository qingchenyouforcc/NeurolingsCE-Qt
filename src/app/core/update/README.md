# update 更新服务

本目录负责检查 GitHub release、选择可用更新并准备下载/安装。它是外围网络与文件替换边界，UI 只消费状态和结果，不应直接操作更新 URL 或临时文件。

## 文件说明

| 文件 | 作用 |
|---|---|
| GitHubUpdateManager.hpp | 声明更新状态、release 信息、下载/安装回调和管理器接口。 |
| GitHubUpdateManager.cc | 请求 GitHub release 元数据，比较版本，下载更新文件并转交平台/安装逻辑，同时处理代理、错误和取消。 |

## 阅读提示

先看 header 中的状态与回调，再看 UI 的 Settings/About 更新入口。修改下载或安装路径时要复核临时目录、代理、网络线程和平台实现，不要让更新逻辑阻塞 GUI tick。
