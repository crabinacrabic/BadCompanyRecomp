# CLAUDE_BOOTSTRAP: AI Agent Direct Handover & Mission Briefing

> **FOR AI AGENTS ONLY (Claude Code / Antigravity / Any Agentic LLM):**
> You are collaborating on the PC static recompilation of **Battlefield: Bad Company (2008)** (Xbox 360, Frostbite 1.0).
> This document contains your exact runtime state, debug analysis, call stacks, and immediate next objectives.

---

## 1. Target & Environment Specification

- **Workspace**: `C:\Recompiles\Bad Company`
- **Target Game**: Battlefield: Bad Company (USA)
- **Title ID**: `454107F9`
- **Media ID**: `5C0F179E`
- **XEX Version**: `0.0.0.6` (Build Date: 2008-05-09 15:14:12 UTC)
- **Engine**: Frostbite 1.0 (EA DICE)
- **SDK**: ReXGlue SDK `v0.10.0.8-dev.g1406e1b`
- **Visual Studio Solution**: `out/build/local-win-relwithdebinfo/`
- **Primary Binary**: `out/build/local-win-relwithdebinfo/badcompanyrecomp.exe` (~65.7 MB)
- **Target Preset**: `local-win-relwithdebinfo` (toolset: ClangCL C++23)

---

## 2. Solved Blockers (What Works Now)

### 1. Memory Access Violations (`0xC0000005`)
- **Problem**: Frostbite allocates and reads dynamic guest memory (`0x808105EC`) before ReXGlue heaps commit host memory pages.
- **Solution in `src/badcompanyrecomp_app.h`**:
  - `SafeCommitRange()`: 64 KB chunked precommit across guest virtual & physical memory ranges (`0x00000000`, `0x40000000`, `0x80000000`, `0xA0000000`, `0xC0000000`, `0xE0000000`).
  - `CrashHandler()` (Vectored Exception Handler): On-Demand Page Fault Handler catches any first-chance `EXCEPTION_ACCESS_VIOLATION` in guest address space `[base, base + 4GB]` and commits 64 KB `PAGE_READWRITE` on the fly.
  - `UnlockImageMemoryProtection()`: Walks XEX image memory (`0x82000000`–`0x83000000`) and removes W^X restriction (`PAGE_EXECUTE_READWRITE`).

### 2. DVD-Probe Calibration Crash (`GAME:\dvd-prb`)
- **Problem**: Frostbite's `SerializingIoManager` tries creating a probe file `GAME:\dvd-prb` to test DVD drive access. ReXGlue defaults to read-only mounts, returning `0xC0000022` (`STATUS_ACCESS_DENIED`), triggering `exit(1)`.
- **Solution in `settings/hardware.toml`**:
  - `allow_game_relative_writes = true`
  - Created placeholder `assets/dvd-prb`.
  - In log #10: `NtSetInformationFile set deleting flag for dvd-prb on close to: true` (PROBE SUCCEEDED).

## 3. Resolved Blocker: HDD Cache Partition Calibration & CRT Abort (Title Screen Reached!)

- **Solution**:
  - `sub_8292BDA0`: Returns `STATUS_SUCCESS` (0) to emulate No-HDD SKU mode.
  - `sub_82591430`: Safely skips null dispatchers preventing CRT `abort(1)`.
  - Discovered and registered indirect functions in `default_functions.toml`: `0x82445ED8`, `0x82442A70`, `0x82445E10`, `0x82458E40`, `0x828D4AC0`, `0x829317D8`, `0x82937170`, `0x82378D40`, `0x827B4800`, `0x827B4BB0`, `0x827B4BE8`.
  - **Result**: Game reaches Title Screen ("Press START to begin") with full D3D12 ROV graphics and real-time audio!

### Crash Call Stack (`logs/crash_stack.txt`):
```text
[stack 00] rexruntimerd.dll!KeBugCheckEx_entry (rex::debug::Break() / 0x80000003)
[stack 26] rexruntimerd.dll!KeBugCheck_entry
[stack 30] badcompanyrecomp.exe!sub_82907DB0 (badcompanyrecomp_recomp.178.cpp:26597) - CRT exit/abort
[stack 40] badcompanyrecomp.exe!sub_8258F920 (badcompanyrecomp_recomp.16.cpp:14337)
[stack 42] badcompanyrecomp.exe!sub_8258E1F8 (badcompanyrecomp_recomp.40.cpp:13754)
[stack 52] badcompanyrecomp.exe!sub_8258B4F0 (badcompanyrecomp_recomp.51.cpp:13863)
[stack 60] badcompanyrecomp.exe!sub_825A2380 (badcompanyrecomp_recomp.22.cpp:15118)
[stack 62] badcompanyrecomp.exe!sub_82591588 (badcompanyrecomp_recomp.14.cpp:14806)
[stack 68] badcompanyrecomp.exe!sub_825943C0 (badcompanyrecomp_recomp.155.cpp:16398)
[stack 72] badcompanyrecomp.exe!sub_82591820 (badcompanyrecomp_recomp.27.cpp:13770)
[stack 76] badcompanyrecomp.exe!sub_825909A8 (badcompanyrecomp_recomp.98.cpp:14509)
[stack 84] badcompanyrecomp.exe!sub_82591A58 (badcompanyrecomp_recomp.71.cpp:14263)
[stack 92] badcompanyrecomp.exe!sub_8258DF58 (badcompanyrecomp_recomp.118.cpp:13420)
[stack 110] badcompanyrecomp.exe!sub_82462C20 (badcompanyrecomp_recomp.173.cpp:10073)
[stack 118] badcompanyrecomp.exe!sub_8244E100 (badcompanyrecomp_recomp.26.cpp:9701)
[stack 122] badcompanyrecomp.exe!sub_8244FD90 (badcompanyrecomp_recomp.27.cpp:9429)
[stack 126] badcompanyrecomp.exe!sub_8244FEE8 (badcompanyrecomp_recomp.8.cpp:9264)
```

### Log Signature (`badcompanyrecomp_010.log`):
```text
[warning] [krnl] NtSetInformationFile set deleting flag for dvd-prb on close to: true
[debug] [fs] NullDevice::ResolvePath()
[debug] [krnl] XeKeysConsolePrivateKeySign - stub
[warning] [krnl] IoDismountVolumeByFileHandle(0xf800003c) - stub
[debug] [fs] NullDevice::ResolvePath()
[warning] [krnl] IoDismountVolumeByFileHandle(0xf800003c) - stub
[debug] [fs] NullDevice::ResolvePath()
[debug] [krnl] *** STOP: 0x00000000 (0x00000000, 0x00000000, 0x00000000, 0x00000000)
```

### Key Functions Identified:
1. `sub_82907DB0`: The Xbox 360 CRT termination procedure ending in `KeBugCheck(0)`.
2. `sub_8292BDA0` (`badcompanyrecomp_recomp.146.cpp:26269`): Opens partition, writes test blocks, and dismounts with `IoDismountVolumeByFileHandle`.
3. `sub_8291DEB8` (`badcompanyrecomp_recomp.31.cpp:25753`): HDD cache initialization routine calling `NtCreateFile` for `\Device\Harddisk0\...`, signing hash with `XeKeysConsolePrivateKeySign`, and reading sectors.
4. `sub_8258DF58` (`badcompanyrecomp_recomp.118.cpp:13420`): Frostbite IO Storage Coordinator. Checks media and cache availability.

---

## 4. Mission for Claude Code: Review & Cache Bypass

1. **Review `src/badcompanyrecomp_app.h` and `src/game_fixes.h`**:
   - Verify that all weak symbol hooks properly follow the SDK rules.
2. **Implement HDD Cache Bypass in `src/game_fixes.h`**:
   - Xbox 360 had SKUs without hard drives (Core, Arcade, 4GB Slim). Frostbite detects whether a hard drive is present and operates directly from the DVD (`game:\`) without caching when HDD is absent.
   - Hook `sub_8292BDA0` or return `STATUS_SUCCESS` / graceful code to bypass HDD cache failure.
   - Guard `sub_82907DB0` with detailed register and stack logging before falling through.
3. **Trigger User Build**:
   - Ask user to press **F7** in Visual Studio.
   - Verify next launch log (`badcompanyrecomp_011.log`).
