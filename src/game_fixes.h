// game_fixes.h - Guest-function overrides and fixes for Battlefield: Bad Company
#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <atomic>
#include <mutex>

#include "generated/default/badcompanyrecomp_pch.h"
#include "generated/default/badcompanyrecomp_funcs.h"

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/logging/api.h>

namespace game_fixes
{
  // ---------------------------------------------------------------------------
  // 1. Serialize XInput calls across multiple guest threads
  // ---------------------------------------------------------------------------
  inline std::mutex g_input_mutex;
  inline std::atomic<uint32_t> g_input_contention{0};

  class InputLock
  {
  public:
    InputLock()
    {
      if (!g_input_mutex.try_lock())
      {
        if (g_input_contention.fetch_add(1, std::memory_order_relaxed) == 0)
        {
          REXLOG_WARN("[fix] XamInput called from two guest threads simultaneously; serialized");
        }
        g_input_mutex.lock();
      }
    }
    ~InputLock() { g_input_mutex.unlock(); }
    InputLock(const InputLock &) = delete;
    InputLock &operator=(const InputLock &) = delete;
  };
} // namespace game_fixes

// ---------------------------------------------------------------------------
// 2. sub_82907DB0: CRT Exit / Abort Diagnosis Handler
// ---------------------------------------------------------------------------
// Called by Frostbite when an unrecoverable engine condition occurs.
// Logs full register and caller context before terminating via KeBugCheck.
// ---------------------------------------------------------------------------
REX_HOOK_RAW(sub_82907DB0)
{
  REXLOG_ERROR("=== FROSTBITE CRT SHUTDOWN TRIGGERED (sub_82907DB0) ===");
  REXLOG_ERROR("  r3 (exit code): 0x{:08X}", ctx.r3.u32);
  REXLOG_ERROR("  r4:             0x{:08X}", ctx.r4.u32);
  REXLOG_ERROR("  r5:             0x{:08X}", ctx.r5.u32);
  REXLOG_ERROR("  Caller LR:      0x{:08X}", (uint32_t)ctx.lr);
  rex::FlushLogging();

  // Call the generated original CRT termination function
  __imp__sub_82907DB0(ctx, base);
}

// ---------------------------------------------------------------------------
// 3. sub_8292BDA0: Frostbite Storage & Cache Partition Check
// ---------------------------------------------------------------------------
// Attempts to open and dismount raw HDD cache partitions (\Device\Harddisk0\Cache0).
// In ReXGlue VFS, these partitions resolve to NullDevice.
// Returning STATUS_SUCCESS (0) emulates an Xbox 360 Core/Arcade SKU (No-HDD)
// so the engine proceeds with direct streaming from GAME:\ without panicking.
// ---------------------------------------------------------------------------
REX_HOOK_RAW(sub_8292BDA0)
{
  REXLOG_INFO("[fix] sub_8292BDA0 called (HDD Cache container open/dismount); returning SUCCESS to emulate No-HDD SKU");
  ctx.r3.s64 = 0; // STATUS_SUCCESS (0)
}
