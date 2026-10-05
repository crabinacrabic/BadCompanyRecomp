# AGENTS.md — Instructions for AI Coding Agents

This file is written for AI coding agents (**Claude Code**, **Antigravity**, **Gemini**, etc.) collaborating on the static PC recompilation of **Battlefield: Bad Company (2008)**.

- **Title**: Battlefield: Bad Company (USA)
- **Title ID**: `454107F9`
- **Platform**: Xbox 360 (Xenon architecture, PowerPC 64-bit Big-Endian)
- **Engine**: Frostbite 1.0 (first appearance of Destruction 1.0)
- **SDK**: ReXGlue SDK `v0.10.0.8-dev.g1406e1b`
- **Maintainer language**: Russian (chat and reports in Russian; code and commits in English).

---

## 1. Absolute Invariant Rules (Never Break)

| Rule ID | Rule Statement | Reason / Consequence |
| :--- | :--- | :--- |
| **RULE_NO_CLI_BUILDS** | **DO NOT run `cmake --build`, `ninja`, `msbuild`, `cl`, `clang-cl` in CLI.** | The terminal will hang or corrupt CMake toolset environment. **User builds exclusively via Visual Studio 2022 (`F7` build, `Ctrl + F5` run).** |
| **RULE_READONLY_GENERATED** | **`generated/` is strictly READ-ONLY.** | The build automatically re-runs `rexglue codegen` whenever manifests, config TOMLs, or the XEX change. It completely wipes `generated/`. All patches and overrides MUST be placed in `src/game_fixes.h` or config TOMLs. |
| **RULE_LANG_RUSSIAN** | **All user communication and markdown reports MUST be in Russian.** | Mandatory user requirement. Code comments and commits in English. |
| **RULE_RETRO_STANDARDS** | **Preserve 30 FPS lock (`badco_fps_60 = false`), 720p/1080p, D3D12 ROV, fast readback.** | Frostbite 1.0 physics and vehicle simulation degrade at variable/unlocked frame rates; lighting relies on ROV and fast readback. |

---

## 2. Weak Symbols Architecture for Guest Overrides

In ReXGlue every recompiled function is generated as:
1. A **strong** implementation: `__imp__sub_XXXXXXXX`
2. A **weak alias**: `sub_XXXXXXXX` (aliased to `__imp__sub_XXXXXXXX`)

All call sites and the dispatch table reference the weak symbol `sub_XXXXXXXX`.
To override any guest function, define a strong symbol in `src/game_fixes.h`:

```cpp
#include <rex/hook.h>

REX_HOOK_RAW(sub_XXXXXXXX) {
    // Custom logic / bypass
    // To call the original implementation:
    // __imp__sub_XXXXXXXX(ctx, base);
}
```
Because `badcompanyrecomp_recomp` is an OBJECT library, the linker picks the strong definition from `main.obj` and discards the weak alias.

---

## 3. Project Directory Map

```text
C:\Recompiles\Bad Company\
├── badcompanyrecomp_manifest.toml   <-- ReXGlue codegen manifest
├── default_functions.toml          <-- Function table & indirect call declarations
├── CMakeLists.txt                  <-- CMake configuration (C++23, dbghelp linked)
├── CMakeUserPresets.json           <-- Presets (local-win-relwithdebinfo)
├── settings\
│   ├── hardware.toml               <-- GPU / Engine settings (D3D12 ROV, 30 FPS, allow_game_relative_writes)
│   └── mapping.toml                <-- Controller / Keyboard & Mouse bindings
├── src\
│   ├── main.cpp                    <-- Main entry point (includes game_fixes.h, defines REX_DEFINE_APP)
│   ├── badcompanyrecomp_app.h      <-- ReXApp lifecycle, SafeCommitRange, On-Demand Page Fault Handler
│   ├── game_fixes.h                <-- ALL guest function overrides and hooks
│   ├── game_cvars.h                <-- Custom CVars
│   ├── game_patches.h              <-- Engine memory byte patches
│   ├── debug_tools.h               <-- Stub sweep and missing function scanner
│   └── utils.h                     <-- Path and configuration helpers
├── assets\                         <-- Game disc files (default.xex, dist/, Output/, etc.)
├── generated\                      <-- [READ-ONLY] 367 generated C++ units (54,126 functions)
└── out\build\local-win-relwithdebinfo\logs\ <-- Runtime logs (badcompanyrecomp_NNN.log, crash.dmp)
```

---

## 4. Current Milestone Status (Proof of Concept)

- **Memory Subsystem**: `0xC0000005` Access Violations resolved via On-Demand Page Fault Handler and chunked precommit.
- **VFS / DVD Probe**: `GAME:\dvd-prb` write check passed (`allow_game_relative_writes = true`).
- **Engine Loop**: Process runs continuously (>2 min, ~312 MB RAM, active thread pool).
- **Current Blocker**: HDD Cache Partition (`\Device\Harddisk0\Cache0`, `IoDismountVolumeByFileHandle` -> `sub_82907DB0` -> `KeBugCheck(0)`).
- **Active Task**: Emulate Xbox 360 Core/Arcade (No-HDD) mode or bypass HDD cache check to reach DICE/EA intro logos and Main Menu.
