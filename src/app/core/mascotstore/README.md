# mascotstore 商店核心服务

本目录实现 NeuolingsCE-Mascots 商店的客户端核心：索引解析、缓存、网络请求、
下载与安装协调。UI 只消费状态和信号，不直接触碰网络或文件。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| MascotStoreIndex.cc/.hpp | 解析 `index-v1.json`（`schemaVersion == 1`）、按 id 确定性排序、搜索/标签过滤、SemVer 比较。 |
| MascotStoreCache.cc/.hpp | 索引缓存原子写（`QSaveFile`）、ETag/Last-Modified 元数据、损坏时保留上一份好缓存。 |
| MascotStoreNetwork.cc/.hpp | `QNetworkAccessManager` 条件请求（304）、带请求身份的超时、流式下载、SHA-256 校验（QtConcurrent 线程）、独立取消与临时文件清理；包下载不会中断索引刷新。 |
| MascotStoreCoordinator.cc/.hpp | 刷新/离线回退/重试/下载安装编排；串行化单个下载/安装操作并发出安装开始、进度和完成状态；安装调用 `MascotPackage::installPackage` 并在线程池执行。 |

## 安全与线程约束

- 下载 URL 只允许 `https`（回环 `http` 仅供本地测试）。
- 下载先写 `.part`，SHA-256 通过后才改名落盘；失败/取消立即清理。
- 索引损坏时绝不覆盖上一份好缓存。
- 所有阻塞 I/O（校验、安装）通过 `QtConcurrent` 离开 GUI 线程。
- 网络回调只通过信号回到协调器；协调器再通过信号回 UI。

## 配置

`include/shijima-qt/MascotStoreConfig.hpp` 保存维护者提供的编译期占位符：
索引 URL、投稿服务 URL、GitHub App Client ID。CMake 支持
`NEUROLINGSCE_STORE_PROFILE=custom|staging|disabled`，也可从同名环境变量
注入这些公开值；默认 `custom` 不推断任何 URL 或 gh CLI 登录状态。
`staging` 只用于显式的 synthetic staging E2E，不能作为生产内容源；
`disabled` 会在配置阶段清空所有 Store 值。未配置时 UI 清空旧缓存条目，
并分别提示商店和 GitHub 登录不可用，不会把其他构建 profile 的缓存显示成当前商店。
