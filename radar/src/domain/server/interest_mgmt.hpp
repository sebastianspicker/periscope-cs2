#pragma once

// Server interest management / fog-of-war for the educational lab.
// Structural counter: only replicate entities the observer should know about.
// Simulated educational policy — not a production game server.

#include "ac/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace server {

// Observer pose used for radius-based interest culling.
struct Observer {
  ac::Vec3 origin{};
  float view_yaw_deg = 0;
};

// One world entity candidate for the replication filter.
struct WorldEntity {
  std::uint32_t id = 0;
  ac::Vec3 origin{};
  std::uint8_t team = 0;
  bool alive = false;
};

// Multi-reason bookkeeping for interest cull (teammates kept + enemies culled).
struct InterestFilterStats {
  int truth_alive = 0;
  int replicated = 0;
  int enemies_culled = 0;
  int teammates = 0;
  bool multi_reason = false;  // both keep and cull paths fired
  std::string detail;
};

/// Structural counter: only replicate entities the observer should know about.
///
/// Lab policy (deterministic, pure — no I/O or World mutation):
/// - Dead entities are never replicated.
/// - Same-team alive entities are always replicated (including self).
/// - Enemy alive entities are replicated only when within `enemy_radius`
///   of the observer origin (inclusive boundary).
/// - `InterestFilterStats::multi_reason` is true when at least one keep path
///   (teammate and/or near enemy) and the cull path both fire, with non-empty
///   `detail` describing the bookkeeping.
class InterestManager {
 public:
  /// Lab heuristic: same-team always; enemies only within radius.
  std::vector<WorldEntity> filter_for_client(const Observer& obs,
                                             std::uint8_t observer_team,
                                             const std::vector<WorldEntity>& all,
                                             float enemy_radius) const;

  /// Same filter with multi-reason stats out-parameter for demos/tests.
  std::vector<WorldEntity> filter_for_client_stats(
      const Observer& obs, std::uint8_t observer_team,
      const std::vector<WorldEntity>& all, float enemy_radius,
      InterestFilterStats& stats) const;
};

}  // namespace server
