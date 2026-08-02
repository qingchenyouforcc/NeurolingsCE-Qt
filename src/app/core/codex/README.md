# codex 配置管理

本目录维护 NeurolingsCE 与 Codex 的 notify 命令集成。它只改写用户选择托管的配置片段，并尽量保存既有 notify 命令；不负责解析所有 Codex 事件，也不负责决定气泡如何显示。

## 主流程

读取 CODEX_HOME/config.toml → 定位 managed begin/end 标记 → 检查冲突或读取备份标记 → 用 QSaveFile 原子写入并生成备份 → 启用、禁用、恢复或测试 notify 命令。

## 文件说明

| 文件 | 作用 |
|---|---|
| CodexConfigManager.cc | 计算配置路径，读取/写入托管块，保存原有命令的 base64 标记，创建时间戳备份，处理冲突并恢复/移除配置。 |

## Agent 注意点

- 默认配置目录来自 CODEX_HOME，否则是用户的 .codex；不要把仓库目录当成配置目录。
- 禁用时只能删除由本程序托管的精确片段，发现非托管冲突应报告错误而不是静默覆盖。
- 文件写入使用 QSaveFile；修改配置逻辑后重点检查备份、重复启用、恢复旧命令和并发失败路径。
- CodexActivity 的事件识别在 core/commands，不要把配置文本解析和事件 JSON 解析混在一起。
