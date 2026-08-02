# audio 声音服务

SoundEffectManager 为 mascot action 提供按名称播放声音的轻量封装。启用 Qt Multimedia 时，它从 mascot 的 imageRoot/声音搜索路径解析文件并限制大小；未启用时编译成兼容的 no-op 实现，使引擎仍可运行。

## 文件说明

| 文件 | 作用 |
|---|---|
| SoundEffectManager.cc | 管理当前 QSoundEffect、查找 sound 文件、播放/停止/playing 状态和资源清理；受 SHIJIMA_USE_QTMULTIMEDIA 条件编译。 |

## 数据流与注意点

引擎 pose/action 给出声音名称 → ShijimaWidget 交给 SoundEffectManager → 在模板资源根下安全解析并播放。声音不是独立线程服务，调用方通常位于 GUI tick；不要在这里增加阻塞式文件扫描或改变 AssetLoader 的路径规则。
