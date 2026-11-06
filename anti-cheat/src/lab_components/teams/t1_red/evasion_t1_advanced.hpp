#pragma once

// Advanced T1 evasion composition for the sim-only anti-cheat lab.

#include "t1_red/syscall_cheat.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t1_red {

struct AdvancedT1EvasionReport {
  bool hooks_detected = false;
  bool clean_ntdll_copy = false;
  bool heavens_gate = false;
  std::uint32_t heavens_gate_stub_pid = 0;
  bool dynamic_ssns = false;
  int resolved_ssn_windows_build = 0;
  bool enhanced_stack_spoof = false;
  int spoofed_call_depth = 0;
  int hooks_evaded_count = 0;
  bool etw_blind = false;
  bool sedebug = false;
  bool threshold_evasion = false;
  bool rop_chain_syscall = false;
  int rop_gadget_count = 0;
  bool hw_breakpoint_evasion = false;
  bool veh_cf_patched = false;
  int veh_handlers_modified = 0;
  bool dynamic_import_resolution = false;
  int dynamic_import_count = 0;
  std::vector<std::string> remaining_scars;
  std::string detail;
};

// Chains the advanced T1 scar surfaces without touching real OS APIs.
class AdvancedT1Evasion {
 public:
  explicit AdvancedT1Evasion(SyscallCheat& cheat) : cheat_(cheat) {}

  AdvancedT1EvasionReport apply(int windows_build = 22631, int spoof_depth = 4);

  /// Execute every T1 simulation scar in the deep-stealth lesson sequence.
  AdvancedT1EvasionReport deep_stealth();

 private:
  SyscallCheat& cheat_;
};

}  // namespace t1_red
