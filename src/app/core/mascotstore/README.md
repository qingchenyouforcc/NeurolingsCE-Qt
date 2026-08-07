# mascotstore 商店核心服务

本目录实现 NeuolingsCE-Mascots 商店的客户端核心：索引解析、缓存、网络请求、
下载与安装协调。UI 只消费状态和信号，不直接触碰网络或文件。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| MascotStoreIndex.cc/.hpp | 解析 `index-v1.json`（`schemaVersion == 1`）、按 id 确定性排序、搜索/标签过滤、SemVer 比较。 |
| MascotStoreCache.cc/.hpp | 索引缓存原子写（`QSaveFile`）、ETag/Last-Modified 元数据、损坏时保留上一份好缓存。 |
| MascotStoreNetwork.cc/.hpp | `QNetworkAccessManager` 条件请求（304）、超时、流式下载、SHA-256 校验（QtConcurrent 线程）、取消与临时文件清理。 |
| MascotStoreCoordinator.cc/.hpp | 刷新/离线回退/重试/下载安装编排；安装调用 `MascotPackage::installPackage` 并在线程池执行。 |

## 安全与线程约束

- 下载 URL 只允许 `https`（回环 `http` 仅供本地测试）。
- 下载先写 `.part`，SHA-256 通过后才改名落盘；失败/取消立即清理。
- 索引损坏时绝不覆盖上一份好缓存。
- 所有阻塞 I/O（校验、安装）通过 `QtConcurrent` 离开 GUI 线程。
- 网络回调只通过信号回到协调器；协调器再通过信号回 UI。

## 配置

`include/shijima-qt/MascotStoreConfig.hpp` 保存维护者提供的编译期占位符：
索引 URL、投稿服务 URL、GitHub App Client ID。未配置时 UI 显示
“维护者尚未配置”，不会崩溃。
