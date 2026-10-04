// game_patches.h - Game-specific memory patches for Battlefield: Bad Company
#pragma once

#include <algorithm>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/runtime.h>

#include "game_cvars.h"
#include "game_constants.h"

namespace game_patches
{
  // Ported from xenia-canary game-patches for Battlefield: Bad Company (454107F9)
  static void FpsUnlock()
  {
    if (!REXCVAR_GET(badco_fps_60))
    {
      return;
    }

    auto *rt = rex::Runtime::instance();
    uint8_t *base = rt ? rt->virtual_membase() : nullptr;
    if (!base)
    {
      return;
    }

    auto patch = GameConstants::PatchConstants::Fps60Patch();
    if (patch.address != 0)
    {
      uint8_t *p = base + patch.address;
      rex::memory::PageAccess old_access{};
      rex::memory::Protect(p, sizeof(patch.value), rex::memory::PageAccess::kReadWrite, &old_access);
      // Write big-endian 32-bit value
      p[0] = static_cast<uint8_t>(patch.value >> 24);
      p[1] = static_cast<uint8_t>(patch.value >> 16);
      p[2] = static_cast<uint8_t>(patch.value >> 8);
      p[3] = static_cast<uint8_t>(patch.value);
      rex::memory::Protect(p, sizeof(patch.value), old_access, nullptr);
    }
  }
} // namespace game_patches
