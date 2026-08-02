# <img src="src/packaging/io.github.qingchenyouforcc.NeurolingsCE.png" alt="NeurolingsCE icon" width="55" /> NeurolingsCE

**English | [中文](README.md)**

> [!NOTE]
**All versions 0.x.x of this application are beta! Please report any bugs in the Issue section!**

A cross-platform desktop mascot (Shimeji) application, extensively modified from [Shijima-Qt](https://github.com/pixelomer/Shijima-Qt).

Built with C++17 / Qt6, supporting Windows, Linux, and macOS.

![NeurolingsCE screenshot](.images/Shijima-Qt-Main-Window.png)

## Features

- 🖥️ Cross-platform support (Windows / Linux / macOS)
- 🎭 Compatible with Shimeji-ee format mascot packs
- 📦 Drag-and-drop mascot pack import
- 🛠️ Create page — check Shimeji zips and convert them to `.mascot`
- 🧩 Mascot combinations — save and restore currently running mascot groups
- 🚀 Start at login and silent restore — restore the last or a selected group on Windows login
- 🪟 Window mode — run mascots in standalone sandbox windows
- 🖱️ Mouse interaction — drag, right-click menu
- 🧰 Dedicated CLI — manage templates and control the runtime with `NeurolingsCE-cli`
- 📡 HTTP REST API (`localhost:32456`)
- 🔐 Safer update checks — use a static update manifest and verify downloaded artifacts
- 🌐 Multi-language support (English / Simplified Chinese)
- 🔊 Optional sound effects (Qt Multimedia)
- 🖥️ Multi-monitor support
- 📐 Custom scaling

## Download

- [Latest Release](https://github.com/qingchenyouforcc/NeurolingsCE/releases/latest)
- [All Releases](https://github.com/qingchenyouforcc/NeurolingsCE/releases)

## Documentation

📖 **[Wiki](https://github.com/qingchenyouforcc/NeurolingsCE/wiki)** — Full documentation including getting started, build guide, architecture, HTTP API, FAQ, and more.

## Highlights Since 0.3.3

Current `main` has moved past the `0.5.0` release line and includes additional updater and startup improvements:

- Added a mascot combinations page for saving the currently running mascot group and restoring the group from the previous close.
- Double-clicking a template can now spawn the matching mascot directly.
- Added the Create page for checking legacy Shimeji `.zip` packs and converting them into NeurolingsCE `.mascot` packages.
- Hardened `.mascot` and legacy ZIP validation, image checks, extraction, and rename-failure handling.
- Update checks now read a static `latest.json` from GitHub Pages instead of calling the GitHub REST releases API from every client.
- Downloaded update artifacts are verified with SHA-256, and release asset names are sanitized before writing to the local cache.
- Windows now supports start-at-login, silent startup, and restoring the previous or selected mascot combination at login.
- CI now includes macOS Intel/Apple Silicon builds, and debug builds run core tests.

## Create

The manager's Create page can convert legacy Shimeji `.zip` packs into NeurolingsCE `.mascot` packages. Choose a zip, run the content check, then select which mascots to convert when the archive contains more than one. Generated packages are written to the output folder you choose and are not imported into the template library automatically.

## Mascot Combinations And Startup Restore

The Combinations page saves groups of mascots that are currently running on screen. You can save multiple spawned mascots as one group and restore it later; NeurolingsCE also records the last group before shutdown.

On Windows, Settings -> Startup can enable start-at-login. With silent startup enabled, NeurolingsCE stays in the tray after login and restores either the last group before close or a selected saved combination.

## Update Manifest

Client update checks now read this static manifest:

```text
https://blog.qingchenyou.asia/NeurolingsCE/update/latest.json
```

GitHub Actions generates and deploys the manifest to GitHub Pages after a Release is published. Clients no longer call `api.github.com/repos/.../releases/latest`, so shared-IP GitHub REST API rate limits no longer break update checks. Downloaded installers are verified with the manifest `sha256` value or `SHA256SUMS.txt` before installation is allowed.

For maintainers, keep GitHub Pages set to the GitHub Actions source and run the `Publish update manifest` workflow after publishing a release. If release assets are uploaded after the release event, rerun the workflow manually.

## Logging And Debugging

NeurolingsCE enables session logging by default. Each launch creates a separate log file covering important GUI/CLI startup events, HTTP API calls, Local IPC requests, mascot import/spawn/close flows, package processing, update checks, audio, platform window observation, and crash paths.

Log levels are:

- `debug`: detailed diagnostic information, including sampled or throttled high-frequency runtime state.
- `info`: normal lifecycle events, service start/stop events, user-visible operations, and success summaries.
- `warning`: recoverable problems, invalid input, skipped items, or business-level 4xx failures.
- `error`: failed operations, service errors, import failures, network/parse failures, and issues that need investigation.
- `critical`: crashes, Qt fatal messages, `std::terminate`, unhandled SEH exceptions, and other unrecoverable failures.

The default minimum level is `info`. To collect deeper diagnostics, set environment variables before starting the app:

```powershell
$env:NEUROLINGSCE_LOG_LEVEL="debug"
$env:NEUROLINGSCE_LOG_STDERR="1"
.\NeurolingsCE.exe
```

Supported `NEUROLINGSCE_LOG_LEVEL` values are `debug`, `info`, `warning`, `error`, and `critical`. `NEUROLINGSCE_LOG_STDERR=1` mirrors logs to stderr, which is useful when launching from a terminal or debugging the CLI.

Default Windows log directory:

```text
%LOCALAPPDATA%\NeurolingsCE\log\YYYY-MM-DD\neurolingsce-HH-mm-ss-zzz.log
```

Linux and macOS prefer Qt's `AppLocalDataLocation/log`; if that is unavailable, the app falls back to `.neurolingsce/log` under the user's home directory. If the preferred directory cannot be created, the app attempts to use `NeurolingsCE/log` under the system temp directory.

For troubleshooting:

1. Set `NEUROLINGSCE_LOG_LEVEL=debug`.
2. Restart NeurolingsCE or `NeurolingsCE-cli`.
3. Reproduce the issue.
4. Attach the newest session log. Logs include paths, IDs, command names, status codes, error summaries, and key counts, but they do not include full request bodies, image data, large XML/JSON payloads, or binary content.

## Building

### Prerequisites

- C++17 compiler (MSVC 2022 / GCC / Clang)
- Qt 6.8+ (Core, Gui, Widgets, Concurrent, LinguistTools)
- CMake 3.21+ (Windows/MSVC) or Make (Linux/macOS)

Initialize the remaining external submodules first (`libshimejifinder`, `cpp-httplib`, and `ElaWidgetTools`):

```bash
git submodule update --init --recursive
```

### Windows (MSVC + CMake)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build
```

You can also open the project directly in Visual Studio — `CMakeSettings.json` includes pre-configured `x64-Debug` and `x64-Release` profiles.

#### Quick Packaging For The Windows bin Directory

If you already built `out/build/x64-Release/bin` with Visual Studio/CMake, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\src\tools\package-windows-bin.ps1
```

By default the script:

- reads `VERSION.txt` to generate the output folder name
- copies `out/build/x64-Release/bin` to `out/package/NeurolingsCE_windows_x86_64_v<version>`
- appends `a` automatically when `VERSION_NAME=Alpha`, for example `NeurolingsCE_windows_x86_64_v0.3.0a`
- excludes `log/`, `shijima_stdout.txt`, and `shijima_stderr.txt`
- creates a zip archive for quick sharing or as MSI input

Common options:

```powershell
powershell -ExecutionPolicy Bypass -File .\src\tools\package-windows-bin.ps1 `
  -SourceDir out/build/x64-Release/bin `
  -OutputRoot out/package `
  -SkipVcRedist `
  -SkipZip
```

#### Windows MSI (WiX)

The repository now also includes a WiX-based MSI entrypoint. Recommended flow:

1. Run `package-windows-bin.ps1` to create a clean staged package directory
2. Run `installer/wix/build-msi.ps1` to build the MSI
3. If you want `vc_redist.x64.exe` chained into the install flow, run `installer/wix/build-bundle.ps1` to build the bootstrapper EXE

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-msi.ps1
```

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-bundle.ps1
```

If WiX is not installed yet, you can still generate the `.wxs` files first:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-msi.ps1 -GenerateOnly
```

### Windows (MinGW Cross-Compilation via Docker)

```bash
docker build -t neurolingsce-dev dev-docker
docker run -e CONFIG=release --rm -v "$(pwd)":/work neurolingsce-dev bash -c 'mingw64-make -j$(nproc)'
```

### Linux

After installing Qt6 development dependencies:

```bash
CONFIG=release make -j$(nproc)
```

### macOS

Apple Silicon (arm64) and Intel (x86_64) are both supported. Qt6 can come from either Homebrew or MacPorts; Homebrew is the recommended path because it lines up best with the CMake-based build flow used on the other platforms.

1. Install dependencies (Homebrew, recommended):

```bash
brew install qt qttools libarchive cmake pkg-config
```

   Or with MacPorts:

```bash
sudo port install qt6-qtbase qt6-qtmultimedia qt6-qttools pkgconfig libarchive cmake
```

2. Build (CMake — preferred, matches the other platforms):

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DQt6_DIR="$(brew --prefix qtbase)/lib/cmake/Qt6"
cmake --build build -j$(sysctl -n hw.ncpu)
```

   Output binaries land in `build/bin/`: `NeurolingsCE`, `NeurolingsCE-cli`, plus the `NeurolingsCETests` test runner.

   For MacPorts, set `Qt6_DIR=/opt/local/libexec/qt6/lib/cmake/Qt6` instead.

3. (Alternative) Top-level Makefile:

```bash
CONFIG=release make -j$(sysctl -n hw.ncpu)
```

   `common.mk` auto-detects Homebrew's `qtbase`, `qtmultimedia`, `qttools`, and `libarchive` on macOS, and falls back to `/opt/local/libexec/qt6` if MacPorts is the only Qt install. To point at a different Qt, override on the command line:

```bash
CONFIG=release make QT_MACOS_PATH=/your/Qt/lib \
  MOC=/your/Qt/libexec/moc RCC=/your/Qt/libexec/rcc \
  LRELEASE=/your/Qt/bin/lrelease -j$(sysctl -n hw.ncpu)
```

## Platform Notes

### Windows

Only x64 toolchain is supported. Tested on Windows 11; Windows 10 should also work. Window tracking works out of the box.

### Linux

Supports KDE Plasma 6 and GNOME 46 (Wayland / X11). On first run, a shell plugin is automatically installed to obtain foreground window information:
- **KDE** — Transparent to the user, no action needed.
- **GNOME** — Requires re-login after first run to restart the Shell. The application will prompt accordingly.
- **Other desktop environments** — Window tracking is not available.

### macOS

Requires Accessibility permission to obtain foreground window information. Minimum system version: macOS 13.

## HTTP API

A built-in HTTP REST API runs at `http://127.0.0.1:32456`, allowing external programs to control mascots.

See detailed documentation at [src/docs/HTTP-API.md](src/docs/HTTP-API.md).

## CLI

The project now provides a dedicated console CLI binary, `NeurolingsCE-cli`.
Template-management commands run standalone; runtime mascot-control commands
auto-start the NeurolingsCE runtime when needed, then talk to it over local IPC.

- Global options: `--quiet`, `--json`, `--connect-timeout-ms`, `--read-timeout-ms`
- Document-style commands: `--help/-h`, `--summon/-s`, `--close`, `--close-all`, `--stop`, `--mascot/-m`, `--list/-l`, `--version/-v`
- `--summon` supports two forms:
  - `--summon mascot --name NAME [label]`
  - `--summon mascot --data-id ID [label]`
  - `--summon random [label]`
- `label` is a user-facing CLI label, not the runtime mascot ID, and only lasts for the current app run
- `--mascot/-m` supports template management:
  - `--mascot list`
  - `--mascot add ZIP`
  - `--mascot remove MASCOT`
- `--mascot/-m` does not require the main app to be running; it directly reads and writes the local template directory
- `--stop` closes all mascots and stops the NeurolingsCE runtime; if no runtime is running, it does not start one just to stop it
- Legacy commands remain supported: `list`, `list-loaded`, `spawn`, `alter`, `dismiss`, `dismiss-all`
- `--json` emits stable structured success payloads and error objects
- `--host` and `--port` are no longer supported; the CLI no longer uses HTTP
- On Windows, call `NeurolingsCE-cli.exe` so shells and agents can reliably read exit codes

See [src/docs/HTTP-API.md](src/docs/HTTP-API.md) for command syntax.

## NeurolingsCE-Skill

The repository also includes a companion skill at `neurolingsce-skill/`. It teaches agents to control already-installed mascot templates through `NeurolingsCE-cli.exe`.

- Skill display name: `NeurolingsCE-Skill`
- Machine name: `neurolingsce-skill`
- Default behavior: call `NeurolingsCE-cli.exe` only; unless the user explicitly asks for it, do not start `NeurolingsCE.exe`, do not open the GUI, and do not start runtime mode
- Semantic rule: when a user says "generate a xxx mascot/desktop pet", interpret that as summoning an **installed template**, not generating mascot assets, images, sprites, XML, or ZIP packs
- If the requested template is missing, the agent should say that the template is not installed or not found, rather than creating a new mascot or substituting a different one

Helper scripts:

```powershell
python neurolingsce-skill/scripts/find_neurolingsce_cli.py
python neurolingsce-skill/scripts/summon_companion.py
```

- `find_neurolingsce_cli.py`: searches explicit paths, common build outputs, `PATH`, and common install locations for `NeurolingsCE-cli.exe`, then records the result in `neurolingsce-skill/cache/neurolingsce-cli-path.json`
- `summon_companion.py`: summons one random **non-default** installed mascot; if only the default template exists, it reports that no non-default template is available and does not create resources

## Project Structure

```
NeurolingsCE/
├── src/app/              # Qt application layer (split into core/runtime/ui)
├── src/platform/Platform/ # Platform abstraction (Windows/Linux/macOS)
├── include/shijima-qt/   # Public headers
├── src/app/core/shijima-engine/ # Integrated core mascot simulation engine source
├── libshimejifinder/     # [submodule] Mascot pack import/extraction
├── cpp-httplib/          # [submodule] HTTP server (header-only)
├── translations/         # i18n translation files
├── cmake/                # CMake helper scripts
├── src/assets/           # Bundled default mascot assets
└── src/packaging/        # Desktop entry, icons, AppStream metadata
```

`src/app` is now organized into three responsibility-focused layers:

- `src/app/core/`: asset loading, audio, HTTP API, and archive import helpers
- `src/app/runtime/`: `ShijimaManager` environment sync, import workflow, lifecycle, and runtime scheduling
- `src/app/ui/`: manager window setup, tray integration, page builders, mascot widget interaction, dialogs, and widgets

Implementation slices follow a `Subject + Responsibility` naming style such as `ManagerImportWorkflow.cc`, `ManagerWindowSetup.cc`, and `MascotWidgetRendering.cc`, so file names map more directly to business logic.

## Credits

This project is based on [Shijima-Qt](https://github.com/pixelomer/Shijima-Qt) by [pixelomer](https://github.com/pixelomer), with extensive modifications and feature enhancements.

This project was originally created as a migration version for "[Neurolings](https://x.com/Monikaphobia/status/1844272129619132682?s=20)", but is now being transformed into a general-purpose Shimeji desktop pet core manager program.

Core dependencies:
- [libshijima](https://github.com/pixelomer/libshijima) — Mascot simulation engine, integrated into `src/app/core/shijima-engine`
- [libshimejifinder](https://github.com/pixelomer/libshimejifinder) — Mascot pack parser
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — HTTP library
- [Qt 6](https://www.qt.io/) — GUI framework

ICO credits:
- Thanks to [宅笙Zhai_Sheng](https://space.bilibili.com/49541366) for creating the application ICO

## Contact

- **Author**: [轻尘呦](https://space.bilibili.com/178381315)
- **Repository**: https://github.com/qingchenyouforcc/NeurolingsCE
- **Bug Reports**: [GitHub Issues](https://github.com/qingchenyouforcc/NeurolingsCE/issues)
- **Chat QQ Group**: 125081756

**Interested in Neuro community project development?**

**Contact me to join NeuForge Center**

**Join STNC to learn more**

## License

This project is open-sourced under the [GNU General Public License v3.0](LICENSE).

### GPLv3 Additional Terms

In addition to complying with GPLv3, any redistribution, modified version, or derivative work must also comply with the following requirements:

- Preserve the original copyright notices, license notices, and related attribution information.
- Do not impersonate the official version of this project, or represent a release in any way that could cause confusion with an official release.
- Do not use this project's trademarks, Logo, or project name, or use them to imply official endorsement, approval, or affiliation, without explicit permission from the project rights holders.

The upstream Shijima-Qt README is available at [Shijima-Qt_README.md](Shijima-Qt_README.md).

![NeurolingsCE icon](src/packaging/io.github.qingchenyouforcc.NeurolingsCE.png)

---
## Ads

(If you need promotion, please contact me.)

---

## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=qingchenyouforcc/NeurolingsCE&type=Date)](https://star-history.com/#qingchenyouforcc/NeurolingsCE&Date)
