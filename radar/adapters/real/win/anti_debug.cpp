#include "real/win/anti_debug.hpp"
#include "real/win/api_table.hpp"
#include "real/win/hwbp.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/thread_hide.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include <cstdint>
#include <intrin.h>

namespace real::win {
namespace {

#if LR_ARCH_X64
std::uint8_t* peb_bytes() noexcept {
  return reinterpret_cast<std::uint8_t*>(peb::get_peb());
}
#elif LR_ARCH_X86
std::uint8_t* peb_bytes() noexcept {
  return reinterpret_cast<std::uint8_t*>(
      static_cast<uintptr_t>(__readfsdword(0x30)));
}
#else
std::uint8_t* peb_bytes() noexcept { return nullptr; }
#endif

bool check_rdtsc_anomaly() noexcept {
  // Two closely spaced RDTSC reads; under single-step a huge delta appears.
  unsigned int aux = 0;
  std::uint64_t t0 = __rdtscp(&aux);
  // Tiny work
  volatile int x = 0;
  for (int i = 0; i < 16; ++i) x += i;
  std::uint64_t t1 = __rdtscp(&aux);
  (void)x;
  // Threshold: single-stepping typically yields multi-million cycle gaps.
  // Normal: a few hundred to a few thousand. Use a conservative 5e6.
  return (t1 - t0) > 5000000ULL;
}

}  // namespace

void clear_debug_flags() noexcept {
  auto* peb = peb_bytes();
  if (!peb) return;

  // BeingDebugged
  peb[0x02] = 0;

#if LR_ARCH_X64
  // NtGlobalFlag at PEB+0xBC on older docs; on x64 Windows 10+ it is 0xBC.
  // Also clear the well-known 0x68 offset used by many references (ProcessParameters
  // adjacent region varies). Clear both the classic 0x68 and 0xBC slots.
  *reinterpret_cast<std::uint32_t*>(peb + 0xBC) &=
      ~static_cast<std::uint32_t>(0x70);  // heap tail/free/validate bits
  // Some toolchains still document 0x68 for NtGlobalFlag on x64 — clear if set.
  *reinterpret_cast<std::uint32_t*>(peb + 0x68) &=
      ~static_cast<std::uint32_t>(0x70);

  // Heap flags
  auto* process_heap = *reinterpret_cast<void**>(peb + 0x30);  // x64 ProcessHeap
  if (process_heap) {
    // Windows 10+ heap: Flags @ +0x70, ForceFlags @ +0x74
    auto* heap_flags =
        reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(process_heap) + 0x70);
    *heap_flags = 2;  // HEAP_GROWABLE
    auto* heap_force =
        reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(process_heap) + 0x74);
    *heap_force = 0;
  }
#elif LR_ARCH_X86
  *reinterpret_cast<std::uint32_t*>(peb + 0x68) &=
      ~static_cast<std::uint32_t>(0x70);
  auto* process_heap = *reinterpret_cast<void**>(peb + 0x18);
  if (process_heap) {
    auto* heap_flags =
        reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(process_heap) + 0x40);
    *heap_flags = 2;
    auto* heap_force =
        reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(process_heap) + 0x44);
    *heap_force = 0;
  }
#endif

  // Hide this thread from debugger enumeration
  hide_current_thread();

  // Clear hardware breakpoints
  hwbp_clear();
}

bool check_debugger_ntqsi() noexcept {
  // PEB BeingDebugged
  auto* peb = peb_bytes();
  if (peb && peb[0x02] != 0) return true;

  // Hardware breakpoints
  if (hwbp_detected()) return true;

  // Timing anomaly under single-step
  if (check_rdtsc_anomaly()) return true;

  auto& api = g_Api();
  if (!api.resolved || !api.NtQueryInformationProcess) return false;

  HANDLE self = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1));

  // ProcessDebugPort (7)
  ULONG_PTR debugPort = 0;
  NTSTATUS status =
      api.NtQueryInformationProcess(self, 7, &debugPort, sizeof(debugPort), nullptr);
  if (status >= 0 && debugPort != 0) return true;

  // ProcessDebugObjectHandle (30)
  HANDLE debugObject = nullptr;
  status =
      api.NtQueryInformationProcess(self, 30, &debugObject, sizeof(debugObject), nullptr);
  if (status >= 0 && debugObject != nullptr) {
    if (api.NtClose) api.NtClose(debugObject);
    return true;
  }

  // ProcessDebugFlags (31) — 0 means debugger present
  ULONG debugFlags = 1;
  status =
      api.NtQueryInformationProcess(self, 31, &debugFlags, sizeof(debugFlags), nullptr);
  if (status >= 0 && debugFlags == 0) return true;

  return false;
}

bool check_debugger_present() noexcept {
  if (check_debugger_ntqsi()) return true;
#if defined(WIN32) || defined(_WIN32)
  if (::IsDebuggerPresent()) return true;
  BOOL remote = FALSE;
  if (::CheckRemoteDebuggerPresent(::GetCurrentProcess(), &remote) && remote)
    return true;
#endif
  return false;
}

}  // namespace real::win

#else
namespace real::win {
void clear_debug_flags() noexcept {}
bool check_debugger_ntqsi() noexcept { return false; }
bool check_debugger_present() noexcept { return false; }
}  // namespace real::win
#endif
