#include "blue/blue_system.hpp"
#include "blue/blue_system_internal.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

namespace blue {
using namespace blue_detail;

BlueViewResult BlueCoordinator::check_handle_table() {
  BlueViewResult result{ac::ObservationView::HandleTable, false, 0.0, "ok", {}};
  const auto game_pid = world_.game_pid();
  if (game_pid == 0) {
    finalize_view(result, view_sensitivity(ac::ObservationView::HandleTable));
    return result;
  }

  // Continuous truth: include handles red hid during enum sampling.
  const auto handles = world_.handles_to(game_pid, /*include_hidden=*/true);
  int foreign_vm_read = 0;
  int hidden_vm_read = 0;
  int proxy_handles = 0;
  int syscall_opens = 0;
  std::uint32_t example_pid = 0;

  for (const auto& handle : handles) {
    const auto* owner = world_.proc(handle.owner_pid);
    if (owner == nullptr || owner->is_game || owner->is_ac) {
      continue;
    }
    const bool vm = sim::has(handle.access, sim::AccessMask::VmRead);
    if (!vm && !sim::has(handle.access, sim::AccessMask::VmWrite) &&
        !sim::has(handle.access, sim::AccessMask::VmOperation)) {
      continue;
    }
    if (vm) {
      ++foreign_vm_read;
      if (example_pid == 0) {
        example_pid = handle.owner_pid;
      }
    }
    if (handle.hidden_during_enum && vm) {
      ++hidden_vm_read;
    }
    if (handle.via_proxy) {
      ++proxy_handles;
    }
    if (handle.via_syscall_path) {
      ++syscall_opens;
    }
  }

  // Inherited / proxy graph scars on World.
  for (const auto& handle : world_.inherited_handles) {
    if (handle.target_pid == game_pid &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      ++proxy_handles;
    }
  }

  if (foreign_vm_read >= 1) {
    add_reason(result,
               "foreign_vm_read count=" + std::to_string(foreign_vm_read) +
                   " e.g. pid=" + std::to_string(example_pid),
               0.55 + 0.08 * std::min(foreign_vm_read, 3));
  }
  if (hidden_vm_read >= 1) {
    add_reason(result, "hidden_during_enum vm_read=" + std::to_string(hidden_vm_read),
               0.35);
  }
  if (proxy_handles >= 1 || world_.handle_proxy_active) {
    add_reason(result,
               "handle_proxy active=" +
                   std::string(world_.handle_proxy_active ? "1" : "0") +
                   " edges=" + std::to_string(proxy_handles),
               0.40);
  }
  if (world_.donor_hijack_active || world_.donor_handle_duplicated) {
    add_reason(result,
               "donor_hijack donor=" + std::to_string(world_.donor_hijack_donor_pid) +
                   " consumer=" + std::to_string(world_.donor_hijack_consumer_pid),
               0.50);
  }
  if (world_.multi_process_split_active && world_.split_holder_pid != 0) {
    add_reason(result,
               "multi_process_split holder=" + std::to_string(world_.split_holder_pid),
               0.35);
  }
  if (syscall_opens >= 1) {
    add_reason(result, "syscall_path_opens=" + std::to_string(syscall_opens), 0.15);
  }
  if (world_.sedebug_privilege) {
    add_reason(result, "sedebug_privilege", 0.25);
  }
  if (world_.heavens_gate_transition) {
    add_reason(result,
               "heavens_gate stub=" + std::to_string(world_.heavens_gate_stub_pid),
               0.30);
  }
  if (world_.dkom_token_stolen || world_.token_bypasses_handle_acls) {
    add_reason(result, "dkom_token_steal", 0.45);
  }

  // Brief reopen race residual: count brief_reopen edges to game.
  int brief = 0;
  for (const auto& handle : handles) {
    if (handle.brief_reopen && sim::has(handle.access, sim::AccessMask::VmRead)) {
      ++brief;
    }
  }
  if (brief >= 1) {
    add_reason(result, "brief_reopen_handles=" + std::to_string(brief), 0.25);
  }

  finalize_view(result, view_sensitivity(ac::ObservationView::HandleTable), 0.40);
  return result;
}

// ── ModuleList: VAS / PEB / manual-map / integrity scars ────────────────────

BlueViewResult BlueCoordinator::check_module_list() {
  BlueViewResult result{ac::ObservationView::ModuleList, false, 0.0, "ok", {}};
  const auto* game = world_.proc(world_.game_pid());
  if (game == nullptr) {
    finalize_view(result, view_sensitivity(ac::ObservationView::ModuleList));
    return result;
  }

  int unlinked = 0;
  int headers_erased = 0;
  int dirty_text = 0;
  int iat_hooks = 0;
  int eat_hooks = 0;
  int present_hooks = 0;

  for (const auto& module : game->modules) {
    if (!module.linked_in_peb) {
      ++unlinked;
    }
    if (module.headers_erased) {
      ++headers_erased;
    }
    if (module.text_hash != "clean" && !module.text_hash.empty()) {
      ++dirty_text;
    }
    if (module.iat_hooked) {
      ++iat_hooks;
    }
    if (module.eat_hooked) {
      ++eat_hooks;
    }
    if (module.present_hooked) {
      ++present_hooks;
    }
  }

  if (unlinked >= 1) {
    add_reason(result, "peb_unlinked_modules=" + std::to_string(unlinked), 0.55);
  }
  if (headers_erased >= 1) {
    add_reason(result, "headers_erased=" + std::to_string(headers_erased), 0.45);
  }
  if (dirty_text >= 1) {
    add_reason(result, "dirty_text_hash=" + std::to_string(dirty_text), 0.40);
  }
  if (iat_hooks + eat_hooks >= 1) {
    add_reason(result,
               "iat_eat_hooks iat=" + std::to_string(iat_hooks) +
                   " eat=" + std::to_string(eat_hooks),
               0.50);
  }
  if (present_hooks >= 1) {
    add_reason(result, "present_hooked_modules=" + std::to_string(present_hooks),
               0.45);
  }
  if (game->manual_mapped_region) {
    add_reason(result, "manual_mapped_region", 0.55);
  }
  if (game->hollowed || world_.process_hollowing_active) {
    add_reason(result,
               "process_hollowing pid=" + std::to_string(world_.hollowed_pid), 0.60);
  }
  if (world_.module_is_stomped) {
    add_reason(result, "module_stomped", 0.50);
  }
  if (world_.mapper_process_present) {
    add_reason(result, "mapper_process_present", 0.35);
  }
  if (world_.module_shadowing_active) {
    add_reason(result, "kernel_module_shadow name=" + world_.shadowed_driver_name,
               0.40);
  }
  // Diagnostic module snapshot residuals.
  for (const auto& dm : world_.diagnostic_state.loaded_modules) {
    if (dm.suspicious) {
      add_reason(result, "diag_suspicious_module=" + dm.name, 0.40);
      break;
    }
  }
  // Extreme module bloat alone is a weak signal (tools can load many DLLs).
  if (game->modules.size() > 80) {
    add_reason(result, "high_module_count=" + std::to_string(game->modules.size()),
               0.20);
  }

  finalize_view(result, view_sensitivity(ac::ObservationView::ModuleList), 0.40);
  return result;
}

// ── MemoryPattern: RPM volume / scatter / DMA / physmem residuals ──────────


}  // namespace blue
