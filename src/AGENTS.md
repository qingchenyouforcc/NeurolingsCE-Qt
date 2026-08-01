# Repository Guidelines

## Scope & Architecture

These guidelines apply to all code under `src/`. NeurolingsCE is a C++17/Qt 6 desktop application. Keep changes in the narrowest responsible layer: entry points start the application, runtime managers coordinate state, core modules implement behavior and services, UI modules render and handle input, and platform modules isolate operating-system APIs.

## Project Structure & Module Organization

- `app/main.cc` and `app/cli_main.cc` are the GUI and CLI entry points.
- `app/core/` contains commands, assets, audio, HTTP, IPC, updates, and the embedded Shijima behavior engine. Preserve action/behavior XML compatibility when modifying `shijima-engine/`.
- `app/runtime/` owns application lifecycle, mascot sessions, template storage, and environment coordination.
- `app/ui/` is split into mascot rendering/interaction, pages, dialogs, menus, and reusable widgets. Keep domain rules out of widgets.
- `app/tests/` contains CTest-backed regression tests, primarily `AppCoreTests.cc`.
- `platform/Platform/{Windows,Linux,macOS,Stub}` contains OS-specific implementations behind shared interfaces.
- `assets/` and `resources/` hold the bundled mascot and Qt resources; `packaging/` and `tools/` support distribution builds.

Public API declarations belong under `../include/shijima-qt/`; internal headers should remain beside their implementation.

## Build, Test, and Development Commands

Run commands from the repository root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQt6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build --parallel
ctest --test-dir build -C Debug --output-on-failure
build/bin/NeurolingsCE-cli --json --version
```

These configure Qt, compile all targets, run registered tests, and smoke-test the CLI respectively.

## Coding Style & Naming Conventions

Follow nearby code: four-space indentation, same-line opening braces, `PascalCase` classes/widgets, and `camelCase` functions/variables. Use `.hpp` for declarations and `.cc` for implementations. No repository-wide formatter is configured; avoid incidental reformatting and do not modify vendored code for unrelated cleanup.

## Testing Guidelines

Add focused regression cases to `app/tests/AppCoreTests.cc` and register new executables with CMake/CTest. Name helpers by behavior, such as `testSelectsNearestBroadcastTarget`. Engine, lifecycle, and platform fixes require a clean build plus relevant CTest and manual UI or CLI verification.

## Commit & Pull Request Guidelines

Use concise Conventional Commit subjects, for example `fix(mascot): select nearby hug target`. Pull requests should link the issue, explain the behavioral change, list verification commands, and include screenshots for visible UI changes. Exclude generated artifacts and unrelated refactors.
