#include "red_example.hpp"

#include <algorithm>
#include <cstring>

namespace examples::physmem_direct_read {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, false,
                   "PhysicalMemory direct-read precondition failed"};
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game_pid || !game || game->memory.empty() || w.ranked_access_denied) {
    return result;
  }
  const auto game_base = game->base;
  std::uint32_t entity_count = 0;
  const auto entity_table = static_cast<std::size_t>(w.lab_entity_table_rel);
  if (entity_table + sizeof(entity_count) > game->memory.size()) {
    result.detail = "PhysicalMemory direct-read entity table unavailable";
    return result;
  }
  std::memcpy(&entity_count, game->memory.data() + entity_table,
              sizeof(entity_count));

  const auto reader_pid = w.spawn("mem-mapper.exe");
  if (!w.proc(reader_pid)) {
    result.detail = "PhysicalMemory direct-read reader could not start";
    return result;
  }
  ++result.steps;

  if (w.trust.dse_enforced) {
    result.detail = "PhysicalMemory direct-read blocked by DSE policy";
    return result;
  }

  w.physmem_device_open = true;
  result.via_physical_device = true;
  ++result.steps;

  w.physmem_game_pfn = game_base >> 12;
  w.physmem_direct_mapped = true;
  ++result.steps;

  const bool no_game_vm_read_handle = std::none_of(
      w.handles.begin(), w.handles.end(), [&](const sim::Handle& handle) {
        return handle.target_pid == game_pid &&
               sim::has(handle.access, sim::AccessMask::VmRead);
      });
  const bool entities_read = w.physmem_game_pfn != 0 && entity_count > 0;
  if (!entities_read || !no_game_vm_read_handle) {
    result.detail = "PhysicalMemory mapping did not preserve handle-free reads";
    return result;
  }

  ++result.steps;
  result.achieved = w.physmem_direct_mapped && entities_read &&
                    no_game_vm_read_handle;
  result.detail =
      "PhysicalMemory mapped game PFN and read entities without a VM_READ handle";
  return result;
}

}  // namespace examples::physmem_direct_read
