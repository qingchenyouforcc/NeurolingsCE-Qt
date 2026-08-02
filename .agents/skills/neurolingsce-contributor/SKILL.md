---
name: neurolingsce-contributor
description: Use when changing NeurolingsCE C++/Qt code, especially local IPC commands, CLI behavior, mascot runtime, speech bubbles, settings, or Codex companion notifications. It routes an agent to the right subsystem, preserves GUI-thread and security boundaries, and supplies a focused verification checklist.
---

# Neurolingsce Contributor

Use this skill for any contribution to the NeurolingsCE repository. It is a
repo-local orientation guide, not a replacement for the source code or the
project handoff documents.

## Required context

1. Read `AGENTS.md` at the repository root.
2. Read `AGENT.md` for the architecture handoff and current constraints.
3. For Codex companion work, read `.agents/plans/codex-companion-integration.md`.
4. Inspect the relevant existing implementation before editing; preserve
   unrelated working-tree changes.

The project is C++17/Qt 6. First-party code is under `src/app/`, public
headers under `include/shijima-qt/`, tests under `src/app/tests/`, and build
metadata in `CMakeLists.txt`/`cmake/`.

## Subsystem routing

- CLI parsing and output: `src/app/cli/CommandLineParser.cc`,
  `CommandExecutor.cc`, `OutputFormatter.cc`, and `InternalCli.hpp`.
- Local IPC transport: `src/app/core/localipc/` and
  `include/shijima-qt/ShijimaLocalApi.hpp`. Keep requests JSONL and bounded by
  `SecurityLimits`; do not add new HTTP routes for local-only commands.
- Command validation/dispatch: `src/app/core/commands/MascotCommandDispatcher.hpp`
  and `MascotCommandService.cc`. Keep transport-neutral parsing in the
  dispatcher and business mutations in the service.
- Manager/runtime: `ShijimaManager`, `ManagerRuntimeState`,
  `ManagerMascotRuntime.cc`, and `MascotSessionStore`. Running mascot order is
  the session list order; template `@` is the bundled Default Mascot
  (its embedded asset path is also `@`).
- Mascot interaction/UI: `ShijimaWidget`, `SpeechBubbleWidget`, and
  `ManagerSettingsPage.cc`. Reuse existing bubbles and settings controls.
- Codex stage one: `CodexActivity`, `CodexConfigManager`, the
  `show_codex_notification` local command, and the `codex/` QSettings keys.
  Only `agent-turn-complete` is rendered; unknown events succeed silently.

## Thread and safety boundaries

- Local IPC and HTTP callbacks run off the GUI thread. Any mascot/window or
  QSettings mutation that depends on the manager must be inside
  `ShijimaManager::onTickSync()` (or an explicitly queued GUI invocation).
- The Codex notify payload is untrusted input. Enforce the documented size
  limit, parse JSON as an object, reject malformed recognized events, and do
  not display user input, cwd, thread IDs, or turn IDs.
- On Windows, payload text must come from `QCoreApplication::arguments()`,
  which reconstructs the Unicode command line. Do not decode the CRT's narrow
  `argv` as UTF-8 for Codex messages; that corrupts Chinese and emoji.
- Do not automatically approve Codex requests. app-server is a future,
  separately managed client session; never assume it can observe the ChatGPT
  desktop task.
- Config changes are opt-in from the settings page only. Use the managed
  markers and `QSaveFile`; back up before every actual write, refuse to
  overwrite an unmanaged `notify`, and remove only NeurolingsCE's block.
- Do not edit `ElaWidgetTools/`, `cpp-httplib/`, or other vendored code.
  Avoid broad formatting changes and generated build output.

## Implementation workflow

1. Identify the public contract and its owning layer before adding code.
2. Add or update a focused model/helper first, then wire parser → IPC →
   service → GUI. Keep the CLI's default success path silent; `--json` is for
   deterministic automation.
3. For UI, use system palette/font, keyboard-accessible controls, localized
   strings, and existing random bubbles' three-second behavior. Codex bubbles
   are structured/queued and must remain bounded.
4. Add focused tests in `AppCoreTests.cc` or a registered CTest target. Do not
   make tests depend on a running GUI unless the behavior cannot be isolated.
5. Review `git diff`, build the affected GUI/CLI/test targets, run CTest, and
   perform a CLI/IPC smoke test when transport code changes.

## Verification matrix

- CLI: missing/multiple arguments, malformed and oversized JSON, unknown event,
  valid completion, `--json`, Windows Unicode/emoji argv, and runtime auto-start
  retry.
- IPC/dispatcher: payload object validation, size/error response, unknown event
  success, recognized event service call, and GUI-thread handoff.
- Runtime/UI: configured-template reuse, missing-template Default Mascot
  fallback, auto-spawn, queue limit/drop logging, empty reply, multilingual
  text/emoji/grapheme truncation, eight-second Codex duration, and unchanged
  three-second click bubbles.
- Config: default/CODEX_HOME path, atomic creation, timestamp backup,
  idempotent update, executable path escaping, conflict refusal, exact managed
  uninstall, and preservation of unrelated TOML.
- Settings: independent toggle, confirmation, keyboard navigation, template
  selection, test notification, light/dark/high-DPI layout, and no silent
  startup/installer configuration writes.
