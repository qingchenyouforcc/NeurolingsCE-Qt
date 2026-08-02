# http HTTP API

ShijimaHttpApi 是可选的本地 HTTP 服务。它把 HTTP 请求转换为 core/commands 的统一 JSON 命令，运行在独立监听线程中；业务访问仍通过 GUI 线程同步到 MascotCommandService。

## 文件说明

| 文件 | 作用 |
|---|---|
| ShijimaHttpApi.cc | 创建/停止 cpp-httplib server，注册 /shijima/api/v1 下的 mascot、loadedMascot、ping、label 和 preview 路由，校验 JSON/body 限制并报告监听错误。 |

## 请求生命周期

HTTP handler → body 大小和 JSON object 校验 → MascotCommandDispatcher/Service → 统一 JSON response；preview 额外返回受限 PNG。stop 需要唤醒监听线程并等待线程退出。

## Agent 注意点

- 这是本机控制面，不等于公开网络 API；端口、启用状态和绑定地址受 Settings 约束。
- HTTP worker 不得直接读取或写入 GUI 对象；需要 Manager 状态时使用 Service 的同步入口。
- 路由字段和 CLI/local IPC 保持一致，错误格式由 MascotApi/Dispatcher 统一。
- cpp-httplib 是 vendored 依赖，除集成适配外不要修改其源码。
