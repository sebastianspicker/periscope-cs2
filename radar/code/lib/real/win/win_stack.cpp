#include "real/win/win_stack.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS

#include "real/win/anti_debug.hpp"
#include "real/win/api_integrity.hpp"
#include "real/win/api_table.hpp"
#include "real/win/etw_blind.hpp"
#include "real/win/hook_detect.hpp"
#include "real/win/module_hide.hpp"
#include "real/win/ntdll_restore.hpp"
#include "real/win/pe_hide.hpp"
#include "real/win/stack_spoof.hpp"
#include "real/win/thread_hide.hpp"
#include "real/win/veh_anti_debug.hpp"

#if LR_ARCH_X64
#include "real/win/syscall_helper.hpp"
#endif

namespace real::win {
namespace {
bool s_ready = false;
WinStackReport s_last{};
WinStackOptions s_opts{};
}  // namespace

WinStackReport win_stack_init(const WinStackOptions& opts) noexcept {
  s_opts = opts;
  WinStackReport rep{};

  // 1) API table
  auto& api = g_Api();
  api.ensure_resolved();
  rep.api_resolved = api.resolved && api.NtOpenProcess != nullptr;

  // 2) Hook baseline (before ntdll restore so we can measure)
  if (opts.init_hook_detect) {
    init_hook_detection();
    rep.hooks_initialized = true;
  }

  rep.ntdll_hooks_before = static_cast<int>(count_ntdll_hooks());

  // 3) Optional clean ntdll restore
  if (opts.restore_ntdll) {
    auto nr = restore_ntdll_text();
    rep.ntdll_restored = nr.text_restored;
    if (opts.init_hook_detect) {
      refresh_hook_baseline();
    }
  }
  rep.ntdll_hooks_after = static_cast<int>(count_ntdll_hooks());

  // 4) Syscall SSN table
#if LR_ARCH_X64
  auto& sc = syscalls();
  rep.ssn_nt_read = sc.NtReadVirtualMemory.number;
  rep.ssn_nt_open = sc.NtOpenProcess.number;
  rep.ssn_nt_close = sc.NtClose.number;
  rep.syscalls_resolved =
      rep.ssn_nt_read >= 0 && rep.ssn_nt_open >= 0 && rep.ssn_nt_close >= 0;
#else
  rep.syscalls_resolved = false;
#endif

  // 5) Stack spoof gadgets
  rep.stack_spoof_ready = stack_spoof_ready();

  // 6) Anti-debug
  if (opts.clear_debug) {
    clear_debug_flags();
    hide_current_thread();
    rep.anti_debug_cleared = true;
  }

  // 7) VEH
  if (opts.register_veh) {
    register_veh();
    rep.veh_registered = veh_is_registered();
  }

  // 8) Optional ETW blind
  if (opts.apply_etw_blind) {
    etw_blind_apply();
  }

  // 9) Optional module hide
  if (opts.hide_module) {
    module_unlink(0);
  }

  // 10) Integrity check
  auto integ = verify_api_table();
  rep.api_integrity_failed = integ.failed;

  rep.detail = rep.api_resolved ? "ok" : "api resolve failed";
  s_last = rep;
  s_ready = rep.api_resolved;
  return rep;
}

void win_stack_shutdown() noexcept {
  if (etw_blind_active()) etw_blind_restore();
  if (veh_is_registered()) unregister_veh();
  if (pe_header_erased()) restore_pe_header();
  if (module_is_hidden()) module_relink();
  s_ready = false;
}

bool win_stack_ready() noexcept { return s_ready; }

const WinStackReport& win_stack_last_report() noexcept { return s_last; }

}  // namespace real::win

#else

namespace real::win {
WinStackReport win_stack_init(const WinStackOptions&) noexcept {
  return {};
}
void win_stack_shutdown() noexcept {}
bool win_stack_ready() noexcept { return false; }
const WinStackReport& win_stack_last_report() noexcept {
  static WinStackReport r{};
  return r;
}
}  // namespace real::win

#endif
