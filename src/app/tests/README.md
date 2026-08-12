# 应用层验证目录

历史 `NeurolingsCETests` 和 `NeurolingsCEBubbleTests` 生成目标已移除；该目录不再
参与 CMake/CTest 构建。应用层变更通过正式 GUI/CLI 构建、跨平台 CI 和手工
smoke test 验证，避免保留未构建的测试源码。
