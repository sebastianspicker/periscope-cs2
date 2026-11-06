#include "red_example.hpp"

#include <cstdio>

namespace examples::byovd_service_diff {

RedResult apply(sim::World& w) {
  RedResult result;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    return {false, 0, "precondition failed: game process unavailable"};
  }

  const auto reader = w.spawn("byovd-reader.exe");
  if (!w.proc(reader)) return {false, 0, "reader spawn verification failed"};
  result.steps = 1;

  // Load a signed but known-bad driver and open its mem-rw device.
  sim::Driver drv{"cleanup-memrw.sys", "cleanup_memrw_hash", "Cleanup Labs",
                  false, true, false, false, true, 80};
  w.load_driver(std::move(drv));
  w.create_device({"\\\\.\\cleanup-memrw", "cleanup-memrw.sys", true});
  w.escalation_driver_loaded = true;
  result.steps = 2;

  std::vector<std::uint8_t> bytes;
  if (!w.device_ioctl_read(reader, "\\\\.\\cleanup-memrw", game_pid,
                           game->base + w.lab_entity_table_rel, 4, bytes) ||
      bytes.size() != 4) {
    return {false, result.steps, "driver IOCTL read failed"};
  }
  result.steps = 3;

  // Cleanup: delete the SCM service and its registry key. This is what most
  // "forensic cleanup" routines stop at.
  w.byovd_service_cleaned = true;
  w.service_db_diff_count = 1;
  result.steps = 4;

  // The loaded-module list is not the service database. The image hash stays
  // until reboot, so the cleanup is incomplete by construction.
  w.loaded_module_hash_orphan = true;
  result.steps = 5;

  std::uint32_t entity_count = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    entity_count |= static_cast<std::uint32_t>(bytes[i]) << (8 * i);
  }
  result.achieved = w.byovd_service_cleaned && w.loaded_module_hash_orphan &&
                    entity_count != 0;
  result.detail = "byovd_service_diff: service_cleaned=1 module_orphan=1 "
                  "entity_count=" + std::to_string(entity_count);
  std::printf("[T2 byovd_service_diff] step %d: driver used, service cleaned\n",
              result.steps);
  w.note(result.detail);
  return result;
}

}  // namespace examples::byovd_service_diff
