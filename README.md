# <img src="src/packaging/io.github.qingchenyouforcc.NeurolingsCE.png" alt="NeurolingsCE icon" width="55" /> NeurolingsCE

**[English](README_EN.md) | 中文**

跨平台桌面看板娘（Shimeji）应用，基于 [Shijima-Qt](https://github.com/pixelomer/Shijima-Qt) 深度修改而来。

使用 C++17 / Qt6 构建，支持 Windows、Linux 和 macOS。

![NeurolingsCE screenshot](.images/Shijima-Qt-Main-Window.png)

## 特性

- 🖥️ 跨平台支持（Windows / Linux / macOS）
- 🎭 兼容 Shimeji-ee 格式的看板娘资源包
- 📦 拖放导入看板娘压缩包
- 🛠️ 制作页 — 检查 Shimeji zip 并转换为 `.mascot`
- 🪟 窗口模式 — 在独立沙盒窗口中运行看板娘
- 🖱️ 鼠标交互 — 拖拽、右键菜单
- 📡 HTTP REST API（`localhost:32456`）
- 🌐 多语言支持（English / 中文简体）
- 🔊 可选的音效支持（Qt Multimedia）
- 🖥️ 多显示器支持
- 📐 自定义缩放

## 下载

Neurolings Core是该项目的发行版，Neurolings是该项目的懒人包

- [最新版本](https://github.com/qingchenyouforcc/NeurolingsCE/releases/latest)
- [所有版本](https://github.com/qingchenyouforcc/NeurolingsCE/releases)

## 文档

📖 **[Wiki 文档](https://github.com/qingchenyouforcc/NeurolingsCE/wiki)** — 包含快速开始、构建指南、架构说明、HTTP API、常见问题等完整文档。

## 制作

主界面的“制作”页可以把旧 Shimeji `.zip` 资源包转换为 NeurolingsCE 使用的 `.mascot` 包。选择 zip 后先执行内容检查；如果压缩包中包含多只桌宠，可以勾选要转换的条目。转换结果会写入你选择的输出目录，不会自动导入模板库。

## 日志与调试

NeurolingsCE 默认启用 session 日志。每次启动都会创建独立日志文件，用于记录 GUI/CLI 启动、HTTP API、Local IPC、看板娘导入/召唤/关闭、素材包处理、更新检查、音频、平台窗口观察和崩溃路径等关键操作。

日志级别包括：

- `debug`：详细调试信息，包括经过采样或节流的高频运行状态。
- `info`：正常生命周期、服务启动停止、用户可见操作和成功摘要。
- `warning`：可恢复异常、无效输入、跳过项或业务 4xx 类失败。
- `error`：操作失败、服务错误、导入失败、网络/解析失败等需要排查的问题。
- `critical`：崩溃、Qt fatal、`std::terminate`、SEH 未处理异常等不可恢复错误。

默认最低日志级别为 `info`。需要更详细日志时，可以在启动前设置环境变量：

```powershell
$env:NEUROLINGSCE_LOG_LEVEL="debug"
$env:NEUROLINGSCE_LOG_STDERR="1"
.\NeurolingsCE.exe
```

支持的 `NEUROLINGSCE_LOG_LEVEL` 值为 `debug`、`info`、`warning`、`error`、`critical`。`NEUROLINGSCE_LOG_STDERR=1` 会把日志同时输出到 stderr，适合从终端启动或调试 CLI。

Windows 默认日志目录：

```text
%LOCALAPPDATA%\NeurolingsCE\log\YYYY-MM-DD\neurolingsce-HH-mm-ss-zzz.log
```

Linux/macOS 会优先写入 Qt 的 `AppLocalDataLocation/log`，如果不可用则回退到用户 home 下的 `.neurolingsce/log`；如果首选目录无法创建，程序会尝试写入系统临时目录下的 `NeurolingsCE/log`。

排查问题时，建议按下面步骤收集日志：

1. 设置 `NEUROLINGSCE_LOG_LEVEL=debug`。
2. 重新启动 NeurolingsCE 或 `NeurolingsCE-cli`。
3. 复现问题。
4. 附上最新 session log。日志会记录路径、ID、命令名、状态码、错误摘要和关键计数，但不会记录完整请求体、图片内容、XML/JSON 大文本或二进制内容。

## 构建

### 前置依赖

- C++17 编译器（MSVC 2022 / GCC / Clang）
- Qt 6.8+（Core, Gui, Widgets, Concurrent, LinguistTools）
- CMake 3.21+（Windows/MSVC）或 Make（Linux/macOS）

剩余外部子模块需要初始化（`libshimejifinder`、`cpp-httplib`、`ElaWidgetTools`）：

```bash
git submodule update --init --recursive
```

### Windows（MSVC + CMake）

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build
```

也可以直接用 Visual Studio 打开项目，在 `CMakeSettings.json` 中已配置好 `x64-Debug` 和 `x64-Release` 两个方案。

#### Windows 快速打包 bin 目录

如果你已经通过 Visual Studio/CMake 构建好了 `out/build/x64-Release/bin`，可以直接运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\src\tools\package-windows-bin.ps1
```

默认行为：

- 读取 `VERSION.txt` 生成输出目录名
- 将 `out/build/x64-Release/bin` 复制到 `out/package/NeurolingsCE_windows_x86_64_v<version>`
- 若 `VERSION_NAME=Alpha`，会自动追加 `a` 后缀，例如 `NeurolingsCE_windows_x86_64_v0.3.0a`
- 默认排除 `log/`、`shijima_stdout.txt`、`shijima_stderr.txt`
- 同时生成 zip 包，便于测试分发或作为后续 MSI 输入目录

常用参数：

```powershell
powershell -ExecutionPolicy Bypass -File .\src\tools\package-windows-bin.ps1 `
  -SourceDir out/build/x64-Release/bin `
  -OutputRoot out/package `
  -SkipVcRedist `
  -SkipZip
```

#### Windows MSI（WiX）

仓库现在也包含了一个 WiX 打包入口，建议流程是：

1. 先运行 `package-windows-bin.ps1` 生成干净发布目录
2. 再运行 `installer/wix/build-msi.ps1` 生成 MSI
3. 如果需要把 `vc_redist.x64.exe` 正式串进安装流程，再运行 `installer/wix/build-bundle.ps1` 生成引导安装器 EXE

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-msi.ps1
```

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-bundle.ps1
```

如果你还没安装 WiX，可以先只生成 `.wxs` 文件检查内容：

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-msi.ps1 -GenerateOnly
```

### Windows（MinGW 交叉编译 via Docker）

```bash
docker build -t neurolingsce-dev dev-docker
docker run -e CONFIG=release --rm -v "$(pwd)":/work neurolingsce-dev bash -c 'mingw64-make -j$(nproc)'
```

### Linux

安装 Qt6 开发依赖后：

```bash
CONFIG=release make -j$(nproc)
```

### macOS

支持 Apple Silicon（arm64）与 Intel（x86_64）。Qt6 可通过 Homebrew 或 MacPorts 安装；下面以 Homebrew 为推荐路径，因为它和 macOS 上的 CMake 流程对齐得最好。

1. 安装依赖（Homebrew，推荐）：

```bash
brew install qt qttools libarchive cmake pkg-config
```

   或使用 MacPorts：

```bash
sudo port install qt6-qtbase qt6-qtmultimedia qt6-qttools pkgconfig libarchive cmake
```

2. 构建（推荐使用 CMake，与其他平台一致）：

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DQt6_DIR="$(brew --prefix qtbase)/lib/cmake/Qt6"
cmake --build build -j$(sysctl -n hw.ncpu)
```

   构建产物会输出到 `build/bin/`，包括 `NeurolingsCE`、`NeurolingsCE-cli` 以及测试可执行文件 `NeurolingsCETests`。

   如果使用 MacPorts，把 `Qt6_DIR` 改为 `/opt/local/libexec/qt6/lib/cmake/Qt6` 即可。

3. （备选）使用顶层 Makefile：

```bash
CONFIG=release make -j$(sysctl -n hw.ncpu)
```

   `common.mk` 在 macOS 上会自动探测 Homebrew 的 `qtbase`、`qtmultimedia`、`qttools` 与 `libarchive`；如果同时安装了 MacPorts，也会回退到 `/opt/local/libexec/qt6`。如需指定其他 Qt 路径，可在命令行覆盖：

```bash
CONFIG=release make QT_MACOS_PATH=/your/Qt/lib \
  MOC=/your/Qt/libexec/moc RCC=/your/Qt/libexec/rcc \
  LRELEASE=/your/Qt/bin/lrelease -j$(sysctl -n hw.ncpu)
```

## 平台说明

### Windows

仅支持 x64 工具链。已在 Windows 11 上测试，Windows 10 应该也可以工作。窗口追踪开箱即用。

### Linux

支持 KDE Plasma 6 和 GNOME 46（Wayland / X11）。首次运行时会自动安装 shell 插件来获取前台窗口信息：
- **KDE** — 对用户透明，无需操作。
- **GNOME** — 首次运行后需要重新登录以重启 Shell。程序会给出相应提示。
- **其他桌面环境** — 窗口追踪不可用。

### macOS

需要辅助功能（Accessibility）权限来获取前台窗口。最低系统版本 macOS 13。

## HTTP API

内置 HTTP REST API 运行在 `http://127.0.0.1:32456`，可用于外部程序控制看板娘。

详细文档见 [src/docs/HTTP-API.md](src/docs/HTTP-API.md)。

## CLI

项目现在提供独立的控制台 CLI 程序 `NeurolingsCE-cli`。模板管理命令可独立运行；
运行时 mascot 控制命令会在需要时自动启动 NeurolingsCE 运行时，再通过本地 IPC 控制它。

- 全局选项：`--quiet`、`--json`、`--connect-timeout-ms`、`--read-timeout-ms`
- 文档命令：`--help/-h`、`--summon/-s`、`--close`、`--close-all`、`--stop`、`--mascot/-m`、`--list/-l`、`--version/-v`
- `--summon` 支持两种形式：
  - `--summon mascot --name NAME [label]`
  - `--summon mascot --data-id ID [label]`
  - `--summon random [label]`
- `label` 是面向用户的 CLI 标签，不等于运行时 mascot ID，只在当前应用进程存活期间有效
- `--mascot/-m` 支持模板管理：
  - `--mascot list`
  - `--mascot add ZIP`
  - `--mascot remove MASCOT`
- `--mascot/-m` 不需要主程序已运行；它直接读写本机模板目录
- `--stop` 会关闭全部桌宠并停止 NeurolingsCE 运行时；如果运行时未启动，不会为了停止而新启动它
- 兼容旧命令：`list`、`list-loaded`、`spawn`、`alter`、`dismiss`、`dismiss-all`
- `--json` 会输出稳定的结构化结果与错误对象
- `--host` 和 `--port` 不再支持；CLI 不走 HTTP
- 在 Windows 上建议直接调用 `NeurolingsCE-cli.exe`，这样 shell/agent 能稳定获取退出码

详细命令格式见 [src/docs/HTTP-API.md](src/docs/HTTP-API.md) 中的 CLI 部分。

## NeurolingsCE-Skill

仓库内提供了一个配套 skill：`neurolingsce-skill/`，用于指导 agent 通过 `NeurolingsCE-cli.exe` 控制已安装的桌宠模板。

- skill 名称：`NeurolingsCE-Skill`
- 机器名：`neurolingsce-skill`
- 默认行为：只调用 `NeurolingsCE-cli.exe`；除非用户明确要求，否则不启动 `NeurolingsCE.exe`、不打开 GUI、也不启动 runtime mode
- 语义约定：当用户说“帮我生成一只 xxx 桌宠”时，含义是 `summon` 一个**已安装模板**，不是生成桌宠资源、图片、sprites、XML 或 ZIP
- 如果没有找到对应模板，agent 应直接提示“没有这个模板 / 模板未安装”，而不是自行创建或替换成别的桌宠

常用辅助脚本：

```powershell
python neurolingsce-skill/scripts/find_neurolingsce_cli.py
python neurolingsce-skill/scripts/summon_companion.py
```

- `find_neurolingsce_cli.py`：在显式路径、常见构建目录、`PATH` 和常见安装位置中搜索 `NeurolingsCE-cli.exe`，并记录到 `neurolingsce-skill/cache/neurolingsce-cli-path.json`
- `summon_companion.py`：随机召唤一只**非默认**已安装桌宠；如果只有默认模板，则只返回“没有可用的非默认模板”，不会创建资源

## 项目结构

```
NeurolingsCE/
├── src/app/              # Qt 应用层（core/runtime/ui 分层）
├── src/platform/Platform/ # 平台抽象层（Windows/Linux/macOS）
├── include/shijima-qt/   # 公共头文件
├── src/app/core/shijima-engine/ # 内置核心看板娘模拟引擎源码
├── libshimejifinder/     # [子模块] 看板娘资源包导入/解压
├── cpp-httplib/          # [子模块] HTTP 服务器（header-only）
├── translations/         # i18n 翻译文件
├── cmake/                # CMake 辅助脚本
├── src/assets/           # 内置默认看板娘资源
└── src/packaging/        # 桌面入口、图标、AppStream 元信息
```

`src/app` 目前按职责拆分为三层：

- `src/app/core/`：资源加载、音效、HTTP API、压缩包导入等基础能力
- `src/app/runtime/`：`ShijimaManager` 的环境同步、导入流程、生命周期与运行时调度
- `src/app/ui/`：管理器窗口、托盘、页面构建、桌宠窗口交互、对话框与部件

实现切片统一使用“主体 + 职责”的文件命名，例如 `ManagerImportWorkflow.cc`、`ManagerWindowSetup.cc`、`MascotWidgetRendering.cc`，方便按文件名直接定位业务边界。

## 致谢

本项目基于 [pixelomer](https://github.com/pixelomer) 的 [Shijima-Qt](https://github.com/pixelomer/Shijima-Qt) 开发，在此基础上进行了大量修改和功能增强。

本项目最早是为 "[Neurolings](https://x.com/Monikaphobia/status/1844272129619132682?s=20)" 而做的迁移版本，现在转为通用Shimeji桌宠核心/管理器程序

核心依赖：
- [libshijima](https://github.com/pixelomer/libshijima) — 已整合进 `src/app/core/shijima-engine` 的看板娘模拟引擎
- [libshimejifinder](https://github.com/pixelomer/libshimejifinder) — 资源包解析
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — HTTP 库
- [Qt 6](https://www.qt.io/) — GUI 框架

软件 ICO 致谢：
- 感谢 [宅笙Zhai_Sheng](https://space.bilibili.com/49541366) 绘制软件 ICO



## 联系方式

- **作者**：[轻尘呦](https://space.bilibili.com/178381315)
- **项目地址**：https://github.com/qingchenyouforcc/NeurolingsCE
- **问题反馈**：[GitHub Issues](https://github.com/qingchenyouforcc/NeurolingsCE/issues)
- **反馈 QQ 群**：423902950
- **交流 QQ 群**：125081756

**如果你对neuro社区项目开发感兴趣的话**

**可以联系我加入NeuForge Center**

**请加入STNC了解更多内容**

**STNC蜂群技术情报中心QQ群 125081756**

**STNC项目反馈QQ群 423902950**

## 许可证

本项目基于 [GNU General Public License v3.0](LICENSE) 开源。

### GPLv3 附加条款

在遵守 GPLv3 的基础上，任何再发布、修改版本或衍生作品还必须遵守以下要求：

- 保留原始版权声明、许可证声明以及相关归属信息。
- 不得冒充本项目的官方版本，或以任何可能造成混淆的方式表示其为官方发布版本。
- 未经项目权利人明确许可，不得使用本项目的商标、Logo、项目名称，或以这些标识暗示官方背书、认可或关联。

上游项目 Shijima-Qt 的 README 见 [Shijima-Qt_README.md](Shijima-Qt_README.md)。

![NeurolingsCE icon](src/packaging/io.github.qingchenyouforcc.NeurolingsCE.png)

---
## 广告位

(如果你需要宣传请来联系我)

如果你对neuro同人文感兴趣的话，请加入文学社谢谢喵

**NeuroEcho文学社QQ群1063898428**

---

## Star 趋势

[![Star History Chart](https://api.star-history.com/svg?repos=qingchenyouforcc/NeurolingsCE&type=Date)](https://star-history.com/#qingchenyouforcc/NeurolingsCE&Date)
