#include "real/win/thread_hide.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS

#include "real/win/api_table.hpp"
#include "real/win/windows_h.hpp"

namespace real::win {
namespace {
constexpr ULONG kThreadHideFromDebugger = 0x11;
}

bool hide_thread(void* thread_handle) noexcept {
  if (!thread_handle) return false;
  auto& api = g_Api();
  api.ensure_resolved();
  if (!api.NtSetInformationThread) return false;
  NTSTATUS st = api.NtSetInformationThread(static_cast<HANDLE>(thread_handle),
                                           kThreadHideFromDebugger, nullptr, 0);
  return st >= 0;
}

bool hide_current_thread() noexcept {
  // NtCurrentThread = (HANDLE)-2
  return hide_thread(reinterpret_cast<void*>(static_cast<intptr_t>(-2)));
}

bool is_current_thread_hidden() noexcept {
  auto& api = g_Api();
  api.ensure_resolved();
  // NtQueryInformationThread may not be in ApiTable — resolve from ntdll
  using NtQueryInformationThread_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
  static NtQueryInformationThread_t NtQueryInformationThread = nullptr;
  if (!NtQueryInformationThread) {
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
      NtQueryInformationThread = reinterpret_cast<NtQueryInformationThread_t>(
          ::GetProcAddress(ntdll, "NtQueryInformationThread"));
    }
  }
  if (!NtQueryInformationThread) {
    // Cannot query — if hide_current_thread was called we still return false.
    return false;
  }
  BOOLEAN hidden = FALSE;
  NTSTATUS st = NtQueryInformationThread(
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2)), kThreadHideFromDebugger,
      &hidden, sizeof(hidden), nullptr);
  return st >= 0 && hidden;
}

}  // namespace real::win

#else

namespace real::win {
bool hide_current_thread() noexcept { return false; }
bool hide_thread(void*) noexcept { return false; }
bool is_current_thread_hidden() noexcept { return false; }
}  // namespace real::win

#endif
