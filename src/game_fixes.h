// game_fixes.h - Guest-function overrides and fixes for Battlefield: Bad Company
#pragma once

#include <atomic>
#include <mutex>

#include <rex/hook.h>
#include <rex/logging.h>

namespace game_fixes
{
  // ---------------------------------------------------------------------------
  // Serialize XInput calls across multiple guest threads
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
