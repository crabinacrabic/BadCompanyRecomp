// badcompanyrecomp_app.h - Battlefield: Bad Company Recompiled Project
// Based on ReXGlue SDK v0.10.0+

#pragma once

#include <cstdlib>
#include <memory>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/gpu_plugin.h>

#include "generated/default/badcompanyrecomp_pch.h"

#include "debug_tools.h"
#include "game_cvars.h"
#include "game_patches.h"
#include "utils.h"

extern const rex::PPCImageInfo PPCImageConfig;

class BadcompanyrecompApp : public rex::ReXApp
{
public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext &ctx)
  {
    return std::unique_ptr<BadcompanyrecompApp>(new BadcompanyrecompApp(ctx, "badcompanyrecomp",
                                                                       PPCImageConfig));
  }

  void PrecommitMemoryAliases()
  {
#if defined(_WIN32)
    uint8_t *base = rex::Runtime::instance() ? rex::Runtime::instance()->virtual_membase() : nullptr;
    if (!base)
    {
      return;
    }
    // Precommit physical and aliased Xbox 360 memory ranges required by Frostbite 1.0 engine
    VirtualAlloc(base + 0x00000000ull, 0x40000000ull, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(base + 0x40000000ull, 0x40000000ull, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(base + 0x80000000ull, 0x20000000ull, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(base + 0xA0000000ull, 0x20000000ull, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(base + 0xC0000000ull, 0x20000000ull, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(base + 0xE0000000ull, 0x1FD00000ull, MEM_COMMIT, PAGE_READWRITE);
#endif
  }

  void OnPreSetup(rex::RuntimeConfig &config) override
  {
    std::string backend = REXCVAR_GET(graphics_backend);
    if (backend != "any" && !config.gpu_plugin.empty())
    {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, backend);
      if (!config.graphics)
      {
        REXLOG_WARN("graphics_backend '{}' unavailable, falling back to automatic selection",
                    backend);
      }
    }
  }

  static void UnlockImageMemoryProtection()
  {
#if defined(_WIN32)
    uint8_t *base = rex::Runtime::instance() ? rex::Runtime::instance()->virtual_membase() : nullptr;
    if (!base)
    {
      return;
    }
    // Xbox 360 does not enforce W^X. Frostbite 1.0 self-modifies or writes to sections during runtime.
    // Walk every committed section in the module range and grant PAGE_EXECUTE_READWRITE.
    uint8_t *curr = base + 0x82000000ull;
    uint8_t *end = base + 0x83000000ull;
    while (curr < end)
    {
      MEMORY_BASIC_INFORMATION mbi{};
      if (VirtualQuery(curr, &mbi, sizeof(mbi)))
      {
        if (mbi.State == MEM_COMMIT && mbi.BaseAddress != nullptr)
        {
          DWORD old_protect;
          VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &old_protect);
        }
        curr = (uint8_t *)mbi.BaseAddress + (mbi.RegionSize ? mbi.RegionSize : 0x1000);
      }
      else
      {
        curr += 0x1000;
      }
    }
#endif
  }

  void OnPostLoadXexImage() override
  {
    PrecommitMemoryAliases();
    UnlockImageMemoryProtection();
    game_patches::FpsUnlock();
  }

  void OnPostSetup() override
  {
    PrecommitMemoryAliases();
    UnlockImageMemoryProtection();
    utils::LoadSettingsFiles();

    if (REXCVAR_GET(dev_debug_runtime))
    {
      debug_tools::PerformMissingFunctionScan();
      debug_tools::PerformStubSweep();
    }
  }

  void OnConfigurePaths(rex::PathConfig &paths) override
  {
    if (paths.game_data_root.empty())
    {
      paths.game_data_root = utils::RepoRoot() / "assets";
    }

    paths.config_path = utils::SettingsDir() / "hardware.toml";
    utils::LoadSettingsFiles();
  }
};
