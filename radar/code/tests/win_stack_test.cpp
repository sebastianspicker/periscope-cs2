// win_stack_test.cpp — Real-path unit tests for lib/real/win.
// Drives shipped entry points (ApiTable, SyscallTable, SSN, stack, anti-debug,
// hook detect, ntdll restore, heavens gate probe, win_stack_init).
//
// No hard-coded SSN values: assertions check live resolution and live syscalls.

#include "real/platform.hpp"
#include "real/win/anti_debug.hpp"
#include "real/win/api_integrity.hpp"
#include "real/win/api_table.hpp"
#include "real/win/eat_util.hpp"
#include "real/win/heavens_gate.hpp"
#include "real/win/hook_detect.hpp"
#include "real/win/hwbp.hpp"
#include "real/win/ntdll_restore.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/ssn_resolve.hpp"
#include "real/win/stack_spoof.hpp"
#include "real/win/syscall_helper.hpp"
#include "real/win/syscall_ops.hpp"
#include "real/win/thread_hide.hpp"
#include "real/win/timing.hpp"
#include "real/win/veh_anti_debug.hpp"
#include "real/win/win_stack.hpp"
#include "real/win/xorstr.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int g_fails = 0;

static void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  } else {
    std::printf("OK: %s\n", msg);
  }
}

int main() {
  std::printf("=== win_stack_test (lib/real/win) ===\n");

#if !LR_PLATFORM_WINDOWS
  std::printf("SKIP: not Windows\n");
  return 0;
#else

  // ── 1) XOR string obfuscation ───────────────────────────────────
  {
    const char* s = OBF("NtClose");
    expect(s && std::strcmp(s, "NtClose") == 0, "OBF decrypts NtClose");
  }

  // ── 2) PEB module walk ──────────────────────────────────────────
  {
    uintptr_t ntdll = real::win::peb::find_module("ntdll");
    expect(ntdll != 0, "peb::find_module(ntdll)");
    uintptr_t k32 = real::win::peb::find_module("kernel32");
    expect(k32 != 0, "peb::find_module(kernel32)");
  }

  // ── 3) EAT resolve ──────────────────────────────────────────────
  {
    uintptr_t ntdll = real::win::peb::find_module("ntdll");
    void* p = real::win::eat::resolve_export(ntdll, "NtClose");
    expect(p != nullptr, "eat::resolve_export(NtClose)");
  }

  // ── 4) ApiTable ─────────────────────────────────────────────────
  {
    auto& api = real::win::g_Api();
    expect(api.resolve() || api.resolved, "g_Api().resolve()");
    api.ensure_resolved();
    expect(api.NtOpenProcess != nullptr, "ApiTable.NtOpenProcess");
    expect(api.NtReadVirtualMemory != nullptr, "ApiTable.NtReadVirtualMemory");
    expect(api.NtClose != nullptr, "ApiTable.NtClose");
    expect(api.NtDelayExecution != nullptr, "ApiTable.NtDelayExecution");
    expect(api.GetCurrentProcessId != nullptr, "ApiTable.GetCurrentProcessId");

    // Live call through resolved pointer
    DWORD pid = api.GetCurrentProcessId();
    expect(pid == ::GetCurrentProcessId(), "GetCurrentProcessId via ApiTable matches");
  }

  // ── 5) SSN resolution (Hell's/Halo's Gate) ──────────────────────
#if LR_ARCH_X64
  {
    auto r = real::win::resolve_ssn_robust("NtClose");
    expect(r.number >= 0, "resolve_ssn_robust(NtClose) >= 0");
    expect(r.method != real::win::SsnMethod::None, "SSN method set");
    std::printf("  NtClose SSN=%d method=%d hooked=%d\n", r.number,
                static_cast<int>(r.method), r.was_hooked ? 1 : 0);

    auto r2 = real::win::resolve_ssn_robust("NtReadVirtualMemory");
    expect(r2.number >= 0, "resolve_ssn_robust(NtReadVirtualMemory) >= 0");
    // On a clean system Hell's Gate should win; SSNs must differ for distinct calls
    expect(r.number != r2.number, "NtClose SSN != NtReadVirtualMemory SSN");
  }

  // ── 6) SyscallTable + live indirect syscall ─────────────────────
  {
    auto& sc = real::win::syscalls();
    expect(sc.NtClose.number >= 0, "syscalls().NtClose resolved");
    expect(sc.NtOpenProcess.number >= 0, "syscalls().NtOpenProcess resolved");
    expect(sc.NtReadVirtualMemory.number >= 0, "syscalls().NtReadVirtualMemory resolved");

    // NtClose(NULL) → STATUS_INVALID_HANDLE (0xC0000008)
    NTSTATUS st = real::win::syscall_4(sc.NtClose.number, 0, 0, 0, 0);
    expect(st == static_cast<NTSTATUS>(0xC0000008L) || st < 0,
           "syscall_4(NtClose,0) returns error status");
    std::printf("  NtClose(0) status=0x%08lX\n", static_cast<unsigned long>(st));
  }

  // ── 7) syscall_ops self-read ────────────────────────────────────
  {
    // Read our own MZ header via NtCurrentProcess path in syscall_direct
    auto* peb = real::win::peb::get_peb();
    expect(peb != nullptr, "get_peb()");
    auto base = reinterpret_cast<std::uint64_t>(peb->ImageBaseAddress);
    std::uint8_t buf[2]{};
    SIZE_T n = 0;
    NTSTATUS st = real::win::syscall_direct_NtReadVirtualMemory(
        reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)),
        reinterpret_cast<PVOID>(static_cast<uintptr_t>(base)), buf, 2, &n);
    expect(st >= 0 && n == 2, "syscall_direct_NtReadVirtualMemory self MZ");
    expect(buf[0] == 'M' && buf[1] == 'Z', "self image starts with MZ");
  }

  // ── 8) Heaven's Gate probe (x64 falls back to native syscall) ───
  {
    auto hg = real::win::heavens_gate_probe();
    expect(hg.used_native_syscall || hg.used_gate, "heavens_gate_probe executed path");
    expect(hg.result != 0 || hg.used_gate, "heavens_gate_probe produced result");
    std::printf("  heavens_gate: %s result=0x%llx\n", hg.detail,
                static_cast<unsigned long long>(hg.result));
  }
#endif  // LR_ARCH_X64

  // ── 9) Stack spoof gadgets ──────────────────────────────────────
  {
    auto& g = real::win::stack_spoof_gadgets();
    expect(g.ret_gadget != nullptr, "stack spoof ret gadget in ntdll");
    expect(g.jmp_rbx != nullptr || g.jmp_rax != nullptr, "stack spoof jmp gadget");
    expect(real::win::stack_spoof_ready(), "stack_spoof_ready");
    // spoof_call_1 plants a fake frame around a real call
    struct Local {
      static std::uint64_t xor_a5(std::uint64_t x) { return x ^ 0xA5A5ULL; }
    };
    std::uint64_t out =
        real::win::spoof_call_1(reinterpret_cast<void*>(&Local::xor_a5), 0x1234);
    expect(out == (0x1234ULL ^ 0xA5A5ULL), "spoof_call_1 round-trip");
  }

  // ── 10) Hook detection ──────────────────────────────────────────
  {
    real::win::init_hook_detection();
    auto rep = real::win::hook_detection_report();
    expect(rep.ready, "hook_detection ready");
    expect(rep.initialized > 0, "hook snapshots captured");
    // On a clean lab box check_hooks should be false (no EDR)
    std::printf("  hooks tracked=%d init=%d hooked=%d check=%d\n", rep.tracked,
                rep.initialized, rep.hooked, real::win::check_hooks() ? 1 : 0);
  }

  // ── 11) API integrity ───────────────────────────────────────────
  {
    auto rep = real::win::verify_api_table();
    expect(rep.checked >= 5, "verify_api_table checked >= 5");
    // ntdll critical path must be clean; kernel32 may be forwarded/hotpatched
    int ntdll_hooks = 0;
    for (int i = 0; i < rep.finding_count; ++i) {
      const auto& f = rep.findings[i];
      std::printf("  finding: %s!%s hook=%d ok=%d\n", f.module, f.function,
                  static_cast<int>(f.hook), f.ok ? 1 : 0);
      if (f.module && std::strcmp(f.module, "ntdll") == 0 && !f.ok) ++ntdll_hooks;
    }
    expect(ntdll_hooks == 0, "verify_api_table: no ntdll integrity failures");
    std::printf("  integrity checked=%d failed=%d hooked=%d ntdll_hooks=%d\n",
                rep.checked, rep.failed, rep.hooked, ntdll_hooks);
  }

  // ── 12) Anti-debug / HWBP / thread hide ─────────────────────────
  {
    real::win::clear_debug_flags();
    // Without an attached debugger this must be false
    expect(!real::win::check_debugger_ntqsi() || ::IsDebuggerPresent(),
           "check_debugger_ntqsi consistent with environment");
    expect(!real::win::hwbp_detected(), "no hardware breakpoints by default");
    expect(real::win::hwbp_clear(), "hwbp_clear succeeds");
    expect(real::win::hide_current_thread(), "hide_current_thread");
  }

  // ── 13) VEH register/unregister ─────────────────────────────────
  {
    real::win::register_veh();
    expect(real::win::veh_is_registered(), "veh registered");
    real::win::unregister_veh();
    expect(!real::win::veh_is_registered(), "veh unregistered");
  }

  // ── 14) ntdll restore (idempotent clean path) ───────────────────
  {
    int before = static_cast<int>(real::win::count_ntdll_hooks());
    auto rep = real::win::restore_ntdll_text();
    expect(rep.mapped_clean, "restore_ntdll_text mapped clean image");
    expect(rep.text_restored, "restore_ntdll_text restored .text");
    std::printf("  ntdll restore: patched=%zu hooks_before=%d hooks_after=%zu detail=%s\n",
                rep.bytes_patched, before, real::win::count_ntdll_hooks(), rep.detail);
  }

  // ── 15) fuzzed_sleep (short) ────────────────────────────────────
  {
    real::win::fuzzed_sleep(1, 10);
    real::win::yield();
    expect(true, "fuzzed_sleep + yield");
  }

  // ── 16) win_stack_init orchestration ────────────────────────────
  {
    real::win::WinStackOptions opts{};
    opts.restore_ntdll = true;
    opts.clear_debug = true;
    opts.register_veh = true;
    opts.init_hook_detect = true;
    opts.apply_etw_blind = false;
    opts.hide_module = false;
    auto rep = real::win::win_stack_init(opts);
    expect(rep.api_resolved, "win_stack_init api_resolved");
#if LR_ARCH_X64
    expect(rep.syscalls_resolved, "win_stack_init syscalls_resolved");
    expect(rep.ssn_nt_close >= 0, "win_stack_init ssn_nt_close");
#endif
    expect(rep.hooks_initialized, "win_stack_init hooks_initialized");
    expect(real::win::win_stack_ready(), "win_stack_ready");
    std::printf("  win_stack: api=%d sc=%d ssn_close=%d veh=%d spoof=%d detail=%s\n",
                rep.api_resolved ? 1 : 0, rep.syscalls_resolved ? 1 : 0, rep.ssn_nt_close,
                rep.veh_registered ? 1 : 0, rep.stack_spoof_ready ? 1 : 0, rep.detail);
    real::win::win_stack_shutdown();
  }

  std::printf("\n=== %s (%d failures) ===\n", g_fails ? "FAILED" : "PASSED", g_fails);
  return g_fails ? 1 : 0;
#endif
}
