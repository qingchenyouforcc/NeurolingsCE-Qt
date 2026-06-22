# NeurolingsCE Agent Guide

Generated: 2026-04-24
Workspace: `D:\CPP_project\NeurolingsCE`

This file is a practical handoff map for coding agents. It complements
`AGENTS.md`, but reflects the current tree more closely, including the
standalone CLI, local IPC runtime control, `.mascot` package format, bundled
skills, and Windows packaging flow.

## Project Summary

NeurolingsCE is a C++17 / Qt 6.8+ desktop shimeji mascot runner forked from
Shijima-Qt. It targets Windows, Linux, and macOS. The app has a Qt manager UI,
transparent per-mascot widgets, an integrated shijima simulation engine, a
localhost HTTP API, and a dedicated `NeurolingsCE-cli` binary that controls the
runtime through `QLocalServer` IPC.

The repo currently has uncommitted work in many source and packaging files.
Treat existing modifications as user work. Do not revert unrelated changes.

## Top-Level Layout

| Path | Purpose |
| --- | --- |
| `src/app/main.cc` | GUI/runtime entry point. Also dispatches CLI mode when invoked with CLI commands. |
| `src/app/cli_main.cc` | Dedicated console CLI executable entry point. |
| `src/app/cli*.cc`, `src/app/cli/` | CLI parser, executor, and output formatting. |
| `src/app/core/` | Shared services: assets, audio, HTTP API, local IPC, command service, update checks, logging. |
| `src/app/runtime/` | `ShijimaManager` runtime slices: lifecycle, environment sync, import flow, mascot ticking/spawning. |
| `src/app/ui/` | Manager window, navigation pages, tray menu, dialogs, mascot widget behavior, speech bubbles. |
| `include/shijima-qt/` | Public/project headers used by app slices. |
| `src/app/core/shijima-engine/` | Integrated libshijima engine sources. Local engine fixes belong here. |
| `src/platform/Platform/` | Platform abstraction for Windows/Linux/macOS active-window tracking and widget behavior. |
| `libshimejifinder/` | Submodule/library for archive analysis and extraction. |
| `cpp-httplib/` | Header-only HTTP server/client dependency. |
| `ElaWidgetTools/` | Fluent-style Qt widget library submodule. |
| `src/assets/DefaultMascot/` | Embedded default mascot assets, including `info.json`, XML, and 46 PNG frames. |
| `translations/` | Qt Linguist `.ts` files. `.qm` files are generated/embedded. |
| `cmake/` | CMake helpers for versioning, default mascot embedding, licenses, Linux platform, AppImage. |
| `src/tools/` | Build/package helper scripts, including Windows bin packaging. |
| `installer/wix/` | WiX Toolset 7 MSI/bootstrapper packaging. |
| `neurolingsce-skill/` | Bundled Codex skill for CLI-based mascot control. |
| `neurolingsce-companion/` | Bundled Codex skill for emotional-companion mascot summon flow. |

## Core Runtime Flow

1. `main()` in `src/app/main.cc` checks whether argv is a CLI invocation via
   `shijimaShouldRunCli()`. If so, it runs CLI logic under `QCoreApplication`.
2. GUI/runtime mode initializes platform hooks, logging, bundled skills on
   Windows, ElaWidgetTools, icon resources, and single-instance IPC checks.
3. The app asks an existing instance to show the manager through local IPC
   (`shijimaLocalApiShowManager()`), then rejects startup if the IPC ping still
   responds.
4. `ShijimaManager::defaultManager()` constructs the manager, creates runtime
   and UI state structs, sets up screens, mascot storage, default mascot,
   package loading, timers, theme, navigation, tray icon, local IPC, and HTTP.
5. Mascot ticks run from `timerEvent()` into `ShijimaManager::tick()`.
   The timer interval is `40 / kSubtickCount`, preserving the 25 FPS engine
   model with subticks.

Important files:

| Behavior | Files |
| --- | --- |
| Manager construction | `src/app/ui/ManagerWindowSetup.cc` |
| Shutdown and singleton | `src/app/runtime/ManagerLifecycle.cc` |
| Spawn, tick, reload, labels | `src/app/runtime/ManagerMascotRuntime.cc` |
| Screen/window environment | `src/app/runtime/ManagerEnvironmentSync.cc` |
| Import dialog/background import | `src/app/runtime/ManagerImportWorkflow.cc` |
| UI actions/settings/language | `src/app/ui/ManagerUiActions.cc` |

## State Model

`ShijimaManager` intentionally keeps mutable state in two private structs:

| Struct | File | Notes |
| --- | --- | --- |
| `ShijimaManagerRuntimeState` | `src/app/runtime/ManagerRuntimeState.hpp` | Mascot maps, loaded templates, environments, timers, CLI labels, paths, IPC callback state. |
| `ShijimaManagerUiState` | `src/app/ui/ManagerUiState.hpp` | Qt widgets, translators, status label, pages, selected-template detail labels. |

The manager exposes const accessors for loaded mascot templates and running
mascots. Cross-thread command handling should go through
`ShijimaManager::onTickSync()`, which invokes work on the manager thread via
`Qt::BlockingQueuedConnection` when needed.

## Mascot Data And Package Format

Current template storage is package based:

| Concept | File/API |
| --- | --- |
| Metadata | `MascotMetadata` in `include/shijima-qt/MascotPackage.hpp` |
| Package extension | `*.mascot` |
| Package contents | `info.json`, `actions.xml`, `behaviors.xml`, `img/*.png`, optional `sound/`, optional `bubble_context.txt` |
| Storage path | `QStandardPaths::AppLocalDataLocation/mascots` |
| Cache path | `QStandardPaths::AppLocalDataLocation/mascot-cache` |
| Default mascot | Embedded from `src/assets/DefaultMascot/` into generated `DefaultMascot.cc` |

`MascotPackage.cc` imports both native `.mascot` files and legacy Shimeji-ee zip
archives. Legacy archive import extracts with `libshimejifinder`, packages each
legacy directory into a sanitized `.mascot`, and writes missing `info.json`
metadata when needed.

Do not manually copy arbitrary directories into the mascot storage directory in
new code. Prefer `MascotPackage::importArchive()`, `installPackage()`, or
`packageLegacyDirectory()`.

## CLI And Local IPC

There are two CLI entry paths:

| Entry | Notes |
| --- | --- |
| `src/app/main.cc` | The GUI binary also runs CLI commands when argv matches known commands. |
| `src/app/cli_main.cc` | Dedicated console binary, output name `NeurolingsCE-cli`. Prefer this on Windows for reliable exit codes. |

The CLI no longer uses HTTP. Runtime commands talk to a local IPC server named:

```text
io.github.qingchenyouforcc.NeurolingsCE.cli
```

Important files:

| File | Responsibility |
| --- | --- |
| `src/app/cli/CommandLineParser.cc` | Document-style and legacy command parsing. Rejects `--host` and `--port`. |
| `src/app/cli/CommandExecutor.cc` | Starts runtime if needed, sends IPC JSON, standalone template management. |
| `src/app/cli/OutputFormatter.cc` | Human and `--json` output. |
| `src/app/core/localipc/ShijimaLocalApi.cc` | `QLocalServer` JSON line protocol and dispatch. |
| `src/app/core/localipc/ShijimaLocalApiClient.cc` | `QLocalSocket` request helpers and ping/show-manager helpers. |
| `src/app/core/commands/MascotCommandService.cc` | Shared command business logic used by IPC and HTTP. |

Document-style commands:

```powershell
NeurolingsCE-cli.exe --json --mascot list
NeurolingsCE-cli.exe --json --mascot add ZIP
NeurolingsCE-cli.exe --json --mascot remove MASCOT
NeurolingsCE-cli.exe --json --summon mascot --name NAME [label]
NeurolingsCE-cli.exe --json --summon mascot --data-id ID [label]
NeurolingsCE-cli.exe --json --summon random [label]
NeurolingsCE-cli.exe --json --list
NeurolingsCE-cli.exe --json --close LABEL
NeurolingsCE-cli.exe --json --close-all
NeurolingsCE-cli.exe --json --stop
```

Legacy commands remain supported: `list`, `list-loaded`, `spawn`, `alter`,
`dismiss`, `dismiss-all`.

CLI labels are temporary user-facing labels scoped to the current runtime
process. They are separate from internal mascot IDs and are cleared on restart.

## HTTP API

The GUI/runtime starts the HTTP API only when not in CLI runtime mode.
It listens on:

```text
http://127.0.0.1:32456/shijima/api/v1
```

Implementation: `src/app/core/http/ShijimaHttpApi.cc`.
Documentation: `src/docs/HTTP-API.md`.

Current routes include:

| Method/path | Purpose |
| --- | --- |
| `GET /ping` | Readiness/app metadata. |
| `GET /mascots` | List running mascots, optional `selector`. |
| `POST /mascots` | Spawn by `name` or `data_id`; optional anchor/behavior. |
| `GET /mascots/:id` | Get one running mascot. |
| `PUT /mascots/:id` | Alter anchor/behavior. |
| `DELETE /mascots/:id` | Dismiss one mascot. |
| `DELETE /mascots` | Dismiss all, optional selector JSON body. |
| `GET /loadedMascots` | List loaded templates. |
| `GET /loadedMascots/:id` | Get one loaded template. |
| `GET /loadedMascots/:id/preview.png` | Return preview PNG. |
| `POST /cli/labels` | Assign CLI label to mascot. |
| `GET /cli/labels/:label` | Resolve CLI label to runtime mascot ID. |

When adding commands, prefer implementing shared behavior in
`MascotCommandService`, then expose it through IPC and HTTP as needed.

## Mascot Rendering And Interaction

| Behavior | Files |
| --- | --- |
| Widget lifetime/tick/context | `src/app/ui/mascot/MascotWidgetLifecycle.cc` |
| Painting, asset resolution, masks, hit tests | `src/app/ui/mascot/MascotWidgetRendering.cc` |
| Mouse drag/click/right-click/speech bubble | `src/app/ui/mascot/MascotWidgetInteraction.cc` |
| Context menu | `src/app/ui/menus/ShijimaContextMenu.cc`, `ContextMenuActions.cc` |
| Speech bubbles | `src/app/ui/widgets/SpeechBubbleWidget.cc`, `SpeechBubbleTextCatalog.cc` |
| Inspector | `src/app/ui/dialogs/inspector/` |

`ShijimaWidget::getActiveAsset()` lowercases active frame names before asset
lookup and falls back to `@/img/__missing__.png` for empty frame names. Paint
failures log and mark the mascot for deletion instead of crashing the app.

Fall-through mode is in `ShijimaWidget`/`ManagerMascotRuntime.cc`: if a mascot
falls more than 700 px off-land in non-windowed mode, it bypasses the taskbar
floor until it lands on the absolute screen bottom.

## Platform Layer

Platform abstractions live under `src/platform/Platform/`.

| Platform | Notes |
| --- | --- |
| Windows | `Windows/Platform.cc`, active window observer implementation, topmost/transparent widget behavior. |
| Linux | Qt DBus is required. Supports KDE/KWin script and GNOME shell extension helpers. |
| macOS | Objective-C++ files use AppKit/ApplicationServices; accessibility permission is needed for active window tracking. |
| Stub | Fallback implementation for unsupported platform builds. |

If changing active-window tracking or topmost behavior, inspect both
`Platform.hpp` and the platform-specific `ActiveWindowObserver` files.

## Build Systems

There are two maintained build systems:

| Build | Primary use | Notes |
| --- | --- | --- |
| CMake | Windows/MSVC, Visual Studio, Ninja | Produces `NeurolingsCE` and `NeurolingsCE-cli`; uses `find_package(Qt6)`. |
| Make | Linux/macOS/MinGW Docker | Uses `common.mk`, `pkg-config`, generated MOC/resource targets. |

Version and app metadata come from `VERSION.txt`. Use it as the single source
of truth for app name, version, bundle ID, executable name, icon name, etc.

Common commands:

```powershell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build
```

```bash
CONFIG=release make -j$(nproc)
```

```bash
docker build -t neurolingsce-dev dev-docker
docker run -e CONFIG=release --rm -v "$(pwd)":/work neurolingsce-dev bash -c 'mingw64-make -j$(nproc)'
```

Windows quick packaging:

```powershell
powershell -ExecutionPolicy Bypass -File .\src\tools\package-windows-bin.ps1
```

Windows MSI:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-msi.ps1
powershell -ExecutionPolicy Bypass -File .\installer\wix\build-bundle.ps1
```

## Generated And Template Files

| Generated artifact | Source/template |
| --- | --- |
| `DefaultMascot.cc` | `cmake/BundleDefaultMascot.cmake`, `src/assets/DefaultMascot/` |
| `licenses_generated.hpp` | `cmake/GenerateLicenses.cmake`, `licenses/` |
| `src/packaging/io.github.qingchenyouforcc.NeurolingsCE.metainfo.xml` | `.xml.in` plus `VERSION.txt` |
| `src/resources/resources.rc` | `.rc.in` plus `VERSION.txt` |
| `src/packaging/NeurolingsCE.app/Contents/Info.plist` | `.plist.in` plus `VERSION.txt` |
| `src/platform/Platform/Linux/gnome_script/metadata.json` | `.json.in` plus `VERSION.txt` |

CMake `define_assets_target()` writes some generated files into `src/`, so a
configure/build can modify tracked files. Prefer changing `.in` templates or
`VERSION.txt` instead of editing generated outputs directly unless the project
already treats that output as the source of truth.

## Bundled Agent Skills

The Windows packaging script copies both skill directories into the release
stage. `main.cc` tries to run
`neurolingsce-skill/scripts/install_to_codex_home.ps1` on Windows startup when
the bundled skill exists next to the executable.

| Skill | Purpose |
| --- | --- |
| `neurolingsce-skill` | Control installed mascots through `NeurolingsCE-cli.exe`; never creates assets. |
| `neurolingsce-companion` | For emotional-company requests, summons exactly one installed non-default mascot before normal support. |

Do not confuse a request to "summon" an installed mascot with a request to
generate new mascot sprites/XML/ZIP resources.

## Coding Conventions

- C++ standard: C++17.
- Source extension: `.cc`; Objective-C++ platform files use `.mm`.
- Header guard style: `#pragma once`.
- Public/project headers live in `include/shijima-qt/`.
- App implementation lives under `src/app/core`, `src/app/runtime`, and
  `src/app/ui`.
- Implementation file names are `Subject + Responsibility`, such as
  `ManagerImportWorkflow.cc` and `MascotWidgetRendering.cc`.
- Project include style:
  - `"shijima-qt/Foo.hpp"` for public project headers.
  - `<shijima/...>` for integrated engine headers.
  - `"Platform/..."` for platform abstraction.
- Match existing 4-space indentation and K&R-style braces.
- Source/header files should keep the GPLv3 boilerplate header.
- No repo-wide `.clang-format` or `.clang-tidy`; preserve nearby style.
- Use Qt APIs for JSON, paths, files, threads, and event dispatch where the
  surrounding code already does.

## Local IPC Trust Boundary

NeurolingsCE uses a local IPC endpoint for communication between the official CLI
and the running desktop runtime. This channel is intended for same-user local
automation only. It is not a security sandbox boundary and does not attempt to
defend against malicious processes already running under the same user account.

Security-sensitive validation focuses on untrusted mascot packages, imported
archives, script selectors, image and audio assets, HTTP input when enabled, and
update package integrity.

## Design Constraints And Anti-Patterns

- Do not support 32-bit MSVC. CMake intentionally fatal-errors for x86 MSVC.
- Do not turn off `SHIJIMA_WITH_DEFAULT_MASCOT`; the app expects
  `DefaultMascot.cc`.
- Do not turn off `SHIJIMA_WITH_LICENSES_TEXT`; the licenses dialog expects
  `licenses_generated.hpp`.
- Do not route new CLI runtime control through HTTP. The CLI uses local IPC.
- Do not use `--host`/`--port` in CLI automation; they are explicitly rejected.
- Do not manually delete user mascot files without checking they are inside the
  mascot storage directory.
- Do not submit changes to upstream libshijima; this repo carries the integrated
  engine locally under `src/app/core/shijima-engine`.
- Avoid broad refactors across runtime/UI/core slices unless the task requires
  it. The split files are intentionally responsibility-oriented.

## Testing And Verification Notes

There is no obvious standalone unit-test suite in the current tree. Verification
is usually build- and smoke-test based.

Useful checks:

```powershell
cmake --build build
```

```powershell
.\out\build\x64-Release\bin\NeurolingsCE-cli.exe --json --version
.\out\build\x64-Release\bin\NeurolingsCE-cli.exe --json --mascot list
.\out\build\x64-Release\bin\NeurolingsCE-cli.exe --json --list
```

For runtime commands, the CLI may auto-start `NeurolingsCE.exe` with
`--neurolingsce-cli-runtime`. `--stop` is intentionally idempotent and does not
auto-start the runtime if it is stopped.

When touching UI/runtime behavior, manually smoke-test:

- Launch GUI once and confirm no duplicate-instance error.
- Import a Shimeji zip or `.mascot`.
- Spawn a mascot from the list.
- Drag, click, right-click, and close a mascot.
- Toggle windowed mode.
- Run `NeurolingsCE-cli.exe --json --list`, `--summon`, `--close`, and `--stop`.

## Quick Modification Map

| Task | Start here |
| --- | --- |
| Add a CLI option or output field | `src/app/cli/CommandLineParser.cc`, `CommandExecutor.cc`, `OutputFormatter.cc`, `src/docs/HTTP-API.md` if public docs change. |
| Add shared mascot command behavior | `include/shijima-qt/MascotApi.hpp`, `MascotCommandService.hpp`, `src/app/core/commands/`. |
| Expose command over IPC | `src/app/core/localipc/ShijimaLocalApi.cc`. |
| Expose command over HTTP | `src/app/core/http/ShijimaHttpApi.cc`, `src/docs/HTTP-API.md`. |
| Change package import/storage | `include/shijima-qt/MascotPackage.hpp`, `src/app/core/assets/MascotPackage.cc`, `MascotData.cc`. |
| Change manager UI pages | `src/app/ui/interface/`. |
| Change tray/menu actions | `src/app/ui/ManagerTrayIcon.cc`, `src/app/ui/menus/`. |
| Change mascot painting/hit-testing | `src/app/ui/mascot/MascotWidgetRendering.cc`. |
| Change drag/click behavior | `src/app/ui/mascot/MascotWidgetInteraction.cc`. |
| Change speech bubbles | `src/app/ui/widgets/SpeechBubbleWidget.cc`, `SpeechBubbleTextCatalog.cc`. |
| Change active window behavior | `src/platform/Platform/*/ActiveWindowObserver*`. |
| Change Windows packaging | `src/tools/package-windows-bin.ps1`, `installer/wix/`. |
| Change version/app metadata | `VERSION.txt`, then regenerate/build affected templates. |
