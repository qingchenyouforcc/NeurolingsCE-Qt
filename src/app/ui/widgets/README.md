# widgets 气泡与文本排版

widgets 负责 speech bubble 的内容来源、队列、绘制和 Codex 消息的可读化。它只呈现文本，不解析完整 Codex 协议，也不选择 mascot 模板。

## 主流程

CodexActivity/普通 action 文本 → CodexBubbleFormatter 或 SpeechBubbleTextCatalog → SpeechBubbleWidget 队列 → 屏幕定位、主题绘制、定时隐藏。

app-server 的审批与 `requestUserInput` 只产生结构化提醒，最终 Plan/回复才产生
完成气泡；审批按钮和问题答案始终位于 ManagerCodexPage。流式 delta、步骤状态和
未运行桌宠时不会唤醒或召唤 mascot。

## 文件说明

| 文件 | 作用 |
|---|---|
| SpeechBubbleWidget.cc | 绘制气泡、排队/丢弃消息、普通与 Codex 时长/尺寸上限、屏幕边界定位和主题调色。 |
| SpeechBubbleTextCatalog.cc | 按 mascot bubble_context.txt、用户 app-data bubbles.txt、资源和 fallback 的优先级读取/缓存/随机选择普通文本。 |
| CodexBubbleFormatter.hpp | 声明安全 Markdown 清洗、QTextDocument 配置、字体测量、文本适配和显示时长计算接口。 |
| CodexBubbleFormatter.cc | 用 Qt QTextDocument 渲染受限 Markdown（包括带语言提示的围栏代码块）；清除原始 HTML/链接目的地和锚点，把本地/相对文件标签转换为 palette-aware 的非交互行内引用，按 grapheme 安全地二分查找 prefix 截断并修复未闭合 Markdown，计算不超过 UI 上限的时长。 |

## Agent 注意点

- Codex 气泡最多 360×240、8 行，队列最多 8 条，普通/ Codex 时长规则不同；修改 UI 规则要同步 formatter 测试。
- 截断不能切开 emoji 或组合字符；使用现有 grapheme 逻辑，不要按 UTF-16 code unit 直接 mid。
- 气泡文本可能来自不可信 Codex payload，长度限制应在 commands 和 widgets 两层都保留。
- 仅 Codex 正文走 Qt 的 `QTextDocument::setMarkdown()`；标题仍由结构化字段用普通文本绘制，普通点击气泡继续用纯文本和居中布局。
- Markdown 先转义原始 HTML、移除链接目标并清除文档锚点，不加载资源也不打开外部链接。完整消息保留强调、粗体、行内代码、标题、列表和代码块；若受 8 行/360×240 约束需要摘录，只保留消息前缀，移除独立截断标记、修复必要的 Markdown 闭合标记，并让省略号成为最终可见字符。
- Codex 本地文件、file URL 和相对路径链接只保留用户可读标签，并以 Qt 行内代码语义和当前 QPalette 的 AlternateBase/Text 颜色显示；http(s) 等外部链接同样只显示标签，不保留 href、下划线或可点击暗示。图片语法只呈现 alt 文本，不加载图片。
- app-server 提醒不包含命令、cwd、diff 或完整审批理由，只显示稳定类型、限长摘要和
  pending 数；点击提醒只导航到 Codex 页面，不抢焦点、不成为默认允许操作。Plan/回复
  完成气泡按 `(threadId, turnId, itemId)` 去重，并与阶段一 notify 的同 turn 使用
  有界 TTL 缓存去重。
