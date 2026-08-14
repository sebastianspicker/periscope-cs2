#include "red_example.hpp"

#include <algorithm>
#include <cstring>

namespace examples::hypercall_mem_read {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, "", "hypercall read precondition failed"};
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game_pid || !game || game->memory.empty() || w.ranked_access_denied) {
    return result;
  }

  const auto reader_pid = w.spawn("hypercall-reader.exe");
  if (!w.proc(reader_pid)) {
    result.detail = "hypercall reader could not start";
    return result;
  }
  ++result.steps;

  if (!w.trust.platform_hv_active || w.trust.platform_hv_vendor.empty()) {
    result.detail = "no platform hypervisor hypercall interface is available";
    return result;
  }
  result.hv_vendor = w.trust.platform_hv_vendor;
  ++result.steps;

  if (w.trust.vbs) {
    result.detail = "VBS restricts the platform hypercall interface";
    return result;
  }

  const auto entity_table = static_cast<std::size_t>(w.lab_entity_table_rel);
  std::uint32_t entity_count = 0;
  if (entity_table + sizeof(entity_count) > game->memory.size()) {
    result.detail = "hypercall target page has no entity data";
    return result;
  }

  // Lab-only model: the exposed hypercall page returns synthetic entity data.
  w.hypercall_read_active = true;
  w.hypercall_vendor = result.hv_vendor;
  std::memcpy(&entity_count, game->memory.data() + entity_table,
              sizeof(entity_count));
  ++result.steps;

  const bool no_game_vm_read_handle = std::none_of(
      w.handles.begin(), w.handles.end(), [&](const sim::Handle& handle) {
        return handle.target_pid == game_pid &&
               sim::has(handle.access, sim::AccessMask::VmRead);
      });
  result.achieved = w.hypercall_read_active && entity_count > 0 &&
                    no_game_vm_read_handle;
  result.detail = "hypercall read entities=" + std::to_string(entity_count) +
                  " vendor=" + result.hv_vendor + " no_vm_read_handle=" +
                  std::to_string(no_game_vm_read_handle);
  return result;
}

}  // namespace examples::hypercall_mem_read
