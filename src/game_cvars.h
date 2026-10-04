// game_cvars.h - Game-specific CVAR definitions for Battlefield: Bad Company
#pragma once

#include <rex/cvar.h>

REXCVAR_DEFINE_STRING(graphics_backend, "any", "GPU",
                      "Graphics API backend: any, d3d12, vulkan")
    .allowed({"any", "d3d12", "vulkan"});

REXCVAR_DEFINE_BOOL(badco_fps_60, false, "Gameplay",
                    "Unlock 60 FPS (Default: false, authentic 30 FPS for physics stability)");

REXCVAR_DEFINE_BOOL(dev_debug_runtime, true, "Debug",
                    "Enable runtime debug tools (stub sweep, missing function scan)");
