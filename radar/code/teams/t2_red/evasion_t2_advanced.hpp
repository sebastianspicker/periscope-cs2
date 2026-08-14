#pragma once

#include "ac/types.hpp"
#include "sim/world.hpp"
#include "t2_red/kernel_radar.hpp"

#include <string>
#include <vector>

namespace t2_red {

struct AdvancedT2Report {
  bool driver_loaded = false;
  bool callbacks_stripped = false;
  bool dkom_stolen = false;
  bool physmem_mapped = false;
  bool acpi_read = false;
  bool hypercall_read = false;
  bool pool_hidden = false;
  bool wfp_installed = false;
  bool etw_ti_blind = false;
  bool instr_callback_stealth = false;
  bool hal_heap_exploit = false;
  bool null_ptr_deref = false;
  bool dpc_execution = false;
  int dpc_queue_count = 0;
  bool module_shadowing = false;
  int technique_count = 0;
  bool no_game_handle = true;
  std::vector<std::string> active_techniques;
  std::vector<std::string> remaining_scars;
  std::string detail;
};

class AdvancedT2Evasion {
 public:
  // Run max stealth: bring up driver + strip all callbacks + DKOM + physmem + WFP + pool +
  // ETW-TI blind + instrumentation callback stealth.
  // Returns a comprehensive report.
  static AdvancedT2Report max_kernel_stealth(
      sim::World& world, KernelRadar& radar,
      KernelPath path = KernelPath::Byovd);

  // Run every T2 simulation technique through the BYOVD driver path.
  static AdvancedT2Report deep_kernel_stealth(sim::World& world,
                                              KernelRadar& radar);
};

}  // namespace t2_red
