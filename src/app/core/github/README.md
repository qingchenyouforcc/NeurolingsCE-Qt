# github GitHub 登录与凭据存储

本目录实现 GitHub App Device Flow 登录与平台安全凭据存储。UI 只调用
`GitHubAuthManager` 的公开接口并消费信号。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| GitHubAuthManager.cc/.hpp | Device Flow 状态机：请求 device code、轮询（`authorization_pending`/`slow_down`）、刷新/撤销 token、拉取用户信息。 |
| CredentialStoreFactory.cc | 平台凭据存储工厂（Windows Credential Manager → macOS Keychain → Linux Secret Service/显式不可用）。 |
| InMemoryCredentialStore.cc | 测试用内存实现。 |
| Secrets.cc/.hpp | token/Authorization/Cookie 统一脱敏函数。 |

## 安全约束

- Client ID 可公开；任何 secret 不进入客户端。
- token 只保存在系统安全存储，默认禁止明文文件回退。
- 刷新失败回到未登录状态；`signOut()` 删除已存凭据。
- 所有日志经过 `redactSensitiveText()`。

平台实现位于 `src/platform/Platform/{Windows,macOS,Linux,Stub}`：

- Windows：`wincred.h`（`CredWriteW/CredReadW/CredDeleteW`）。
- macOS：Security framework Keychain（`SecItemAdd/CopyMatching/Delete`）。
- Linux：可选 `SHIJIMA_WITH_LIBSECRET`；未启用时明确不可用。
