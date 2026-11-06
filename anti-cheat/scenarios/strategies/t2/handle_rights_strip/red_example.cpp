#include "red_example.hpp"

#include <cstdio>

namespace examples::handle_rights_strip {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game || game->memory.empty()) {
    std::printf("[T2 handle_rights_strip] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T2 handle_rights_strip] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("handle-strip-actor.exe");
  if (!w.proc(actor)) {
    std::printf("[T2 handle_rights_strip] FAIL: lab actor could not start\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T2 handle_rights_strip] step %d: actor pid=%u created\n", ++steps, actor);

  // Clean slate: the attach path is only interesting when no handle edge exists
  // for blue to downgrade.
  for (const auto& handle : w.handles_to(game_pid)) {
    if (handle.owner_pid == actor) {
      std::printf("[T2 handle_rights_strip] FAIL: actor already holds a handle\n");
      return {false, steps, "precondition failed: actor already holds a handle"};
    }
  }
  std::printf("[T2 handle_rights_strip] step %d: no handle edge for actor (clean slate)\n", ++steps);

  // KeStackAttachProcess-style context switch: red maps the game's address
  // space into its own context and reads without OpenProcess at all.
  w.kernel_attach_read_active = true;
  std::printf("[T2 handle_rights_strip] step %d: kernel attach context established\n", ++steps);

  const auto table_rel = static_cast<std::uint64_t>(w.lab_entity_table_rel);
  auto read = w.read_mem(actor, game_pid, game->base + table_rel, 4, false);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T2 handle_rights_strip] FAIL: attach-context read failed\n");
    return {false, steps, "attach-context read failed"};
  }
  std::uint32_t entity_count = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    entity_count |= static_cast<std::uint32_t>(read.bytes[i]) << (8 * i);
  }
  ++w.kernel_attach_read_ops;
  std::printf("[T2 handle_rights_strip] step %d: attach read decoded entity_count=%u\n",
              ++steps, entity_count);

  if (entity_count == 0) {
    return {false, steps, "attach read returned no entity data"};
  }

  // Scar verification: reads happened with zero VM_READ handles in the graph,
  // which is exactly the traffic a handle-rights strip cannot see.
  bool any_foreign_vm_read = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      any_foreign_vm_read = true;
    }
  }
  if (any_foreign_vm_read || w.kernel_attach_read_ops == 0) {
    std::printf("[T2 handle_rights_strip] FAIL: attach scar not retained\n");
    return {false, steps, "scar verification failed: attach path without handle"};
  }
  std::printf("[T2 handle_rights_strip] step %d: attach-read scar planted (no handle edge)\n", ++steps);

  const std::string detail =
      "handle_rights_strip: attach read ops=" + std::to_string(w.kernel_attach_read_ops) +
      " entity_count=" + std::to_string(entity_count) + " vm_read_handles=0";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::handle_rights_strip
