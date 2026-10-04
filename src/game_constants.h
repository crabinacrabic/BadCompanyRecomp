// game_constants.h - Game-specific constants for Battlefield: Bad Company (454107F9)
#pragma once

#include <cstdint>

namespace GameConstants
{
  constexpr uint32_t kImageBase = 0x82000000;
  constexpr uint32_t kImageSize = 0x00F00000; // 0x82000000 .. 0x82F00000
  constexpr uint32_t kImageEnd  = kImageBase + kImageSize;

  constexpr uint32_t kCodeBase  = 0x82160000;
  constexpr uint32_t kCodeSize  = 0x00A743BC; // 0x82160000 .. 0x82BD43BC
  constexpr uint32_t kCodeEnd   = kCodeBase + kCodeSize;
}

namespace GameConstants::PatchConstants
{
  struct Patch32
  {
    std::uintptr_t address;
    std::uint32_t value;
  };

  // Ported from xenia-canary game-patches for Battlefield: Bad Company (454107F9)
  // 60 FPS patch by illusion: sets target framerate to 60 FPS
  constexpr Patch32 Fps60Patch()
  {
    return Patch32{
        0x8228CCF4,
        0x38800001};
  }
}
