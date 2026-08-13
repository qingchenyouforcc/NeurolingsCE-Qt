# settings dialogs

`CompactFluentColorDialog` 是 Settings 页使用的 first-party 紧凑颜色编辑器。它以
ElaDialog 为窗口 chrome，但自行绘制紧凑的色相/饱和度面板，提供 HEX/RGB 输入、基本
色板、自定义色和明确的确定/取消动作；初始尺寸由内容 size hint 计算并按屏幕可用区
clamp，取消不会回写 Settings。ElaTheme 切换会同步表面、字段和焦点语义。
