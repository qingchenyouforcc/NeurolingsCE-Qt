# tests 应用层测试

测试覆盖 core 协议和安全逻辑、引擎脚本/广播，以及 Qt 气泡排版。测试采用轻量的自定义 expect 与 CTest 注册目标，不依赖完整 GUI 主窗口。

## 文件说明

| 文件 | 作用 |
|---|---|
| AppCoreTests.cc | 测试 API JSON/status、CodexActivity 与配置托管、包名/legacy archive/路径安全、命令 dispatcher、脚本超时和广播等核心行为。测试中会生成受控 ZIP fixture。 |
| CodexBubbleFormatterTests.cc | 使用 offscreen QGuiApplication 检查短/长文本、emoji、多行、超大字体、截断拟合和显示时长。 |

## 阅读和扩展方式

- 先按行为名找 test helper，再回到被测模块的 README；测试是协议和安全限制的可执行补充。
- 新增命令、包格式、Codex 字段或气泡排版规则时，应增加 focused case，并在 CMake/CTest 中注册。
- 测试 fixture 生成的归档必须使用明确的临时路径和边界输入，不要把真实用户目录作为测试目标。
