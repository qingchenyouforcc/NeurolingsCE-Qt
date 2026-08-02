# mascot QWidget 与交互

本目录是单只 mascot 的显示层。ShijimaWidget 的公共声明位于 include/shijima-qt/ShijimaWidget.hpp；三个实现文件按生命周期、绘制和输入分工。

## 文件说明

| 文件 | 作用 |
|---|---|
| MascotWidgetLifecycle.cc | 创建透明/无焦点窗口，绑定引擎 manager、声音和初始帧；tick 引擎/气泡/inspector，处理 fall-through 和析构。 |
| MascotWidgetRendering.cc | 根据当前 pose 安全定位 PNG，通过 AssetLoader 获取 Asset，计算缩放/anchor/窗口 mask，并执行 paint 与 alpha hit-test。 |
| MascotWidgetInteraction.cc | 鼠标拖拽、点击/双击、hotspot 按住、右键菜单、行为调用、繁殖和 Codex 气泡输入处理。 |

## 渲染/输入顺序

引擎 tick 产生 pose → Rendering 选择图像、镜像、偏移和尺度 → QWidget paint；
鼠标事件先做透明像素命中和 hotspot 判定 → Interaction 决定拖拽、行为、speech bubble 或上下文菜单 → Manager 在下一 tick 同步状态。

不要在 paint 中改变引擎状态；不要从鼠标回调直接 delete 当前 widget。窗口删除由 runtime 的 MascotSessionStore 延迟提交。
