#pragma once

// Red simulation for pattern_offset_scan. It plants the observable scar: pattern-scan telemetry.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <cstdint>
#include <string>

namespace examples::pattern_offset_scan {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
  bool handle_opened = false;
  bool pattern_hit = false;
  int entity_count = 0;
  int bulk_read_ops = 0;
  bool refreshed = false;
  std::uintptr_t resolved_table_va = 0;
};

struct ScannerSession {
  std::uint32_t actor_pid = 0;
  bool has_resolve = false;
  int layout_generation = 0;
  std::uintptr_t resolved_table_va = 0;
};
RedResult apply(sim::World& w);
RedResult apply(sim::World& w, ScannerSession& session);
RedResult refresh(sim::World& w, ScannerSession& session);
}  // namespace examples::pattern_offset_scan
