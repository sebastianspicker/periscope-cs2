#include "real/win/veh_anti_debug.hpp"
#include "real/win/api_table.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include <cstdint>
#include <intrin.h>

namespace real::win {
namespace {

// AddVectoredExceptionHandler returns PVOID handle — store as-is.
static PVOID s_veh_handle = nullptr;
static uint64_t s_registrationTick = 0;
static bool s_first_handler = true;  // 1 = call first

static uint64_t read_tick() noexcept {
#if LR_ARCH_X64 || LR_ARCH_X86
  unsigned int aux = 0;
  return __rdtscp(&aux);
#else
  return 0;
#endif
}

static LONG CALLBACK veh_handler(PEXCEPTION_POINTERS ExceptionInfo) noexcept {
  if (!ExceptionInfo || !ExceptionInfo->ExceptionRecord)
    return EXCEPTION_CONTINUE_SEARCH;

  auto code = ExceptionInfo->ExceptionRecord->ExceptionCode;

  // Swallow single-step / breakpoint exceptions used by debuggers.
  if (code == EXCEPTION_SINGLE_STEP || code == EXCEPTION_BREAKPOINT) {
    // Clear trap flag if context available
    if (ExceptionInfo->ContextRecord) {
#if LR_ARCH_X64
      ExceptionInfo->ContextRecord->EFlags &= ~0x100u;  // TF
#elif LR_ARCH_X86
      ExceptionInfo->ContextRecord->EFlags &= ~0x100u;
#endif
    }
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  // Hardware breakpoint related STATUS_SINGLE_STEP already covered.
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // anonymous namespace

void register_veh() noexcept {
  if (s_veh_handle) return;

  auto& api = g_Api();
  api.ensure_resolved();

  if (api.AddVectoredExceptionHandler) {
    s_veh_handle = api.AddVectoredExceptionHandler(
        s_first_handler ? 1u : 0u, veh_handler);
  } else {
    s_veh_handle = ::AddVectoredExceptionHandler(s_first_handler ? 1u : 0u, veh_handler);
  }
  s_registrationTick = read_tick();
}

void unregister_veh() noexcept {
  if (!s_veh_handle) return;

  auto& api = g_Api();
  if (api.RemoveVectoredExceptionHandler) {
    api.RemoveVectoredExceptionHandler(s_veh_handle);
  } else {
    ::RemoveVectoredExceptionHandler(s_veh_handle);
  }
  s_veh_handle = nullptr;
}

bool veh_is_registered() noexcept { return s_veh_handle != nullptr; }

void veh_cycle() noexcept {
  if (!s_veh_handle) {
    register_veh();
    return;
  }

  uint64_t tick = read_tick();
  uint64_t elapsed = tick - s_registrationTick;

  // ~3-8 seconds at ~2 GHz TSC, with jitter
  uint64_t cycle_interval = 6ULL * 2000000000ULL;
  uint64_t jitter = (tick & 0x1FFFFFFFULL) % (cycle_interval / 2 + 1);
  cycle_interval += jitter;

  if (elapsed < cycle_interval) return;

  unregister_veh();

  // Brief unregistered window (~10-50ms worth of TSC)
  volatile uint64_t unreg_window = 10000000ULL + ((tick & 0xFFFF) * 1000ULL);
  volatile uint64_t wait_start = read_tick();
  while ((read_tick() - wait_start) < unreg_window) {
    _mm_pause();
  }

  // Alternate first/last registration to evade position-based probes
  s_first_handler = !s_first_handler;
  register_veh();
}

}  // namespace real::win

#else
namespace real::win {
void register_veh() noexcept {}
void unregister_veh() noexcept {}
void veh_cycle() noexcept {}
bool veh_is_registered() noexcept { return false; }
}  // namespace real::win
#endif
