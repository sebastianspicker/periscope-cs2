#include "real/win/heavens_gate.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS

#include "real/win/api_table.hpp"
#include "real/win/windows_h.hpp"

#include <cstring>

#if LR_ARCH_X64
#include "real/win/syscall_helper.hpp"
#endif

namespace real::win {

bool is_wow64_process() noexcept {
#if LR_ARCH_X64
  // Native x64 process is never WOW64.
  // Still check IsWow64Process for completeness if someone injects us oddly.
  BOOL wow = FALSE;
  using IsWow64Process_t = BOOL(WINAPI*)(HANDLE, PBOOL);
  static IsWow64Process_t fn = nullptr;
  if (!fn) {
    HMODULE k32 = ::GetModuleHandleW(L"kernel32.dll");
    if (k32) fn = reinterpret_cast<IsWow64Process_t>(::GetProcAddress(k32, "IsWow64Process"));
  }
  if (fn && fn(::GetCurrentProcess(), &wow)) return wow != FALSE;
  return false;
#elif LR_ARCH_X86
  BOOL wow = FALSE;
  using IsWow64Process_t = BOOL(WINAPI*)(HANDLE, PBOOL);
  HMODULE k32 = ::GetModuleHandleW(L"kernel32.dll");
  auto fn = k32 ? reinterpret_cast<IsWow64Process_t>(::GetProcAddress(k32, "IsWow64Process"))
                : nullptr;
  if (fn && fn(::GetCurrentProcess(), &wow)) return wow != FALSE;
  return false;
#else
  return false;
#endif
}

bool heavens_gate_available() noexcept {
#if LR_ARCH_X86
  return is_wow64_process();
#else
  // Native x64: Heaven's Gate not applicable; report false.
  return false;
#endif
}

HeavensGateReport heavens_gate_probe() noexcept {
  HeavensGateReport rep{};
  rep.wow64_process = is_wow64_process();
  rep.gate_available = heavens_gate_available();

#if LR_ARCH_X64
  // Fall through to indirect syscall path — the modern equivalent.
  auto& table = syscalls();
  if (table.NtClose.number < 0) {
    rep.detail = "SSN unresolved";
    return rep;
  }
  // NtClose(nullptr) → STATUS_INVALID_HANDLE (0xC0000008) — proves syscall works
  NTSTATUS st = syscall_4(table.NtClose.number, 0, 0, 0, 0);
  rep.used_native_syscall = true;
  rep.result = static_cast<std::uint64_t>(static_cast<std::uint32_t>(st));
  rep.detail = "x64 native indirect syscall (Heaven's Gate N/A)";
  return rep;
#elif LR_ARCH_X86
  if (!rep.gate_available) {
    rep.detail = "not a WOW64 process";
    return rep;
  }
  // WOW64 Heaven's Gate: far jump to 33: (x64 code segment).
  // The segment selector 0x33 is the 64-bit user CS on Windows WOW64.
  // We execute a minimal x64 stub that does `mov eax, C0000008h; retf` style
  // return via the WOW64 transition.
  //
  // Implementation note: MSVC x86 cannot emit 64-bit instructions inline.
  // We use a small shellcode buffer marked executable that performs:
  //   mov eax, SSN_NtClose (resolved from 64-bit ntdll via wow64cpu)
  //   For educational builds we invoke the 64-bit ntdll through the
  //   documented wow64 transition using X86SwitchTo64BitMode export when
  //   available; otherwise report structured unavailability.

  using Fn = void (*)();
  HMODULE wow64cpu = ::GetModuleHandleW(L"wow64cpu.dll");
  if (!wow64cpu) {
    // Try load
    wow64cpu = ::LoadLibraryW(L"wow64cpu.dll");
  }
  if (!wow64cpu) {
    rep.detail = "wow64cpu.dll unavailable";
    return rep;
  }

  // TurboThunks / Godzilla gate: call through heavent's classic 0x33 far jump
  // encoded as shellcode. Allocate RWX, write gate, execute, free.
  // Shellcode (x86):
  //   push 0x33
  //   call $+5
  //   add dword [esp], (x64_code - return)
  //   retf
  // x64_code:
  //   xor eax, eax
  //   mov al, 0x08  ; low byte marker
  //   retf

  std::uint8_t shell[] = {
      // x86: push 0x33; call next; add [esp], 0x0A; retf
      0x6A, 0x33,              // push 0x33
      0xE8, 0x00, 0x00, 0x00, 0x00,  // call $+5
      0x83, 0x04, 0x24, 0x0A,  // add dword [esp], 10
      0xCB,                    // retf
      // x64 (entered via CS=0x33): xor eax,eax; mov al,0x08; retf (6 bytes-ish)
      // Actually in 64-bit mode retf is still valid for wow64 return.
      0x48, 0x31, 0xC0,  // xor rax, rax
      0xB0, 0x08,        // mov al, 8
      0xCB,              // retf
  };

  void* mem = ::VirtualAlloc(nullptr, sizeof(shell), MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE);
  if (!mem) {
    rep.detail = "VirtualAlloc failed";
    return rep;
  }
  std::memcpy(mem, shell, sizeof(shell));
  // Execute — may fault on modern hardened WOW64; catch via SEH
  std::uint32_t gate_result = 0;
  __try {
    auto f = reinterpret_cast<std::uint32_t (*)()>(mem);
    gate_result = f();
    rep.used_gate = true;
    rep.result = gate_result;
    rep.detail = "Heaven's Gate far-jump executed";
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    rep.detail = "Heaven's Gate shellcode faulted (CFG/CIG?)";
  }
  ::VirtualFree(mem, 0, MEM_RELEASE);
  return rep;
#else
  rep.detail = "unsupported arch";
  return rep;
#endif
}

}  // namespace real::win

#else

namespace real::win {
bool is_wow64_process() noexcept { return false; }
bool heavens_gate_available() noexcept { return false; }
HeavensGateReport heavens_gate_probe() noexcept {
  HeavensGateReport r{};
  r.detail = "non-Windows";
  return r;
}
}  // namespace real::win

#endif
