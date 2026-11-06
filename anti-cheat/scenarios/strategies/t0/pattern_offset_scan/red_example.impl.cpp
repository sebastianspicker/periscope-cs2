// red_example.impl.cpp -- deep, simulation-only CS2 pattern scan lesson.

#include "red_example.impl.hpp"

#include "lab/lab_memory.hpp"

#include <cstring>

namespace examples::pattern_offset_scan {

RedScanResult run_deep_scan(sim::World& world, std::uint32_t reader_pid) {
  RedScanResult result;
  const auto game_pid = world.game_pid();
  auto* game = world.proc(game_pid);
  if (!game || !game->is_game || !world.proc(reader_pid)) {
    result.detail = "precondition failed: game or reader process unavailable";
    return result;
  }

  lab::plant_cs2_pattern_markers(game->memory, game->base,
                                 cs2::SignatureDatabase::get());
  if (!world.open_process(reader_pid, game_pid, sim::AccessMask::VmRead, false)) {
    result.detail = "OpenProcess VM_READ failed";
    return result;
  }

  // Model the observable initial process-memory acquisition. The scanner then
  // operates only on this local snapshot, as an external RPM implementation does.
  const auto image = world.read_mem(reader_pid, game_pid, game->base,
                                    game->memory.size(), true);
  if (image.status != ac::Status::Ok) {
    result.detail = "game image read failed";
    return result;
  }
  lab::AobScanner scanner;
  scanner.set_memory_region(image.bytes.data(), image.bytes.size(), game->base);
  const auto report = scanner.scan_entity_radar_patterns();
  result.patterns_scanned = report.attempted_scans;
  result.patterns_found = report.found_count;
  for (const auto& operation : report.operations) {
    if (operation.found) result.resolved_patterns.push_back(operation.pattern_name);
  }
  if (result.patterns_found == 0) {
    result.detail = "no entity/radar CS2 signatures resolved";
    world.note("pattern_offset_scan deep scan found no CS2 signatures");
    return result;
  }

  // Signatures resolve the entity access path; the existing fixture table then
  // supplies the simulation's controller/pawn-derived entity rows.
  const auto table_address = game->base + world.lab_entity_table_rel;
  const auto count_read = world.read_mem(reader_pid, game_pid, table_address,
                                         sizeof(std::uint32_t), true);
  std::uint32_t entity_count = 0;
  if (count_read.status != ac::Status::Ok ||
      count_read.bytes.size() != sizeof(entity_count)) {
    result.detail = "entity count read failed after signature resolution";
    return result;
  }
  std::memcpy(&entity_count, count_read.bytes.data(), sizeof(entity_count));
  if (entity_count == 0 || entity_count > 64) {
    result.detail = "resolved entity table contains an invalid count";
    return result;
  }
  constexpr std::size_t kSimEntityRowSize = 16;
  const auto rows = world.read_mem(reader_pid, game_pid, table_address + 0x10,
                                   entity_count * kSimEntityRowSize, true);
  if (rows.status != ac::Status::Ok ||
      rows.bytes.size() != entity_count * kSimEntityRowSize) {
    result.detail = "entity entries read failed after signature resolution";
    return result;
  }
  result.entities_read = static_cast<int>(entity_count);
  result.achieved = true;
  result.detail = "CS2 AOB signatures resolved entity access, then RPM read entity rows";
  world.schema_cache_active = true;
  world.schema_cache_version = "cs2-signature-scan";
  world.note(result.detail + " patterns=" + std::to_string(result.patterns_found) +
             " entities=" + std::to_string(result.entities_read));
  return result;
}

}  // namespace examples::pattern_offset_scan
