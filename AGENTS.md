# Repository Guidelines

## Project Structure & Module Organization

NeurolingsCE is a C++17/Qt 6 desktop application. First-party application code lives in `src/app/`: `core/` contains assets, commands, IPC, HTTP, audio, updates, and the Shijima engine; `runtime/` coordinates sessions and lifecycle; `ui/` contains windows, pages, dialogs, menus, and mascot rendering. Public headers mirror these features under `include/shijima-qt/`. Platform-specific implementations are under `src/platform/Platform/{Windows,Linux,macOS,Stub}`. Tests currently live in `src/app/tests/`. Resources and the bundled mascot are in `src/resources/` and `src/assets/`; translations are in `translations/`.

Treat `ElaWidgetTools/`, `cpp-httplib/`, and most of `libshimejifinder/` as vendored dependencies. Build helpers belong in `cmake/`, packaging scripts in `src/tools/` and `installer/`, and generated output in `build/` or `out/`.

## Agent Code Orientation

src/app/README.md is the entry point for understanding the application layer. Every directory under src/app/ has a README.md that describes the directory responsibility, the main data/control flow, and the purpose of every source or header file in that directory. Before changing a file, read the nearest README and then follow its links to the caller/callee modules when the change crosses a boundary.

Use the documentation hierarchy as a fast reading map:

- src/app/README.md explains application startup, GUI tick flow, CLI/IPC flow, and the module index.
- src/app/core/README.md explains shared services; core/commands/README.md is the JSON contract boundary and core/assets/README.md is the package/path security boundary.
- src/app/runtime/README.md explains Manager lifecycle, template/session ownership, screen environments, imports, and tick ordering.
- src/app/ui/README.md explains the QWidget layer; its child READMEs separate pages, mascot rendering/input, menus, dialogs, and speech bubbles.
- src/app/core/shijima-engine/README.md and the nested engine READMEs explain parser → factory → behavior → action → state/environment execution. rapidxml/ and scripting/duktape/ are vendored boundaries.
- src/app/cli/README.md and src/app/tests/README.md document command execution/output and the corresponding behavior/security tests.

These READMEs are navigation documents, not substitutes for public headers or implementation contracts. When adding, moving, renaming, or materially changing a file under src/app/, update the nearest README and any parent directory index in the same change. Keep file responsibility descriptions aligned with the actual call graph, thread boundary, ownership/lifecycle rules, security limits, and JSON fields. Do not modify vendored engine/dependency sources merely to update documentation.

## Build, Test, and Development Commands

Initialize dependencies after cloning:

```powershell
git submodule update --init --recursive
```

On Windows with Ninja + MSVC, use the provided build entry so that configure
and build run under the same console code page. CMake derives the MSVC
`/showIncludes` dependency prefix from the console output code page at
configure time; if configure runs under a different effective code page than
the build (for example a UTF-8 PowerShell session), `rules.ninja` gets a
mojibake `msvc_deps_prefix`, Ninja records no header dependencies, and stale
objects are not rebuilt.

```powershell
src\tools\build-windows-ninja.cmd build Release   # also accepts Debug / RelWithDebInfo
```

The script pins the console code page (936 on zh-CN systems), loads the MSVC
environment, configures with Ninja, and builds. Set
`NEUROLINGSCE_BUILD_CODEPAGE` to override the code page (for example a
non-zh-CN locale). The equivalent manual commands must run in the same `cmd`
session:

```powershell
cmd /c "chcp 936 >nul && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=D:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6 && cmake --build build --parallel"
```

Run the registered tests with:

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
