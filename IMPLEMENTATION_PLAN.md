# IMPLEMENTATION_PLAN

目标：完成“Mascot 注册表、商店、GitHub 登录、投稿上传”的可实施最小闭环。
每个阶段完成后运行相关测试与构建，再进入下一阶段。

## 阶段与状态

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| 0 | 阅读仓库、建立设计文档与计划 | 完成 |
| 1 | CLI `--mascot validate <FILE> --json` + 包安全验证扩展 + 测试 | 完成 |
| 2 | 构建/CTest 验证 CLI 阶段 | 完成 |
| 3 | NeurolingsCE-Mascots 初始化：schema、工具、测试、文档、Actions | 完成 |
| 4 | 商店 core（index/cache/network/download/install）+ 测试 | 完成 |
| 5 | GitHub 登录（Device Flow + CredentialStore）+ 测试 | 完成 |
| 6 | 投稿客户端 + 投稿服务 + 测试 | 完成 |
| 7 | 商店/登录 UI 接入（页面、详情、投稿对话框） | 完成 |
| 8 | 文档收尾、README 更新、最终构建与交付总结 | 完成 |
| 9 | 生产前安全修正：App 权限、workflow 隔离、投稿信任边界、认证收紧、幂等补偿、Release 崩溃复核 | 完成 |
| 10 | 第二轮审计：Draft asset 认证下载、Login/Publisher App 拆分、去 scope、session secret 强制、发布与分支保护解耦、静态 workflow 检查、staging E2E 设计 | 完成 |
| 11 | staging 前最终修正：publish→index→Pages 合并顺序、302 重定向脱敏、Link 分页、numeric ID 所有权模型、manifest status 语义清理 | 完成 |

## 当前完成情况

- 阶段 1/2：`NeurolingsCE-cli --mascot validate <FILE> --json` 已实现并通过
  单元测试与真实 CLI 冒烟测试（退出码 0/1/2，稳定 JSON）。
- 阶段 3：NeurolingsCE-Mascots 本地仓库已初始化并完成首次提交
  （schema、Python 工具与 12 项测试、投稿服务与 10 项测试、文档、
  4 个 GitHub Actions workflow 模板）。
- 阶段 4：商店 index/cache/network/coordinator 与本地 HTTP 服务测试通过。
- 阶段 5：Device Flow 状态机、token 刷新/撤销/过期、InMemory +
  Windows/macOS/Linux 凭据存储接口与日志脱敏测试通过。
- 阶段 6：投稿服务端（stdlib 优先）与客户端 `MascotSubmissionClient`。
- 阶段 7：商店页（搜索/标签/刷新/安装/取消/详情）、GitHub 登录按钮与
  user-code 对话框、投稿对话框已接入主窗口导航。
- 阶段 9：投稿服务改为两阶段认证（`POST /v1/auth/github` 换取 HMAC
  session token）；服务端生成 `submission/<id>-<version>` 分支并只写
  `mascots/<id>/manifest.json`，PR 创建后核对 changed files 白名单；
  `SUBMISSION_ENV=production` 强制公共验证器（启动自检 + 每次投稿调用，
  超时/崩溃/非法 JSON fail closed）；幂等恢复与安全补偿；GitHub 429
  尊重 `Retry-After`；Device Flow 支持 `device_flow_disabled` 与
  `slow_down` interval；logout 删除服务下全部凭据；workflow 最小权限 +
  第三方 Action 固定完整 commit SHA。
- 阶段 10：PR validation 改为 base SHA checkout + API 白名单校验 +
  认证下载 Draft asset（消除匿名下载 Draft 的矛盾）；拆分 Login App
  与 Publisher App；Device Flow 不再发送 `scope`；生产强制
  `SUBMISSION_SESSION_SECRET`（hex/base64 ≥32 字节、固定 HS256、
  iss/aud/sub/login/iat/nbf/exp/jti）；发布 workflow 不再向 main 回写
  （发布状态由 Release 推导，索引读取已发布 tag）；新增 workflow 静态
  检查与 staging E2E 脚本。
- 阶段 11：发布与 Pages 合并为 `publish-and-deploy.yml`
  （`publish_releases → generate_index → deploy_pages`，concurrency
  `queue: max`：新 push 不替换 pending、最多 100 个 pending、执行顺序
  不保证等于 push 派发顺序、每次运行按真实 Release 状态幂等收敛 +
  `workflow_dispatch` 恢复）；Draft asset 下载只接受 HTTPS 重定向并在跨
  host 时剥离 Authorization；所有列表接口改为 Link 分页（per_page 100、
  最多 20 页/5000 条、循环检测、changed_files 数量核对）；所有权改为
  GitHub numeric user ID（`owner.userId`/`maintainerUserIds`，新 ID 记录
  首版提交者、已有 ID 严格版本递增、maintainers 集合变更需批准、同一
  numeric ID 的 login 改名由服务端同步刷新）；源 manifest 不再写
  `status`，索引写入派生 `status: published`。

## 仍需维护者操作

- 创建远程 `NeurolingsCE-Mascots` 仓库并推送本地 main。
- 配置 GitHub App（Client ID/App ID/安装 ID/私钥）与 Pages。
- 部署投稿服务并配置 Secrets。
- workflow 中的第三方 Action 已固定到完整 commit SHA（checkout v4.2.2、
  configure-pages v5.0.0、upload-pages-artifact v3.0.1、deploy-pages v4.0.5），
  推送前只需确认仓库内文件未再被回退。

## 阶段 1 验收标准

- `NeurolingsCE-cli --mascot validate <FILE> --json` 成功/失败均输出稳定 JSON；
- 退出码：0=有效，1=无效包，2=参数错误；
- 报告包含 `package_version`（info.json version）、`mascot`（metadata）、
  `entry_count`、`file_count`、`extracted_bytes`、`errors`；
- 不输出二进制内容，不输出完整敏感路径到日志；
- `--mascot add/list/remove` 行为不变；
- `AppCoreTests` 增加 validate 相关用例并通过。

## 阶段 3 验收标准

- `schemas/*.schema.json` 可被 `jsonschema` 或内置校验器校验；
- `tools/validate_registry.py` 与 `tools/generate_index.py` 测试通过；
- 索引生成确定性（相同输入相同输出）；
- workflow 模板存在且不执行投稿内容、不泄漏 secret。

## 阶段 4 验收标准

- 索引解析、缓存原子写、损坏回退、离线读取、ETag/304、SHA-256 不匹配、
  下载取消清理均有测试；
- 无阻塞 GUI 主线程的 I/O；
- 已安装/可更新状态由安装协调器提供。

## 阶段 5 验收标准

- Device Flow 状态机单元测试（pending/slow_down/取消/过期/刷新）；
- 模拟 GitHub HTTP 服务测试；token 日志脱敏测试；
- CredentialStore：Windows/macOS 平台实现 + 测试用内存实现。

## 阶段 6 验收标准

- 投稿服务端（stdlib-only）通过 unittest；
- multipart 大小限制、幂等、重复 ID/版本、速率限制、身份校验测试；
- 客户端投稿服务层提供取消/超时/结构化错误。

## 外部依赖（需要维护者提供）

- GitHub App ID / Client ID / 安装 ID / 私钥（服务端）；
- 投稿服务域名与部署；
- GitHub Pages 配置；
- Mascots 远程仓库与默认分支保护。

## 已知风险

- Release 退出崩溃已复核：旧二进制（`build-release/bin` 中的陈旧副本）在
  `parseCliArguments` 退出清理时触发 `QString::~QString` 访问冲突
  （写 0x50000063）；用当前 MSVC 2026 Insiders 工具链重新全量构建后，
  `--version`/`--help`/`--mascot validate` 均退出码正常，CTest Release 2/2
  通过。结论：该崩溃为旧工具链/旧产物问题，当前源码+工具链不再复现；
  正式发布仍建议使用 MSVC 2022 稳定工具链。
- Debug/RelWithDebInfo 下 `NeurolingsCETests` 在引擎测试
  （hotspot/window-push）中失败并崩溃（`request_window_push` 回调被破坏），
  详细记录见 `docs/known-issues.md`；修复前不把全部构建配置描述为健康。
- 真实 GitHub E2E 未执行；公开投稿保持 Blocked。
- 投稿服务生产环境需要 `cryptography`（GitHub App JWT RS256）或系统
  `openssl`；README 中已说明用途与许可证（Apache-2.0/MIT 双许可）。

## 验证命令

```powershell
cmake --build build
ctest --test-dir build -C Release --output-on-failure
.\build\bin\NeurolingsCE-cli.exe --json --mascot validate <file.mascot>
uv run python -m unittest discover -s tools/tests -p "test_*.py"
uv run python -m unittest discover -s submission-service/tests -p "test_*.py"
uv run python tools/validate_registry.py . --json
```
