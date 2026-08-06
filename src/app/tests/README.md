# tests 应用层测试

测试覆盖 core 协议和安全逻辑、引擎脚本/广播，以及 Qt 气泡排版。测试采用轻量的自定义 expect 与 CTest 注册目标，不依赖完整 GUI 主窗口。

## 文件说明

| 文件 | 作用 |
|---|---|
| AppCoreTests.cc | 测试 API JSON/status、CodexActivity 与配置托管、Codex 摘要的 grapheme-safe prefix-first 预压缩、包名/legacy archive/模板目录/路径安全、命令 dispatcher、脚本超时、广播、mascot 长按判定、失焦/丢失抓取时的左键生命周期策略、临时行为预选恢复、窗口推动设置/有效窗口/边缘门控、Fall 动作在全局 floor/work_area 与活动窗口边界冲突时的优先级（含 fall-through 落地）以及贴近真实 Cerber 的 hotspot → action → behavior 连续重启动作链等核心行为。测试中会生成受控 ZIP fixture。 |
| CodexBubbleFormatterTests.cc | 使用 offscreen QGuiApplication 检查短/长文本、emoji、多行、超大字体、prefix 截断拟合和显示时长，并验证 Codex Markdown 的强调/列表/围栏代码块渲染、本地/相对/file URL 引用的非交互行内视觉、图片 alt 降级、原始 HTML 转义与链接锚点清除。 |

## 阅读和扩展方式

- 先按行为名找 test helper，再回到被测模块的 README；测试是协议和安全限制的可执行补充。
- 新增命令、包格式、Codex 字段或气泡排版规则时，应增加 focused case，并在 CMake/CTest 中注册。
- 测试 fixture 生成的归档必须使用明确的临时路径和边界输入，不要把真实用户目录作为测试目标。
