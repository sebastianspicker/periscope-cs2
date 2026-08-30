#pragma once

// Structural kill depth: partial fog, delayed origin, stream crypto + key exfil.
// Blue mitigation is multi-step scoring — not only toggling full-replication.

#include "server/interest_mgmt.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace depth {

enum class FogPolicy : std::uint8_t {
  FullReplication = 0,  // red-friendly: all enemy origins
  StrictRadius = 1,     // classic interest management
  PartialLeak = 2,      // buggy fog: fraction of far entities still sent
  DelayedOrigin = 3,    // far entities sent with stale/delayed positions
};

// Lab type `EntityTruth` used by this educational unit.
struct EntityTruth {
  std::uint32_t id = 0;
  ac::Vec3 origin{};
  std::uint8_t team = 0;
  bool alive = true;
  bool currently_observable = false;  // vision/sound right now
};

// Lab type `ReplicatedEntity` used by this educational unit.
struct ReplicatedEntity {
  std::uint32_t id = 0;
  ac::Vec3 origin{};
  std::uint8_t team = 0;
  bool alive = true;
  bool delayed = false;
  bool leaked_beyond_fog = false;
};

// Lab type `LeakageScore` used by this educational unit.
struct LeakageScore {
  int truth_enemies = 0;
  int replicated_enemies = 0;
  int leaked_beyond_fog = 0;
  int delayed_origins = 0;
  int key_exfil_events = 0;
  double leakage_ratio = 0;   // leaked / truth far enemies
  double risk = 0;            // higher = more radar value remains
  bool structural_kill = false;  // leakage low enough + no key
  std::string detail;
};

// Lab type `StreamCryptoState` used by this educational unit.
struct StreamCryptoState {
  bool encrypted = false;
  bool client_has_key = false;
  bool key_exfiltrated = false;  // red pulled key from client mem
  bool server_rotated_key = false;
};

/// Multi-step interest + entity-stream leakage scorer (shipped server depth API).
class LeakageScorer {
 public:
  void set_fog(FogPolicy p) { fog_ = p; }
  void set_radius(float r) { radius_ = r; }
  void set_partial_leak_fraction(float f) { partial_leak_ = f; }
  void set_delay_ticks(int t) { delay_ticks_ = t; }

  /// Replicate world entities under current fog policy for one observer.
  std::vector<ReplicatedEntity> replicate(const server::Observer& obs,
                                          std::uint8_t team,
                                          const std::vector<EntityTruth>& all) const;

  /// Score how much unfair radar value remains after policy + stream state.
  LeakageScore score(const server::Observer& obs, std::uint8_t team,
                     const std::vector<EntityTruth>& all,
                     const StreamCryptoState& stream) const;

  /// Apply multi-step blue mitigation on World (fog + crypto + key rotation).
  LeakageScore mitigate_world(sim::World& w, const server::Observer& obs,
                              std::uint8_t team,
                              const std::vector<EntityTruth>& all) const;

 private:
  FogPolicy fog_ = FogPolicy::StrictRadius;
  float radius_ = 50.f;
  float partial_leak_ = 0.25f;
  int delay_ticks_ = 3;
};

/// Red multi-step: want full data + exfil stream key if present.
struct StreamExfilResult {
  bool wanted_full_replication = false;
  bool key_present = false;
  bool key_exfiltrated = false;
  bool useful_radar = false;
  std::string detail;
};

StreamExfilResult run_stream_exfil_red(sim::World& w);

}  // namespace depth
