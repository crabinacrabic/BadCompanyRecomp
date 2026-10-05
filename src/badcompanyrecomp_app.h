// badcompanyrecomp_app.h - Battlefield: Bad Company Recompiled Project
// Based on ReXGlue SDK v0.10.0+

#pragma once

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/logging/api.h>
#include <rex/memory/utils.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/gpu_plugin.h>
#include <rex/system/function_dispatcher.h>

#include "generated/default/badcompanyrecomp_pch.h"
#include "generated/default/badcompanyrecomp_funcs.h"

#include "debug_tools.h"
#include "game_cvars.h"
#include "game_patches.h"
#include "utils.h"

extern const rex::PPCImageInfo PPCImageConfig;

#if defined(_WIN32)
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS *ep)
{
  // 1. On-demand guest memory page fault handler
  if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
  {
    uintptr_t fault_addr = ep->ExceptionRecord->ExceptionInformation[1];
    uint8_t *base = rex::Runtime::instance() ? rex::Runtime::instance()->virtual_membase() : nullptr;
    if (base && fault_addr >= (uintptr_t)base && fault_addr < (uintptr_t)base + 0x100000000ull)
    {
      void *page_base = (void *)(fault_addr & ~0xFFFFull);
      if (VirtualAlloc(page_base, 0x10000, MEM_COMMIT, PAGE_READWRITE))
      {
        return EXCEPTION_CONTINUE_EXECUTION;
      }
    }
  }

  // 2. Fatal crash diagnostics and minidump creation
  if (ep->ExceptionRecord->ExceptionCode == 0x80000003 ||
      ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
      ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
  {
    rex::FlushLogging();

    FILE *f = std::fopen("logs/crash_stack.txt", "w");
    if (f)
    {
      std::fprintf(f, "=== CRASH CAUGHT BY HANDLER ===\n");
      std::fprintf(f, "Exception Code:    0x%08lX\n", ep->ExceptionRecord->ExceptionCode);
      std::fprintf(f, "Exception Address: %p\n", ep->ExceptionRecord->ExceptionAddress);
      std::fprintf(f, "Thread ID:         %lu\n", GetCurrentThreadId());
      std::fprintf(f, "RIP: %p, RSP: %p\n", (void *)ep->ContextRecord->Rip, (void *)ep->ContextRecord->Rsp);

      uintptr_t *rsp = (uintptr_t *)ep->ContextRecord->Rsp;
      for (int i = 0; i < 128; ++i)
      {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(&rsp[i], &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT)
        {
          uintptr_t val = rsp[i];
          HMODULE mod = nullptr;
          if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)val, &mod))
          {
            char modname[MAX_PATH];
            GetModuleFileNameA(mod, modname, sizeof(modname));
            const char *base = std::strrchr(modname, '\\');
            std::fprintf(f, "  [stack %02d] %p (%s + 0x%llX)\n", i, (void *)val, base ? base + 1 : modname, (unsigned long long)(val - (uintptr_t)mod));
          }
        }
      }
      std::fclose(f);
    }

    HANDLE hFile = CreateFileA("logs/crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
      MINIDUMP_EXCEPTION_INFORMATION mei;
      mei.ThreadId = GetCurrentThreadId();
      mei.ExceptionPointers = ep;
      mei.ClientPointers = FALSE;
      MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mei, NULL, NULL);
      CloseHandle(hFile);
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

class BadcompanyrecompApp : public rex::ReXApp
{
public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext &ctx)
  {
#if defined(_WIN32)
    AddVectoredExceptionHandler(1, CrashHandler);
#endif
    return std::unique_ptr<BadcompanyrecompApp>(new BadcompanyrecompApp(ctx, "badcompanyrecomp",
                                                                       PPCImageConfig));
  }

  static void SafeCommitRange(uint8_t *start, size_t size)
  {
#if defined(_WIN32)
    const size_t kChunkSize = 64 * 1024;
    for (size_t offset = 0; offset < size; offset += kChunkSize)
    {
      MEMORY_BASIC_INFORMATION mbi{};
      if (VirtualQuery(start + offset, &mbi, sizeof(mbi)))
      {
        if (mbi.State == MEM_RESERVE)
        {
          VirtualAlloc(start + offset, kChunkSize, MEM_COMMIT, PAGE_READWRITE);
        }
      }
    }
#endif
  }

  void PrecommitMemoryAliases()
  {
#if defined(_WIN32)
    uint8_t *base = rex::Runtime::instance() ? rex::Runtime::instance()->virtual_membase() : nullptr;
    if (!base)
    {
      return;
    }
    // Commit physical, cached and uncached write-combined Xbox 360 memory ranges (512MB each alias)
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
    uint8_t *curr = base + 0x82000000ull;
    uint8_t *end = base + 0x84000000ull;
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

    // Start real-time background log flusher (every 500ms) to ensure logs are never lost
    static std::atomic<bool> s_flush_running{false};
    if (!s_flush_running.exchange(true))
    {
      std::thread([]() {
        while (true)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          rex::FlushLogging();
        }
      }).detach();
    }

    auto *rt = rex::Runtime::instance();
    auto *fd = rt ? rt->function_dispatcher() : nullptr;
    if (fd)
    {
      // Intercept NULL indirect function pointer calls
      fd->SetFunction(0x00000000, [](PPCContext &ctx, uint8_t *base) {
        REXLOG_WARN("[fix] Ignored call to NULL guest function pointer (Caller LR=0x{:08X})", (uint32_t)ctx.lr);
        ctx.r3.s64 = 0;
      });

      // Intercept KeBugCheck to diagnose guest kernel panics
      fd->SetFunction(0x82B2FD90, [](PPCContext &ctx, uint8_t *base) {
        REXLOG_ERROR("=== GUEST KERNEL PANIC: KeBugCheck ===");
        REXLOG_ERROR("  BugCheckCode: 0x{:08X}", ctx.r3.u32);
        REXLOG_ERROR("  Caller LR:    0x{:08X}", (uint32_t)ctx.lr);
        rex::FlushLogging();

        FILE *f = std::fopen("logs/bugcheck.txt", "w");
        if (f)
        {
          std::fprintf(f, "KeBugCheck: Code=0x%08X (LR=0x%08llX)\n",
                       ctx.r3.u32, (unsigned long long)ctx.lr);
          std::fclose(f);
        }
        __imp__KeBugCheck(ctx, base);
      });

      // Intercept KeBugCheckEx to diagnose guest kernel panics
      fd->SetFunction(0x82B30170, [](PPCContext &ctx, uint8_t *base) {
        REXLOG_ERROR("=== GUEST KERNEL PANIC: KeBugCheckEx ===");
        REXLOG_ERROR("  BugCheckCode: 0x{:08X}", ctx.r3.u32);
        REXLOG_ERROR("  Param 1:      0x{:08X}", ctx.r4.u32);
        REXLOG_ERROR("  Param 2:      0x{:08X}", ctx.r5.u32);
        REXLOG_ERROR("  Param 3:      0x{:08X}", ctx.r6.u32);
        REXLOG_ERROR("  Param 4:      0x{:08X}", ctx.r7.u32);
        REXLOG_ERROR("  Caller LR:    0x{:08X}", (uint32_t)ctx.lr);
        rex::FlushLogging();

        FILE *f = std::fopen("logs/bugcheck.txt", "w");
        if (f)
        {
          std::fprintf(f, "KeBugCheckEx: Code=0x%08X (r4=0x%08X, r5=0x%08X, r6=0x%08X, r7=0x%08X, LR=0x%08llX)\n",
                       ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, (unsigned long long)ctx.lr);
          std::fclose(f);
        }
        __imp__KeBugCheckEx(ctx, base);
      });
    }

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
