# widgets 气泡与文本排版

widgets 负责 speech bubble 的内容来源、队列、绘制和 Codex 消息的可读化。它只呈现文本，不解析完整 Codex 协议，也不选择 mascot 模板。

## 主流程

CodexActivity/普通 action 文本 → CodexBubbleFormatter 或 SpeechBubbleTextCatalog → SpeechBubbleWidget 队列 → 屏幕定位、主题绘制、定时隐藏。

## 文件说明

| 文件 | 作用 |
|---|---|
| SpeechBubbleWidget.cc | 绘制气泡、排队/丢弃消息、普通与 Codex 时长/尺寸上限、屏幕边界定位和主题调色。 |
| SpeechBubbleTextCatalog.cc | 按 mascot bubble_context.txt、用户 app-data bubbles.txt、资源和 fallback 的优先级读取/缓存/随机选择普通文本。 |
| CodexBubbleFormatter.hpp | 声明字体测量、文本适配和显示时长计算接口。 |
| CodexBubbleFormatter.cc | 用 QFontMetrics 测量文本，按 grapheme 安全地二分查找 head-tail/prefix 截断，计算不超过 UI 上限的时长。 |

## Agent 注意点

- Codex 气泡最多 360×240、8 行，队列最多 8 条，普通/ Codex 时长规则不同；修改 UI 规则要同步 formatter 测试。
- 截断不能切开 emoji 或组合字符；使用现有 grapheme 逻辑，不要按 UTF-16 code unit 直接 mid。
- 气泡文本可能来自不可信 Codex payload，长度限制应在 commands 和 widgets 两层都保留。
