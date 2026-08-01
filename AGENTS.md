# Repository Guidelines

## Project Structure & Module Organization

NeurolingsCE is a C++17/Qt 6 desktop application. First-party application code lives in `src/app/`: `core/` contains assets, commands, IPC, HTTP, audio, updates, and the Shijima engine; `runtime/` coordinates sessions and lifecycle; `ui/` contains windows, pages, dialogs, menus, and mascot rendering. Public headers mirror these features under `include/shijima-qt/`. Platform-specific implementations are under `src/platform/Platform/{Windows,Linux,macOS,Stub}`. Tests currently live in `src/app/tests/`. Resources and the bundled mascot are in `src/resources/` and `src/assets/`; translations are in `translations/`.

Treat `ElaWidgetTools/`, `cpp-httplib/`, and most of `libshimejifinder/` as vendored dependencies. Build helpers belong in `cmake/`, packaging scripts in `src/tools/` and `installer/`, and generated output in `build/` or `out/`.

## Build, Test, and Development Commands

Initialize dependencies after cloning:

```powershell
git submodule update --init --recursive
```

Configure and build a Windows release with an installed Qt kit:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6
cmake --build build --parallel
```

For development, substitute `Debug`. Run the registered tests with:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Executables are emitted beneath `build/bin/` (or `out/build/<preset>/bin/` with Visual Studio settings). Smoke-test CLI changes with `NeurolingsCE-cli --json --version`.

## Coding Style & Naming Conventions

Preserve the surrounding C++ style; no repository-wide formatter or linter is configured. Use four-space indentation, braces on the same line, and focused translation units. Classes and Qt widgets use `PascalCase`; functions and variables use `camelCase`; constants follow the nearest module. Keep public declarations in `.hpp` and implementations in `.cc`. Do not reformat or edit vendored code incidentally.

## Testing Guidelines

Add focused cases to `src/app/tests/AppCoreTests.cc` for core logic and register new test executables with CMake/CTest. Name test helpers by behavior, such as `testRejectsUnsafePath`. There is no stated coverage threshold; every change should build cleanly, pass CTest, and receive a manual UI or CLI smoke test when applicable.

## Commit & Pull Request Guidelines

History generally follows concise Conventional Commit subjects, for example `fix(linux): prevent AppImage abort` or `feat(startup): add silent launch`. Use an imperative subject and a scope when useful. Pull requests should explain the problem and solution, list verification commands, link relevant issues, and include screenshots for visible UI changes. Keep generated artifacts and unrelated refactors out of the diff.
