// radar_sampling_test.cpp: R2: player-sample hygiene policies.
//
// Table-driven tests per policy branch: torn read bridge, dormant freeze,
// dead freeze, same-handle respawn reset, zero-origin scene init, Steam-ID
// conflict handling.

#include "radar/sampling.hpp"
#include "radar/snapshot.hpp"

#include <cstdio>

namespace {

using radar_domain::PlayerLastKnownState;
using radar_domain::PlayerSampleIdentity;
using radar_domain::PositionRead;
using radar_domain::RetainedPlayerSample;
using radar_domain::retain_player_sample;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

bool same_position(const PositionRead& a, const PositionRead& b) {
  return a.succeeded == b.succeeded && a.x == b.x && a.y == b.y && a.z == b.z;
}

PlayerSampleIdentity identity(std::uint64_t player_id,
                              std::uintptr_t pawn = 0x1000) {
  PlayerSampleIdentity id;
  id.player_id = player_id;
  id.controller_address = 0x2000;
  id.pawn_address = pawn;
  id.pawn_handle = 42;
  id.team = radar_domain::kTeamT;
  return id;
}

PositionRead read(float x, float y, float z) {
  return PositionRead{true, x, y, z};
}

void test_validity_bounds() {
  check(radar_domain::is_valid_position(1.0f, 2.0f, 3.0f), "finite inside");
  check(!radar_domain::is_valid_position(1e8f, 0, 0), "beyond limit x");
  check(!radar_domain::is_valid_position(0, 0, 1e8f), "beyond limit z");
  const float quiet_nan = 0.0f / 0.0f;
  check(!radar_domain::is_valid_position(quiet_nan, 0, 0), "nan rejected");
  check(radar_domain::is_valid_position(0, 0, 0), "zero is a valid position");
  check(radar_domain::is_usable_yaw(180.0f), "yaw ok");
  check(!radar_domain::is_usable_yaw(2e6f), "insane yaw rejected");
  check(radar_domain::is_non_zero_valid_position(1, 2, 3),
        "non-zero valid");
  check(!radar_domain::is_non_zero_valid_position(0, 0, 0),
        "zero not non-zero");
}

void test_choose_position_priority() {
  // Valid absolute origin wins over old origin.
  const auto a =
      radar_domain::choose_position(read(10, 20, 30), read(1, 2, 3));
  check(a && same_position(*a, read(10, 20, 30)), "absolute preferred");

  // Zero absolute + valid old origin: old origin wins (scene init).
  const auto b = radar_domain::choose_position(read(0, 0, 0), read(1, 2, 3));
  check(b && same_position(*b, read(1, 2, 3)), "old origin on zero scene");

  // Zero absolute + zero old origin: zero absolute accepted as last resort.
  const auto c = radar_domain::choose_position(read(0, 0, 0), read(0, 0, 0));
  check(c && same_position(*c, read(0, 0, 0)), "zero last resort");

  // Everything invalid: no position.
  const PositionRead failed{};
  const auto d = radar_domain::choose_position(failed, failed);
  check(!d, "all invalid -> none");

  // Invalid absolute + valid old origin.
  const float quiet_nan = 0.0f / 0.0f;
  const auto e = radar_domain::choose_position(PositionRead{true, quiet_nan, 0, 0},
                                               read(4, 5, 6));
  check(e && same_position(*e, read(4, 5, 6)), "invalid absolute falls back");
}

void test_torn_read_bridge() {
  PlayerLastKnownState state;
  const PlayerSampleIdentity id = identity(1);

  const RetainedPlayerSample first =
      retain_player_sample(state, id, 1000, true, false, read(10, 20, 30),
                           45.0f);
  check(first.position && same_position(*first.position, read(10, 20, 30)),
        "first sample passes through");
  check(first.yaw && *first.yaw == 45.0f, "first yaw passes through");

  // Torn read within the bridge window reuses the last known value.
  const RetainedPlayerSample bridged =
      retain_player_sample(state, id, 1100, true, false, std::nullopt,
                           std::nullopt);
  check(bridged.position &&
            same_position(*bridged.position, read(10, 20, 30)),
        "torn read bridged");

  // Beyond the bridge window the stale value is not reused.
  const RetainedPlayerSample stale =
      retain_player_sample(state, id, 5000, true, false, std::nullopt,
                           std::nullopt);
  check(!stale.position, "stale bridge expires");
}

void test_dormant_freeze() {
  PlayerLastKnownState state;
  const PlayerSampleIdentity id = identity(2);

  retain_player_sample(state, id, 1000, true, false, read(10, 20, 30), 0.0f);
  // Dormant: position freezes at the last client-known state.
  const RetainedPlayerSample dormant =
      retain_player_sample(state, id, 2000, true, true, read(99, 99, 99),
                           123.0f);
  check(dormant.position && same_position(*dormant.position, read(10, 20, 30)),
        "dormant keeps last known, ignores dormant memory churn");
  check(dormant.yaw && *dormant.yaw == 0.0f, "dormant yaw frozen");
}

void test_dead_freeze_and_respawn_reset() {
  PlayerLastKnownState state;
  const PlayerSampleIdentity id = identity(3);

  retain_player_sample(state, id, 1000, true, false, read(10, 20, 30), 0.0f);
  // Death freezes at the last live position.
  const RetainedPlayerSample dead =
      retain_player_sample(state, id, 1100, false, false, std::nullopt,
                           std::nullopt);
  check(dead.position && same_position(*dead.position, read(10, 20, 30)),
        "dead freezes at last live position");

  // Corpse movement while dead must not move the marker.
  const RetainedPlayerSample corpse =
      retain_player_sample(state, id, 1200, false, false, read(500, 500, 500),
                           90.0f);
  check(corpse.position && same_position(*corpse.position, read(10, 20, 30)),
        "corpse movement ignored");

  // Same-handle respawn: alive again -> new state generation.
  const RetainedPlayerSample respawn =
      retain_player_sample(state, id, 2000, true, false, read(70, 80, 90),
                           15.0f);
  check(respawn.position && same_position(*respawn.position, read(70, 80, 90)),
        "respawn resets generation");
  check(state.position_sampled_at_ms == 2000, "respawn timestamp updated");
}

void test_steam_identity_conflict() {
  PlayerLastKnownState state;
  PlayerSampleIdentity id = identity(4);
  id.steam_id = 1000;
  id.steam_id_known = true;
  retain_player_sample(state, id, 1000, true, false, read(1, 2, 3), 0.0f);

  // A failed Steam-ID read is unknown, not a different player.
  PlayerSampleIdentity unknown_read = identity(4);
  unknown_read.steam_id_known = false;
  const RetainedPlayerSample tolerated =
      retain_player_sample(state, unknown_read, 1100, true, false,
                           std::nullopt, std::nullopt);
  check(tolerated.position && state.identity.steam_id == 1000,
        "failed steam read keeps identity");

  // A conflicting known Steam ID resets the state.
  PlayerSampleIdentity conflicting = identity(4);
  conflicting.steam_id = 9999;
  conflicting.steam_id_known = true;
  const RetainedPlayerSample reset =
      retain_player_sample(state, conflicting, 1200, true, false,
                           read(9, 9, 9), 0.0f);
  check(reset.position && state.identity.steam_id == 9999,
        "conflicting steam id resets identity");
}

void test_reuse_gate_monotonicity() {
  const PlayerSampleIdentity id = identity(5);
  // A sampled-at stamp from the future is never reusable.
  check(!radar_domain::can_reuse_last_known(id, id, 1000, 1500),
        "future sample rejected");
  // Exactly at the boundary is still inside the window.
  check(radar_domain::can_reuse_last_known(id, id, 1150, 1000),
        "boundary age accepted");
  check(!radar_domain::can_reuse_last_known(id, id, 1151, 1000),
        "beyond boundary rejected");
}

}  // namespace

int main() {
  test_validity_bounds();
  test_choose_position_priority();
  test_torn_read_bridge();
  test_dormant_freeze();
  test_dead_freeze_and_respawn_reset();
  test_steam_identity_conflict();
  test_reuse_gate_monotonicity();

  if (failures != 0) {
    std::printf("%d sampling check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_sampling_test: all checks passed\n");
  return 0;
}
