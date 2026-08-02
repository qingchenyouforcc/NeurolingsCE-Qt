# mascot QWidget 与交互

本目录是单只 mascot 的显示层。ShijimaWidget 的公共声明位于 include/shijima-qt/ShijimaWidget.hpp；三个实现文件按生命周期、绘制和输入分工。

## 文件说明

| 文件 | 作用 |
|---|---|
| MascotWidgetLifecycle.cc | 创建透明/无焦点窗口，绑定引擎 manager、声音和初始帧；tick 引擎/气泡/inspector，处理 fall-through 和析构。 |
| MascotWidgetRendering.cc | 根据当前 pose 安全定位 PNG，通过 AssetLoader 获取 Asset，计算缩放/anchor/窗口 mask，并执行 paint 与 alpha hit-test。 |
| MascotWidgetInteraction.cc | 鼠标拖拽、点击/双击、hotspot 长按摸头、右键菜单、行为调用、繁殖和 Codex 气泡输入处理。 |
| MascotHoldGesture.hpp | 不依赖 QWidget 的长按阈值、移动容差和点击判定策略；供交互实现和 focused tests 共用。 |

## 渲染/输入顺序

引擎 tick 产生 pose → Rendering 选择图像、镜像、偏移和尺度 → QWidget paint；
鼠标事件先做透明像素命中和 hotspot 判定 → Interaction 决定拖拽、行为、speech bubble 或上下文菜单 → Manager 在下一 tick 同步状态。左键命中带行为的 hotspot 时先进入候选状态，保持 260 ms 且最大曼哈顿移动不超过 12 px 才排队行为；按下时钟、最大移动距离和候选状态都记录在实际命中的 target 上，避免重叠窗口由事件接收者代管时丢失长按。摸头行为 active 且鼠标仍按住时会预选同名下一轮，因此动画结束后持续播放。释放前保持鼠标按下会在行为切换后重复排队摸头动画；释放/取消会撤销尚未激活的同名队列，并恢复当前行为自己的 `Add`/`NextBehaviorList`。短按（最多 400 ms、全程最多 6 px）仍走点击/气泡路径，移动越过长按容差则切换到拖拽并取消候选。正常释放、明确 `UngrabMouse`、隐藏或关闭窗口会清理按下状态；FocusOut/WindowDeactivate/ApplicationDeactivate 只有在左键已不再按下时才清理，避免无焦点透明窗口在按住期间误停摸头；每个 tick 还检查 `QGuiApplication::mouseButtons()`，覆盖丢失抓取但没有 release 的情况。所有取消路径都会断开“事件接收者 → 实际 target”的反向指针，避免状态悬挂。

不要在 paint 中改变引擎状态；不要从鼠标回调直接 delete 当前 widget。窗口删除由 runtime 的 MascotSessionStore 延迟提交。
