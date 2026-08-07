# submission 投稿上传客户端

`MascotSubmissionClient` 负责把本地已验证的 `.mascot` 与元数据以
multipart 流式上传到投稿服务，并解析结构化结果（PR URL）。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| MascotSubmissionClient.cc/.hpp | `QHttpMultiPart` 流式上传、进度、取消、超时、结构化错误、幂等请求 ID。 |

## 流程

1. UI 先调用 `MascotPackage::validatePackage` 本地验证。
2. 用户填写元数据并确认发布权。
3. 客户端计算 SHA-256 由服务端校验；客户端上传 multipart。
4. 服务端返回 `{id, status, pr}`；客户端展示 PR 链接。

用户 access token 只用于请求身份验证，不落盘、不写日志。
