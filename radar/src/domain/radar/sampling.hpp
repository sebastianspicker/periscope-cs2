// sampling.hpp — R2: player-sample hygiene policies.
//
// Ported from tiansongyu/cs2_cheat `radar_player_sample.hpp` (MIT license,
// NOTICE recorded). Semantics preserved; naming follows this tree.
//
// These policies decide what a sampled player *means*. They are pure
// functions over sampled values so every branch is table-testable without a
// process, a game, or a clock — see tests/radar_domain_sampling_test.cpp.
//
// The six documented decisions:
//   1. Identity = (player_id, controller, pawn, pawn handle, team). A failed
//      Steam-ID read is *unknown*, not *different player*.
//   2. Position arbitration: SceneNode absolute origin first, m_vOldOrigin
//      fallback on transition frames; zero accepted only as last resort.
//   3. Retention: active = short last-known bridge; dormant = freeze the last
//      client-known state; dead = freeze at the last live position; a
//      same-handle respawn is a new state generation so corpse coordinates
//      cannot leak into the spawn.
//   4. Sanity bounds: finite checks, world-coordinate limit, yaw sanity.
//   5. Reuse gate with sampled-at/now monotonicity.
//   6. Compatibility predicate separating identity conflict from read failure.

#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace radar_domain {

// A legitimate world origin is exactly {0, 0, 0} only when no better
// candidate exists; keep corrupted samples out of every consumer while still
// accepting that case.
inline constexpr float kWorldCoordinateLimit = 10000000.0f;
inline constexpr float kZeroPositionEpsilon = 0.001f;
inline constexpr float kMaximumAbsoluteYawDegrees = 1000000.0f;
inline constexpr std::uint64_t kLastKnownMaximumAgeMs = 150;

struct PositionRead {
  bool succeeded = false;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct PlayerSampleIdentity {
  std::uint64_t player_id = 0;
  std::uintptr_t controller_address = 0;
  std::uintptr_t pawn_address = 0;
  std::uint32_t pawn_handle = 0;
  std::uint8_t team = 0;
  std::uint64_t steam_id = 0;
  bool steam_id_known = false;
};

[[nodiscard]] inline constexpr bool has_same_stable_identity(
    const PlayerSampleIdentity& left,
    const PlayerSampleIdentity& right) noexcept {
  return left.player_id == right.player_id &&
         left.controller_address == right.controller_address &&
         left.pawn_address == right.pawn_address &&
         left.pawn_handle == right.pawn_handle && left.team == right.team;
}

[[nodiscard]] inline constexpr bool has_conflicting_steam_identity(
    const PlayerSampleIdentity& left,
    const PlayerSampleIdentity& right) noexcept {
  return left.steam_id_known && right.steam_id_known &&
         left.steam_id != right.steam_id;
}

// A failed Steam-ID RPM is an unknown observation, not evidence that the
// controller now belongs to a different player. The stable pawn identity
// remains usable until a different known Steam ID is observed.
[[nodiscard]] inline constexpr bool is_compatible_identity_observation(
    const PlayerSampleIdentity& current,
    const PlayerSampleIdentity& cached) noexcept {
  return has_same_stable_identity(current, cached) &&
         !has_conflicting_steam_identity(current, cached);
}

[[nodiscard]] inline bool is_valid_position(float x, float y,
                                            float z) noexcept {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
    return false;
  }
  return std::abs(x) < kWorldCoordinateLimit &&
         std::abs(y) < kWorldCoordinateLimit &&
         std::abs(z) < kWorldCoordinateLimit;
}

[[nodiscard]] inline bool is_usable_yaw(float yaw) noexcept {
  return std::isfinite(yaw) && std::abs(yaw) <= kMaximumAbsoluteYawDegrees;
}

[[nodiscard]] inline bool is_zero_position(float x, float y, float z) noexcept {
  return std::abs(x) <= kZeroPositionEpsilon &&
         std::abs(y) <= kZeroPositionEpsilon &&
         std::abs(z) <= kZeroPositionEpsilon;
}

[[nodiscard]] inline bool is_non_zero_valid_position(float x, float y,
                                                     float z) noexcept {
  return is_valid_position(x, y, z) && !is_zero_position(x, y, z);
}

// SceneNode absolute origin is the rendered entity's authoritative world
// position. m_vOldOrigin is only a fallback for transitions where the
// SceneNode pointer/value cannot be sampled coherently. A zero SceneNode
// commonly appears briefly while an entity is being initialized: prefer a
// valid non-zero old origin in that case, but do not globally reject zero
// because it is a valid world coordinate.
[[nodiscard]] inline std::optional<PositionRead> choose_position(
    const PositionRead& absolute_origin,
    const PositionRead& old_origin) noexcept {
  const bool absolute_valid =
      absolute_origin.succeeded &&
      is_valid_position(absolute_origin.x, absolute_origin.y,
                        absolute_origin.z);
  const bool old_valid = old_origin.succeeded &&
                         is_valid_position(old_origin.x, old_origin.y,
                                           old_origin.z);

  if (absolute_valid && !is_zero_position(absolute_origin.x, absolute_origin.y,
                                          absolute_origin.z)) {
    return absolute_origin;
  }
  if (old_valid && !is_zero_position(old_origin.x, old_origin.y,
                                     old_origin.z)) {
    return old_origin;
  }
  if (absolute_valid) {
    return absolute_origin;
  }
  if (old_valid) {
    return old_origin;
  }
  return std::nullopt;
}

[[nodiscard]] inline constexpr bool can_reuse_last_known(
    const PlayerSampleIdentity& current_identity,
    const PlayerSampleIdentity& cached_identity, std::uint64_t now_ms,
    std::uint64_t sampled_at_ms,
    std::uint64_t maximum_age_ms = kLastKnownMaximumAgeMs) noexcept {
  return is_compatible_identity_observation(current_identity,
                                            cached_identity) &&
         sampled_at_ms <= now_ms && now_ms - sampled_at_ms <= maximum_age_ms;
}

struct PlayerLastKnownState {
  PlayerSampleIdentity identity{};
  bool identity_known = false;
  bool state_known = false;
  bool was_alive = false;
  bool was_dormant = false;
  std::optional<PositionRead> position;
  std::uint64_t position_sampled_at_ms = 0;
  std::optional<float> yaw;
  std::uint64_t yaw_sampled_at_ms = 0;
  std::uint64_t last_observed_at_ms = 0;
};

struct RetainedPlayerSample {
  std::optional<PositionRead> position;
  std::optional<float> yaw;
};

// Active players use a short cache only to bridge a torn RPM sample. Dormant
// players retain the last client-known state, and dead players freeze at the
// last live position. A same-handle respawn is treated as a new state
// generation so corpse coordinates cannot leak into the spawn.
inline RetainedPlayerSample retain_player_sample(
    PlayerLastKnownState& state, const PlayerSampleIdentity& identity,
    std::uint64_t now_ms, bool alive, bool dormant,
    std::optional<PositionRead> current_position,
    std::optional<float> current_yaw) {
  if (current_position &&
      !is_valid_position(current_position->x, current_position->y,
                         current_position->z)) {
    current_position.reset();
  }
  if (current_yaw && !is_usable_yaw(*current_yaw)) {
    current_yaw.reset();
  }

  const bool identity_changed =
      !state.identity_known ||
      !is_compatible_identity_observation(identity, state.identity);
  const bool revived_with_same_identity =
      !identity_changed && state.state_known && !state.was_alive && alive;
  if (identity_changed || revived_with_same_identity) {
    state = PlayerLastKnownState{};
    state.identity = identity;
    state.identity_known = true;
  } else if (identity.steam_id_known) {
    // Bind a previously unknown identity, while retaining a known identity
    // across a single failed/unknown Steam-ID read.
    state.identity.steam_id = identity.steam_id;
    state.identity.steam_id_known = true;
  }

  state.last_observed_at_ms = now_ms;
  RetainedPlayerSample result;
  const bool freeze_last_known = dormant || !alive;
  if (freeze_last_known) {
    // If the first verified sample is already dormant/dead, capture it once.
    // Subsequent corpse movement or dormant memory changes must not move the
    // marker.
    if (!state.position && current_position) {
      state.position = current_position;
      state.position_sampled_at_ms = now_ms;
    }
    if (!state.yaw && current_yaw) {
      state.yaw = current_yaw;
      state.yaw_sampled_at_ms = now_ms;
    }
    result.position = state.position;
    result.yaw = state.yaw;
  } else {
    if (current_position) {
      state.position = current_position;
      state.position_sampled_at_ms = now_ms;
      result.position = current_position;
    } else if (state.position &&
               can_reuse_last_known(identity, state.identity, now_ms,
                                    state.position_sampled_at_ms)) {
      result.position = state.position;
    }

    if (current_yaw) {
      state.yaw = current_yaw;
      state.yaw_sampled_at_ms = now_ms;
      result.yaw = current_yaw;
    } else if (state.yaw &&
               can_reuse_last_known(identity, state.identity, now_ms,
                                    state.yaw_sampled_at_ms)) {
      result.yaw = state.yaw;
    }
  }

  state.state_known = true;
  state.was_alive = alive;
  state.was_dormant = dormant;
  return result;
}

}  // namespace radar_domain
