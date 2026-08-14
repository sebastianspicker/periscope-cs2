// strategy_support.cpp — non-inline surface for the strategy support library.
// Most helpers are header-inline for pair convenience; this TU anchors the
// library and re-exports risk/signal helpers that tests can link against.

#include "strategies/strategy_support.hpp"
#include "strategies/multi_reason.hpp"
#include "strategies/scar_sensors.hpp"

namespace strategies::support_detail {

// Anchor symbols so the static library is never empty-of-symbols under LTO.
int library_abi_version() {
  // Bump when ScarInventory / MultiReasonDetector public fields change.
  return 2;
}

double score_world_risk(const sim::World& w) {
  const auto inv = sensors::inventory(w);
  const auto blue = sensors::inventory_to_blue(inv);
  return blue.risk;
}

int count_world_scars(const sim::World& w) {
  return sensors::inventory(w).total_scars;
}

}  // namespace strategies::support_detail
