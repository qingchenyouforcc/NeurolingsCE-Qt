# Mascot Registry、商店、GitHub 登录与投稿设计

> 状态：实施蓝图（与 `IMPLEMENTATION_PLAN.md` 同步维护）
> 适用仓库：`qingchenyouforcc/NeurolingsCE`（客户端）与
> `qingchenyouforcc/NeurolingsCE-Mascots`（注册表/服务/工作流）

## 1. 目标与范围

本设计覆盖四块能力：

1. 公共 `.mascot` 验证能力：`NeurolingsCE-cli --mascot validate <FILE> --json`，
   供本地、投稿服务和 GitHub Actions 复用，验证规则只有一份（`MascotPackage.cc`）。
2. Mascot Registry 与静态索引：独立仓库维护 manifest、JSON Schema、审核记录，
   GitHub Pages 发布 `index-v1.json`，客户端只读取静态索引。
3. 客户端 Mascot 商店：索引数据模型、网络客户端、缓存、下载任务、SHA-256 校验、
   安装协调器、商店页面与详情窗口，全部 I/O 不在 GUI 主线程。
4. GitHub 登录与投稿：GitHub App Device Flow + CredentialStore 安全存储，
   客户端本地验证后通过投稿服务创建 draft release 与 PR。

非目标：不修改 `.mascot` 二进制格式；不把 GitHub App 私钥放进客户端；不使用
`pull_request_target` 执行投稿内容；不把注册表拆成多个 packages 仓库。

## 2. 仓库职责

### 2.1 NeurolingsCE（本仓库）

- 客户端：商店浏览/下载/安装、GitHub 登录、投稿上传入口。
- 公共验证器：`MascotPackage::validatePackage()` 与 CLI 命令。
- 文档与 schema 的“本地开发”部分。

### 2.2 NeurolingsCE-Mascots（独立新仓库）

```text
NeurolingsCE-Mascots/
├── mascots/<mascot-id>/manifest.json   # 每个 mascot 一个目录，目录名=id
├── schemas/
│   ├── mascot-manifest-v1.schema.json
│   └── mascot-index-v1.schema.json
├── generated/index-v1.json             # 生成产物，随 Pages 发布
├── tools/
│   ├── validate_registry.py            # schema/重复/一致性检查
│   └── generate_index.py               # 确定性索引生成
├── submission-service/                 # stdlib-only Python 投稿服务
├── docs/                               # 部署、App 设置、安全、审核等
└── .github/workflows/                  # PR 校验、发布、Pages、清理
```

规则：

- ID 全局唯一且不可重新占用；`id` 必须匹配 `^[a-z0-9]+(-[a-z0-9]+)*$`，
  首次合并后进入 `mascots/`，后续只能更新版本。
- 版本必须 SemVer（`MAJOR.MINOR.PATCH`，允许预发布后缀）。
- 同一 `id`+`version` 不允许重复发布。
- 索引由 `generate_index.py` 确定性生成：按 `id` 字典序排序，
  相同输入产生逐字节相同输出；只有 GitHub Release 已发布的投稿进入索引
  （`--published-tags-file` 传入已发布 tag），条目写入派生
  `status: published`。
- 下载 URL 由索引提供，客户端不做字符串拼接。
- 索引带 `schemaVersion`（v1），后续镜像/协议升级通过版本字段迁移。

## 3. 数据契约

### 3.1 info.json（`.mascot` 包内，已有格式）

保持现状：`name`、`version`、`description`、`author`。Registry manifest 是
**包外**元数据，不与包内 `info.json` 合并，避免破坏现有包格式。

### 3.2 Mascot manifest（v1）

字段（完整 JSON Schema 见 Mascots 仓库 `schemas/mascot-manifest-v1.schema.json`）：

| 字段 | 说明 |
| --- | --- |
| `schemaVersion` | 固定 `"1"` |
| `id` | 全局唯一稳定 ID |
| `name` / `version` | 展示名与 SemVer |
| `summary` / `description` | 简介与详细描述 |
| `authors` | 作者 GitHub 登录名 + 显示名 |
| `owner` | `{userId, login}`：首版提交者；GitHub numeric user ID 为权限依据 |
| `maintainers` / `maintainerUserIds` | 展示登录名数组 + 权限依据 numeric user ID 数组 |
| `license` | SPDX 标识 |
| `upstream` | 可选：上游来源 URL/说明 |
| `isDerivative` | 是否二次创作 |
| `minimumNeurolingsCEVersion` | 所需最低客户端版本 |
| `icon` / `previews` | 图标与预览文件信息（由服务端托管） |
| `package` | 包大小、SHA-256、GitHub Release asset 信息 |
| `createdAt` / `updatedAt` | 创建/更新时间 |
| `tags` / `categories` | 可选标签与分类 |

### 3.3 索引（v1）

```json
{
  "schemaVersion": 1,
  "generatedAt": "2026-08-06T00:00:00Z",
  "registry": "qingchenyouforcc/NeurolingsCE-Mascots",
  "mascots": [
    {
      "id": "example",
      "name": "Example",
      "version": "1.2.3",
      "summary": "...",
      "authors": ["octocat"],
      "maintainers": ["octocat"],
      "license": "MIT",
      "minimumNeurolingsCEVersion": "0.5.1",
      "tags": ["cat"],
      "download": {
        "url": "https://github.com/.../releases/download/v1.2.3/example.mascot",
        "size": 123456,
        "sha256": "ab12..."
      },
      "icon": { "url": "...", "sha256": "..." },
      "previews": [ { "url": "...", "sha256": "..." } ],
      "createdAt": "...",
      "updatedAt": "..."
    }
  ]
}
```

客户端只消费 `schemaVersion == 1`；未来升级时保留 `v1` 兼容端点。

## 4. 客户端模块划分

### 4.1 包验证（core/assets）

`MascotPackage::validatePackage(path, report)`：

- 文件存在/大小上限（`kMascotPackageMaxBytes`）。
- ZIP 条目数上限；路径规范化（拒绝绝对路径、`..`、盘符、`\` 混用）。
- 扩展名白名单：`info.json`、`actions.xml`、`behaviors.xml`、
  `bubble_context.txt`、`img/*.png`、`sound/*`。
- 拒绝可执行/脚本扩展名与嵌套压缩包（`.zip/.mascot/.rar/.7z/.tar/...`）。
- 单文件大小、解压后总大小、PNG 像素总量与 PNG 头部有效性。
- 解压到随机临时目录，校验无符号链接、无路径逃逸，随后可靠清理。
- 返回结构化报告：`ok`、metadata、entryCount、extractedFileCount、
  extractedBytes、errors[]。

### 4.2 商店（core/mascotstore）

保持“数据模型 / 网络 / 缓存 / 下载 / 安装”分离：

| 文件 | 职责 |
| --- | --- |
| `MascotStoreIndex` | index-v1 解析、`schemaVersion` 校验、搜索/标签过滤、版本比较（纯数据） |
| `MascotStoreCache` | 索引缓存原子写（`QSaveFile`）、损坏回退旧缓存、ETag/Last-Modified 持久化、离线读取 |
| `MascotStoreNetwork` | `QNetworkAccessManager` 请求、条件请求头、超时、结构化错误 |
| `MascotStoreDownloadTask` | 流式下载、进度/取消/临时文件清理、SHA-256 校验、重试 |
| `MascotStoreInstallCoordinator` | 校验成功后调用 `MascotPackage::installPackage`，通过信号回 UI |

约束：

- 所有 `QNetworkReply` 回调保护对象生命周期（`QPointer`/`QObject` 父子）；
- 阻塞 I/O 使用 `QtConcurrent::run`，结果经信号回 GUI 线程；
- 索引损坏时保留旧缓存，不覆盖；
- 错误信息 = 面向用户的文案 + 稳定错误码（如 `mascotstore.download.sha256_mismatch`）。

### 4.3 GitHub 登录（core/github）

| 文件 | 职责 |
| --- | --- |
| `GitHubAuthManager` | Device Flow 状态机、轮询（遵守 `interval`/`slow_down`）、刷新、撤销、用户信息 |
| `CredentialStore` | 抽象接口 + `InMemoryCredentialStore` + 平台工厂 |
| `WindowsCredentialStore` | Windows Credential Manager（`wincred.h`） |
| `macOSCredentialStore` | macOS Keychain（Security framework，`.mm`） |
| `LinuxCredentialStore` | Secret Service（可选 `libsecret` 编译开关）；默认显式不可用 |

安全规则：

- 桌面客户端只使用 **Login GitHub App** 的 Client ID（权限全 None，
  设备码请求只发送 `client_id`，不发送传统 OAuth `scope`；`GET /user`
  不需要任何 App 权限）；**Publisher GitHub App** 的 App ID/安装 ID/私钥
  只存在于投稿服务端。
- Secret/私钥/安装令牌绝不出现在客户端。
- token 不进日志；日志层提供 `redactSensitiveText()` 统一脱敏
  （`Authorization`、`Cookie`、`access_token`、`refresh_token`）。
- 刷新失败即回到未登录状态；明文 token 文件回退默认禁止。

### 4.4 投稿（core/submission）

`MascotSubmissionService`（客户端）：

- 本地 `validatePackage` 通过后，用户填写元数据并确认发布权。
- 两阶段认证：先 `POST /v1/auth/github` 用 GitHub user access token 换取
  5～10 分钟有效的 submission session token（HMAC 签名、绑定 GitHub
  login/user id）；GitHub token 不进入 multipart 上传，也不出现在 metadata。
- session token 生产模式强制使用固定 `SUBMISSION_SESSION_SECRET`
  （hex/base64，解码 ≥32 字节；缺失/过短/非法编码拒绝启动），固定
  HMAC-SHA256，含 `iss`/`aud`/`sub`/`login`/`iat`/`nbf`/`exp`/`jti`。
- 计算 SHA-256，multipart 流式上传到投稿服务。
- 幂等请求 ID、取消、超时、结构化错误、PR URL 展示。

### 4.5 UI

- `ManagerStorePage`：列表/搜索/过滤/状态（已安装、可更新）。
- `MascotStoreDetailDialog`：详情、作者/许可证/来源、下载进度、重试/取消、安装。
- `MascotSubmissionDialog`：元数据表单 + 确认 + PR 结果。
- 所有 UI 状态经信号更新，不直接触碰网络层。

## 5. 投稿流程

```text
客户端本地验证 → 用户填写元数据 → 计算 SHA-256 →
POST /v1/auth/github 换取 session token → POST /v1/submissions（幂等，
只携带 session token）→ 服务端验证身份 →
服务端重新验证包 → 创建 draft release + 上传 asset →
创建分支 + manifest + PR → 返回 PR URL
```

投稿服务（stdlib-only Python）：

- `POST /v1/submissions`：multipart 流式上传，限制大小，速率限制（用户/IP），
  幂等请求 ID，重复 `id`+`version` 检测。
- 分支名与目标路径由服务端生成：`submission/<id>-<version>` 与
  `mascots/<id>/manifest.json`；PR 创建后再次核对 changed files 白名单，
  越界自动关闭 PR、删除分支与 draft release。
- PR validation 只 checkout `pull_request.base.sha`（可信工具），不执行
  PR 代码；Draft asset 通过 Release Asset API 认证下载（自动生成的只读
  `GITHUB_TOKEN`），绝不匿名下载、绝不提前发布。
- `SUBMISSION_ENV=production` 时公共验证器必填并启动自检；每次投稿调用，
  超时/崩溃/非法 JSON 一律 fail closed，不降级为 Python 检查。
- 新 ID：首版提交者的 numeric user ID 被记录为 owner/初始 maintainer；
  已有 ID：session token 的 numeric user ID 必须属于 `maintainerUserIds`，
  新版本必须严格高于已发布版本，修改 maintainers/owner 必须经批准。
- `GET /v1/submissions/<id>`：状态查询。
- `DELETE /v1/submissions/<id>`：取消/清理 draft release。
- 用户 access token 只用于 `/v1/auth/github` 换取 session token，
  不落库、不写日志、请求后释放；上传只携带 session token。
- 官方仓库操作使用 GitHub App installation token（分支、draft release、asset、PR）。
- 所有日志对 token/Authorization/Cookie 脱敏。
- PR 标题、分支名、文件名只作为 JSON 参数传给 GitHub API，
  不拼接 shell 命令。

## 6. GitHub Actions（Mascots 仓库）

| Workflow | 触发 | 要点 |
| --- | --- | --- |
| `pr-validation.yml` | PR 到 `main` | schema 校验、ID/版本重复、下载 asset 并运行 CLI 验证器、安全扫描 |
| `publish-and-deploy.yml` | `main` 合并 / `workflow_dispatch` | `publish_releases → generate_index → deploy_pages` 顺序执行，发布失败不部署不完整索引 |
| `cleanup-submissions.yml` | 关闭/拒绝 PR | 清理对应 draft release |

安全要求：

- 显式 `permissions` 最小化；第三方 Action 固定完整 commit SHA；
- 不在 `pull_request_target` 中检出并执行投稿内容；
- 不执行包内脚本/二进制；发布 workflow 只处理受保护 `main` 分支上的 manifest；
- GitHub-hosted runner，所有下载包再次校验 SHA-256。
- 发布状态由 GitHub Release 状态推导，发布 workflow **不向 main 回写**，
  不与分支保护冲突，也不会形成 push 循环。
- `publish-and-deploy.yml` 使用固定 concurrency group 与 `queue: max`
  （无 `cancel-in-progress`）：新 push 不会替换 pending 运行，执行顺序
  不保证等于 push 派发顺序，每次运行都按执行时真实 Release 状态幂等
  收敛，不依赖运行先后。
- 源 manifest 不保存可推导的 `status`；索引条目写入派生 `status: published`。
- workflow 只使用自动生成的 `GITHUB_TOKEN`，不读取维护者配置的 Secrets。

## 7. 配置与密钥

占位符配置（真实值由仓库维护者提供）：

| 名称 | 位置 | 说明 |
| --- | --- | --- |
| `NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID` | 客户端编译期配置 | Login App Client ID（公开） |
| `NEUROLINGSCE_SUBMISSION_SERVICE_URL` | 客户端编译期配置 | 投稿服务地址 |
| `NEUROLINGSCE_MASCOT_INDEX_URL` | 客户端编译期配置 | Pages 索引地址 |
| `GITHUB_PUBLISHER_PRIVATE_KEY_PATH` | 投稿服务 Secret | Publisher App 私钥（PEM） |
| `GITHUB_PUBLISHER_APP_ID` / `GITHUB_PUBLISHER_INSTALLATION_ID` | 投稿服务 Secret/环境 | 服务端使用 |
| `SUBMISSION_SERVICE_TOKEN` | 客户端/服务端 | 可选服务间认证 |
| `SUBMISSION_SESSION_SECRET` | 投稿服务 Secret | 生产必填；session token 签名密钥 |

客户端编译期配置集中在 `include/shijima-qt/MascotStoreConfig.hpp`；
`NEUROLINGSCE_STORE_PROFILE=staging|custom|disabled` 选择配置边界，三个
`NEUROLINGSCE_*` 值也可通过同名环境变量传给 CMake。默认 `staging` 使用
现有 `NeurolingsCE-Mascots-Staging` Pages 索引和公开 Login App Client ID；
它是当前公开商店目标，不包含生产仓库内容。`custom` 不配置值，`disabled`
会清空值。CI 通过仓库 Actions Variables
`NEUROLINGSCE_STORE_PROFILE`、`NEUROLINGSCE_MASCOT_INDEX_URL`、
`NEUROLINGSCE_SUBMISSION_SERVICE_URL` 和
`NEUROLINGSCE_GITHUB_LOGIN_CLIENT_ID` 注入公开配置，不读取 gh CLI 的
登录状态或发布服务 secret。未配置时商店与登录显示“维护者尚未配置”，
并清空其他 profile 的旧索引条目，不崩溃。

截至当前实现，正式生产仓库仍未配置；需要生产发布时必须显式选择
`custom` 并提供维护者确认的内容源，不能把 Staging 条目包装为生产目录。

## 8. 威胁模型摘要

| 威胁 | 缓解 |
| --- | --- |
| 恶意 `.mascot`（路径穿越/symlink/ZIP bomb/巨型像素） | `SafePath` + `SecurityLimits` + 临时目录 + 清理 |
| 可执行/脚本载荷 | 扩展名白名单 + 不按扩展名信任 + 不执行包内容 |
| 索引被篡改 | HTTPS + SHA-256 校验 + 条件请求 |
| token 泄露 | CredentialStore + 日志脱敏 + 不落盘明文 |
| 投稿滥用（重复 ID、伪造作者） | 服务端重新验证、GitHub 身份校验、速率限制、maintainer 审核 |
| 供应链 Action 投毒 | 固定 SHA 的第三方 Action + 最小权限 |

## 9. 交付边界

远程仓库、GitHub App、Pages、Secrets 需要维护者配置，本设计不声称已创建；
本地代码、schema、工具、测试、workflow 模板与文档全部使用占位符。
