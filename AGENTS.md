# PROJECT KNOWLEDGE BASE

**Generated:** 2026-02-25
**Commit:** 78a67c9
**Branch:** main

## OVERVIEW

NeurolingsCE — a fork of Shijima-Qt, a cross-platform desktop shimeji (mascot pet) simulator. C++17 / Qt6 / CMake+Make. Targets Windows (MSVC + MinGW), Linux, macOS.

## STRUCTURE

```
NeurolingsCE/
├── src/app/              # Qt application layer (13 .cc files)
├── src/platform/Platform/ # OS abstraction: Windows/Linux/macOS
├── include/shijima-qt/   # Public headers (1:1 mirror of src/app/)
├── libshijima/           # [submodule] Core mascot simulation engine
├── libshimejifinder/     # [submodule] Archive import/extract for mascot packs
├── cpp-httplib/          # [submodule] HTTP server (header-only)
├── cmake/                # CMake helper scripts (BundleDefaultMascot, GenerateLicenses)
├── src/assets/           # Bundled DefaultMascot sprites + XML configs
├── src/packaging/        # Desktop entry, icons, metainfo, .app skeleton
├── src/tools/            # Shell helpers (bundle-default.sh, find_dlls.sh)
├── src/docs/             # HTTP API documentation
├── src/resources/        # Windows resource file (.rc)
├── licenses/             # Third-party license texts (embedded at build time)
├── dev-docker/           # Fedora-based Docker image for Windows cross-compilation
└── .github/workflows/    # CI: debug (push/PR) + release (manual dispatch)
```

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| App entry point | `src/app/main.cc` | Checks single-instance via HTTP ping, creates `ShijimaManager` |
| Mascot lifecycle | `src/app/ShijimaManager.cc` + header in `include/` | Spawning, ticking, environment sync, drag-drop import |
| Mascot rendering | `src/app/ShijimaWidget.cc` | Per-mascot transparent widget, paint/mouse events |
| HTTP REST API | `src/app/ShijimaHttpApi.cc` | Localhost :32456, docs at `src/docs/HTTP-API.md` |
| Platform abstraction | `src/platform/Platform/` | See `src/platform/Platform/AGENTS.md` |
| Asset loading | `src/app/AssetLoader.cc` | Async mascot pack loading |
| Sound support | `src/app/SoundEffectManager.cc` | Optional, gated by `SHIJIMA_USE_QTMULTIMEDIA` |
| Context menu | `src/app/ShijimaContextMenu.cc` | Right-click actions on mascots |
| CLI mode | `src/app/cli.cc` | Invoked when `argc > 1` |
| Build config (CMake) | `CMakeLists.txt` | Primary for MSVC/Visual Studio |
| Build config (Make) | `Makefile` + `common.mk` | Primary for MinGW/GCC/Clang |
| Default mascot embedding | `cmake/BundleDefaultMascot.cmake` | Generates `DefaultMascot.cc` at build time |
| License embedding | `cmake/GenerateLicenses.cmake` | Generates `licenses_generated.hpp` |

## CONVENTIONS

- **Header/source split**: All public headers in `include/shijima-qt/*.hpp`, implementations in `src/app/*.cc`. Extension is `.cc` (not `.cpp`).
- **Include style**: `#include "shijima-qt/Foo.hpp"` for project headers, `#include <shijima/...>` for libshijima, `#include "Platform/..."` for platform layer.
- **C++ standard**: C++17 (`-std=c++17`).
- **Header guard**: `#pragma once` everywhere.
- **License header**: GPLv3 boilerplate at top of every source/header (pixelomer copyright).
- **No `.clang-format`/`.clang-tidy`**: No enforced formatter. Match existing 4-space indent, K&R brace style.
- **Singleton pattern**: `ShijimaManager::defaultManager()` is the global singleton.
- **Template trick**: `PlatformWidget<T>` CRTP-like template wraps QWidget/QMainWindow for cross-desktop behavior.
- **Conditional compilation**: Platform code via `#ifdef __linux__`, `WIN32`, `APPLE` + CMake `if(WIN32)/elseif(APPLE)/elseif(UNIX)`.
- **Feature flags**: `SHIJIMA_USE_QTMULTIMEDIA`, `SHIJIMA_WITH_SHIMEJIFINDER`, `SHIJIMA_WITH_DEFAULT_MASCOT`.

## ANTI-PATTERNS (THIS PROJECT)

- **32-bit MSVC**: Explicitly fatal-errored. Must use x64 toolchain.
- **`SHIJIMA_WITH_DEFAULT_MASCOT=OFF`**: Not supported — `DefaultMascot.cc` is required.
- **`SHIJIMA_WITH_LICENSES_TEXT=OFF`**: Not supported — `licenses_generated.hpp` is required.
- **libshijima**: Does NOT accept contributions (stated in their README). Treat as read-only dependency.

## DUAL BUILD SYSTEMS

| Aspect | CMake | Make |
|--------|-------|------|
| Primary use | MSVC / Visual Studio / Windows-native | MinGW (via Docker) / Linux / macOS |
| Qt discovery | `find_package(Qt6)` | `pkg-config` / framework flags |
| Config | `CMakeSettings.json` (VS), `-DCMAKE_BUILD_TYPE` | `CONFIG=debug\|release` env var |
| libshimejifinder | Off by default on MSVC | Always built |
| Conda Qt fix | Auto-detects, forces `/MD` runtime | N/A |

## SUBMODULES

| Module | Purpose | Notes |
|--------|---------|-------|
| `libshijima` | Mascot simulation core (XML parsing, behaviors, scripting via duktape) | Header-heavy, `shijima/` namespace |
| `libshimejifinder` | Archive extraction for `.zip` mascot packs | Uses libunarr + optional libarchive |
| `cpp-httplib` | Header-only HTTP client/server | Used for single-instance check + REST API |

All three: `git submodule update --init --recursive`.

## COMMANDS

```bash
# CMake (Windows/MSVC)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQt6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build

# Make (Linux/macOS)
CONFIG=release make -j$(nproc)

# Make (Windows via Docker)
docker build -t shijima-qt-dev dev-docker
docker run -e CONFIG=release --rm -v "$(pwd)":/work shijima-qt-dev bash -c 'mingw64-make -j$(nproc)'

# Linux packaging
make appimage

# macOS packaging  
make macapp

# Install (Linux)
make install PREFIX=/usr/local
```

## NOTES

- **Single-instance**: App pings `http://127.0.0.1:32456/shijima/api/v1/ping` on startup. If reachable → refuses to start.
- **Tick rate**: Shimeji run at 25 FPS (hardcoded in libshijima design).
- **Thread safety**: `ShijimaManager` uses `std::mutex` + `std::condition_variable` for tick callbacks from HTTP API thread.
- **Window tracking**: Each platform has its own `ActiveWindowObserver` implementation. Linux supports KDE (KWin script) and GNOME (shell extension).
- **Qt version**: 6.8.x. CI uses 6.8.2 (Linux), MacPorts Qt6 (macOS), Docker Fedora (Windows cross).
- **Project origin**: Fork of [pixelomer/Shijima-Qt](https://github.com/pixelomer/Shijima-Qt) with NeurolingsCE-specific modifications.
- **TODO items** (from README): fix Taskbar bug, i18n support.
