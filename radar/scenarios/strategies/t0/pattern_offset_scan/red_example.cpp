#include "red_example.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>

namespace examples::pattern_offset_scan {
namespace {

RedResult scan(sim::World& w, ScannerSession* session, bool refreshed) {
  RedResult result;
  result.refreshed = refreshed;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game) {
    result.detail = "precondition failed: game process unavailable";
    return result;
  }
  ++result.steps;

  std::uint32_t actor = session ? session->actor_pid : 0;
  if (!actor || !w.proc(actor)) {
    actor = w.spawn("pattern_offset_scan-actor.exe");
    if (session) session->actor_pid = actor;
  }
  // Re-resolve game after spawn: unordered_map rehash invalidates Process*.
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.detail = "game process lost after actor spawn";
    return result;
  }
  const auto game_base = game->base;
  const auto game_mem_size = game->memory.size();

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    result.detail = "OpenProcess VM_READ failed";
    return result;
  }
  result.handle_opened = true;
  ++result.steps;

  const auto reads_before = w.remote_read_ops;
  // Prefer the World-owned marker offset to avoid scanning the full image.
  std::size_t marker_rel = 0;
  bool found = false;
  if (w.lab_pattern_marker_present &&
      w.lab_pattern_marker_off + 4 <= game_mem_size) {
    marker_rel = w.lab_pattern_marker_off;
    found = true;
  }

  const auto image = w.read_mem(actor, game_pid, game_base, game_mem_size, true);
  if (image.status != ac::Status::Ok) {
    result.detail = "pattern image read failed status=" +
                    std::to_string(static_cast<int>(image.status)) +
                    " size=" + std::to_string(game_mem_size);
    std::printf("[T0 pattern_offset_scan] RED early: %s\n", result.detail.c_str());
    return result;
  }
  if (!found) {
    const char magic[4] = {'A', 'C', 'P', 'T'};
    const auto marker = std::search(image.bytes.begin(), image.bytes.end(),
                                    magic, magic + 4);
    if (marker == image.bytes.end()) {
      result.detail = "ACPT pattern not found bytes=" +
                      std::to_string(image.bytes.size()) +
                      " marker_flag=" +
                      std::to_string(w.lab_pattern_marker_present ? 1 : 0);
      std::printf("[T0 pattern_offset_scan] RED early: %s\n", result.detail.c_str());
      return result;
    }
    marker_rel = static_cast<std::size_t>(marker - image.bytes.begin());
  } else {
    // Verify planted marker bytes when using known offset.
    if (image.bytes.size() < marker_rel + 4 ||
        image.bytes[marker_rel] != 'A' || image.bytes[marker_rel + 1] != 'C' ||
        image.bytes[marker_rel + 2] != 'P' || image.bytes[marker_rel + 3] != 'T') {
      // Fall back to full search if offset bytes are wrong.
      const char magic[4] = {'A', 'C', 'P', 'T'};
      const auto marker = std::search(image.bytes.begin(), image.bytes.end(),
                                      magic, magic + 4);
      if (marker == image.bytes.end()) {
        result.detail = "ACPT pattern not found at known offset";
        std::printf("[T0 pattern_offset_scan] RED early: %s\n", result.detail.c_str());
        return result;
      }
      marker_rel = static_cast<std::size_t>(marker - image.bytes.begin());
    }
  }
  result.pattern_hit = true;
  ++result.steps;

  const auto offset_bytes = w.read_mem(actor, game_pid, game_base + marker_rel + 4,
                                       sizeof(std::uint32_t), true);
  if (offset_bytes.status != ac::Status::Ok || offset_bytes.bytes.size() != sizeof(std::uint32_t)) {
    result.detail = "pattern payload read failed";
    return result;
  }
  std::uint32_t table_rel = 0;
  std::memcpy(&table_rel, offset_bytes.bytes.data(), sizeof(table_rel));
  result.resolved_table_va = static_cast<std::uintptr_t>(game_base + table_rel);

  const auto count_bytes = w.read_mem(actor, game_pid, result.resolved_table_va,
                                      sizeof(std::uint32_t), true);
  if (count_bytes.status != ac::Status::Ok || count_bytes.bytes.size() != sizeof(std::uint32_t)) {
    result.detail = "entity count read failed";
    return result;
  }
  std::uint32_t count = 0;
  std::memcpy(&count, count_bytes.bytes.data(), sizeof(count));
  const auto entities = w.read_mem(actor, game_pid, result.resolved_table_va + 0x10,
                                   static_cast<std::size_t>(count) * 16, true);
  if (entities.status != ac::Status::Ok) {
    result.detail = "entity bulk read failed";
    return result;
  }
  result.entity_count = static_cast<int>(count);
  result.bulk_read_ops = static_cast<int>(w.remote_read_ops - reads_before);
  ++result.steps;

  if (session) {
    session->has_resolve = true;
    session->layout_generation = static_cast<int>(w.lab_pattern_generation);
    session->resolved_table_va = result.resolved_table_va;
  }
  w.schema_cache_active = true;
  w.schema_cache_version = "lab-pattern-cache";
  if (refreshed) {
    ++w.pattern_rescan_count;
    w.schema_remote_update = true;
    ++w.schema_fetch_count;
  }
  result.achieved = result.entity_count > 0;
  result.detail = "pattern_offset_scan: ACPT resolved entity table with bulk reads";
  w.note(result.detail);
  std::printf("[T0 pattern_offset_scan] RED achieved=%d hit=%d entities=%d bulk=%d ops=%llu detail=%s\n",
              result.achieved ? 1 : 0, result.pattern_hit ? 1 : 0, result.entity_count,
              result.bulk_read_ops, static_cast<unsigned long long>(w.remote_read_ops),
              result.detail.c_str());
  return result;
}

}  // namespace

RedResult apply(sim::World& w) {
  return scan(w, nullptr, false);
}

RedResult apply(sim::World& w, ScannerSession& session) { return scan(w, &session, false); }

RedResult refresh(sim::World& w, ScannerSession& session) { return scan(w, &session, true); }

}  // namespace examples::pattern_offset_scan
