#include "blue_example.hpp"

#include <cstdio>

namespace examples::multi_process_ipc_split {

BlueResult detect(sim::World& w) {
  BlueResult result{};
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    result.detail = "game unavailable";
    return result;
  }

  bool foreign_vm_read_handle = false;
  bool audiodg_vm_read_handle = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      foreign_vm_read_handle = true;
      audiodg_vm_read_handle = audiodg_vm_read_handle || owner->name == "audiodg.exe";
    }
  }
  bool section_with_entities = false;
  for (const auto& section : w.sections) {
    section_with_entities = section_with_entities || section.carries_entity_bytes;
  }
  const bool holder_ui_mismatch = w.split_holder_pid != 0 && w.split_ui_pid != 0 &&
      w.split_holder_pid != w.split_ui_pid;

  if (w.multi_process_split_active) {
    result.reasons.emplace_back("strategy scar: multi-process split is active");
    result.risk += 0.20;
  }
  if (section_with_entities) {
    result.reasons.emplace_back("shared section carries entity bytes");
    result.risk += 0.20;
  }
  if (foreign_vm_read_handle) {
    result.reasons.emplace_back("foreign VM_READ handle on non-game/non-AC process");
    result.risk += 0.15;
  }
  if (holder_ui_mismatch) {
    result.reasons.emplace_back("handle holder and UI are distinct processes");
    result.risk += 0.15;
  }
  if (audiodg_vm_read_handle) {
    result.reasons.emplace_back("audiodg.exe holds a VM_READ handle");
    result.risk += 0.15;
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2;
  result.mitigated = result.risk >= 0.60;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "multi-process split risk=" + std::to_string(result.risk);
  std::printf("[T0 multi_process_ipc_split] BLUE signals=%d risk=%.2f detected=%s\n",
              result.signals, result.risk, result.detected ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T0 multi_process_ipc_split]   %s\n", reason.c_str());
  }
  return result;
}

}  // namespace examples::multi_process_ipc_split
