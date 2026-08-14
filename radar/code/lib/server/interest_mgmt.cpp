// interest_mgmt.cpp — server interest management / fog-of-war filter for entity stream.
// Structural kill: do not full-replicate unobservable enemies to client.

#include "server/interest_mgmt.hpp"

#include <sstream>

namespace server {
namespace {

// Squared Euclidean distance between two lab positions (avoids sqrt).
float dist2(const ac::Vec3& a, const ac::Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

// Clamp radius to a non-negative finite value for stable comparisons.
float sanitize_radius(float enemy_radius) {
  if (!(enemy_radius > 0.f) || enemy_radius != enemy_radius) {
    return 0.f;
  }
  return enemy_radius;
}

}  // namespace

// InterestManager::filter_for_client: Filter entity stream to what client should observe.
std::vector<WorldEntity> InterestManager::filter_for_client(
    const Observer& obs, std::uint8_t observer_team,
    const std::vector<WorldEntity>& all, float enemy_radius) const {
  InterestFilterStats unused{};
  return filter_for_client_stats(obs, observer_team, all, enemy_radius, unused);
}

// InterestManager::filter_for_client_stats: Filter stream and return InterestFilterStats.
std::vector<WorldEntity> InterestManager::filter_for_client_stats(
    const Observer& obs, std::uint8_t observer_team,
    const std::vector<WorldEntity>& all, float enemy_radius,
    InterestFilterStats& stats) const {
  stats = {};
  std::vector<WorldEntity> out;
  out.reserve(all.size());

  const float radius = sanitize_radius(enemy_radius);
  const float r2 = radius * radius;

  bool kept_team = false;
  bool kept_enemy = false;
  bool culled_enemy = false;
  int near_enemies = 0;
  int dead_skipped = 0;

  for (const auto& e : all) {
    // Dead entities are never part of the replication set.
    if (!e.alive) {
      ++dead_skipped;
      continue;
    }
    ++stats.truth_alive;

    // Same-team alive entities are always replicated (structural team share).
    if (e.team == observer_team) {
      out.push_back(e);
      ++stats.teammates;
      kept_team = true;
      continue;
    }

    // Enemies: keep only when inside inclusive radius of observer origin.
    if (dist2(obs.origin, e.origin) <= r2) {
      out.push_back(e);
      kept_enemy = true;
      ++near_enemies;
    } else {
      ++stats.enemies_culled;
      culled_enemy = true;
    }
  }

  stats.replicated = static_cast<int>(out.size());
  // Multi-reason: both a keep path (team or near enemy) and a cull path fired.
  stats.multi_reason = (kept_team || kept_enemy) && culled_enemy;

  std::ostringstream oss;
  oss << "alive=" << stats.truth_alive << " rep=" << stats.replicated
      << " culled=" << stats.enemies_culled << " team=" << stats.teammates
      << " near_enemy=" << near_enemies << " dead_skip=" << dead_skipped
      << " keep_team=" << (kept_team ? 1 : 0)
      << " keep_enemy=" << (kept_enemy ? 1 : 0)
      << " cull=" << (culled_enemy ? 1 : 0)
      << " multi=" << (stats.multi_reason ? 1 : 0)
      << " radius=" << radius;
  if (stats.multi_reason) {
    oss << " reasons=keep+cull";
  }
  stats.detail = oss.str();
  return out;
}

}  // namespace server
