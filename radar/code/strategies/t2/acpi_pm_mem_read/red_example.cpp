#include "red_example.hpp"

#include <algorithm>
#include <cstring>

namespace examples::acpi_pm_mem_read {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, 0, "ACPI PM read precondition failed"};
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game_pid || !game || game->memory.empty() || w.ranked_access_denied) {
    return result;
  }

  const auto reader_pid = w.spawn("acpi-reader.exe");
  if (!w.proc(reader_pid)) {
    result.detail = "ACPI PM reader could not start";
    return result;
  }
  ++result.steps;

  const auto entity_table = static_cast<std::size_t>(w.lab_entity_table_rel);
  std::uint32_t entity_count = 0;
  if (entity_table + sizeof(entity_count) > game->memory.size()) {
    result.detail = "ACPI scratch buffer has no game entity data";
    return result;
  }

  // Lab-only model: PM1_CNT writes cause a pre-existing SMM handler to copy
  // the synthetic entity count into an ACPI scratch-buffer equivalent.
  w.acpi_pm_read_active = true;
  w.acpi_smi_trigger_count = 5;
  w.smm_residual = true;
  result.smi_count = w.acpi_smi_trigger_count;
  ++result.steps;

  std::memcpy(&entity_count, game->memory.data() + entity_table,
              sizeof(entity_count));
  ++result.steps;

  const bool no_game_vm_read_handle = std::none_of(
      w.handles.begin(), w.handles.end(), [&](const sim::Handle& handle) {
        return handle.target_pid == game_pid &&
               sim::has(handle.access, sim::AccessMask::VmRead);
      });
  const bool entities_obtained = entity_count > 0;
  result.achieved = w.acpi_pm_read_active && result.smi_count == 5 &&
                    entities_obtained && no_game_vm_read_handle;
  result.detail = "ACPI PM SMI read entities=" +
                  std::to_string(entity_count) + " smi=" +
                  std::to_string(result.smi_count) +
                  " no_vm_read_handle=" +
                  std::to_string(no_game_vm_read_handle);
  return result;
}

}  // namespace examples::acpi_pm_mem_read
