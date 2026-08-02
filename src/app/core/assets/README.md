# assets 资产与 mascot 包

本目录把内置资源、外部 mascot 包、PNG/音频文件和 legacy Shimeji 压缩包转换成运行时可以消费的 MascotData。它是文件系统和不可信归档的主要安全边界。

## 主流程

路径/包名校验 → 解包到受控 cache → 检查 manifest、actions.xml、behaviors.xml 和 PNG → 创建 MascotData → AssetLoader 按图像根目录缓存 Asset。

导入 legacy 时，MascotPackage 先分析候选目录和 archive entries，再由 UI 选择或转换；不要让界面直接读 zip entry 或绕过 SafePath。

## 文件说明

| 文件 | 作用 |
|---|---|
| Asset.cc | 读取一张图并计算 alpha 非透明边界、原始尺寸、裁剪偏移和镜像缓存；Linux 下还维护窗口 mask 所需信息。 |
| AssetLoader.cc | 资产单例缓存；加载默认内置 @ 资源或包目录 PNG，执行大小/像素限制，失败时返回透明占位图，并支持按 imageRoot 卸载。 |
| MascotData.cc | 解包/定位单个 mascot，解析 actions.xml 与 behaviors.xml，选择预览图并暴露名称、路径、元数据和有效性。 |
| MascotPackage.cc | 包名清洗、归档检查、受限解压/打包、安装、legacy archive 分析、候选转换和旧目录迁移。 |
| SafePath.cc | 拒绝绝对路径、盘符、. / ..、分隔符和符号链接逃逸，并检查 canonical path 是否仍在允许根目录内。 |

## Agent 注意点

- archive 总大小、entry 数、单文件大小、PNG 像素数和路径穿越限制由 SecurityLimits 与 SafePath 共同约束。
- @ 是内置默认 mascot 的特殊根，不要把它当成本地可写目录。
- MascotData 的生命周期由 runtime 的 MascotTemplateStore 管理；AssetLoader 的缓存必须在模板/窗口退出前按顺序清理。
- 任何导入流程改动都要同时查看 ui/interface/ManagerCreatePage.cc 和 runtime/ManagerImportWorkflow.cc。
