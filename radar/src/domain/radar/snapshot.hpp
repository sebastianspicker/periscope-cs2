// snapshot.hpp: R1: immutable absolute-state GameSnapshot + protocol v1.
//
// Adapted from the 2026-11-05 research notes:
//   - tiansongyu/cs2_cheat (MIT): `GameSnapshot` / `web_radar_json.hpp` -
//     every frame is complete absolute state, no deltas; u64 identifiers are
//     serialized as JSON strings so JavaScript cannot lose precision.
//   - avitran0/deadlocked `shared/src/entity.rs`: typed entity variant
//     (Weapon | Inferno | Molotov | Smoke | Flash | HE | Decoy | Chicken).
//
// The snapshot is the single data contract between the sampler, the local
// radar renderer and the web-radar/relay network layers (R5/R6). It must stay
// free of any process/hardware access: domain layer, pure std.
//
// Serialization hygiene (ledger R1): reject or normalize non-finite floats,
// escape strings, omit process addresses, and send Steam IDs only behind an
// explicit opt-in flag (default off).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace radar_domain {

// World position in game units. Plain struct so the ring (R3) can memcpy it.
struct WorldPosition {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

// Team numbering follows the Source convention used by the upstream models:
// 2 = T, 3 = CT; anything else is spectator/unknown.
constexpr std::uint8_t kTeamT = 2;
constexpr std::uint8_t kTeamCT = 3;

// One player row as seen by the radar. `player_id` is the stable in-game id;
// `steam_id` is identity metadata and is only serialized when the sharing
// options opt in.
struct PlayerState {
  std::uint64_t player_id = 0;
  std::uint64_t steam_id = 0;
  std::uint8_t team = 0;
  std::string name;
  std::uint8_t color = 0;
  bool alive = false;
  bool dormant = false;
  WorldPosition pos{};
  float yaw = 0.0f;
  std::int32_t health = 0;
  std::int32_t armor = 0;
  std::int32_t money = 0;
  bool helmet = false;
  bool defuser = false;
  bool has_bomb = false;
  std::int32_t weapon_id = 0;
  std::int32_t ammo_clip = 0;
  std::int32_t ammo_reserve = 0;
};

// Local player block: the observing user, kept separate so team-view policies
// can filter opponents without special-casing index 0.
struct LocalPlayerState {
  std::uint64_t player_id = 0;
  std::uint64_t steam_id = 0;
  std::uint8_t team = 0;
  std::string name;
  bool alive = false;
  WorldPosition pos{};
  float yaw = 0.0f;
  std::int32_t health = 0;
  std::int32_t armor = 0;
  std::int32_t money = 0;
  bool has_defuser = false;
  bool has_bomb = false;
  std::int32_t weapon_id = 0;
  std::int32_t ammo_clip = 0;
  std::int32_t ammo_reserve = 0;
};

// C4 lifecycle, mirroring the union of fields both upstream protocols carry.
struct BombState {
  bool planted = false;
  bool ticking = false;
  bool being_defused = false;
  bool dropped = false;
  WorldPosition pos{};
  float blow_time = 0.0f;
  float defuse_end_time = 0.0f;
  float timer_length = 0.0f;
};

// Typed world entities (deadlocked's enum shape). `hull` carries the inferno
// flame-hull polygon; `life_remaining_s` counts down for timed projectiles.
enum class EntityKind: std::uint8_t {
  Weapon = 0,
  Inferno = 1,
  Molotov = 2,
  Smoke = 3,
  Flashbang = 4,
  HeGrenade = 5,
  Decoy = 6,
  Chicken = 7,
};

struct EntityEvent {
  EntityKind kind = EntityKind::Weapon;
  WorldPosition pos{};
  std::int32_t weapon_id = 0;
  float life_remaining_s = 0.0f;
  std::vector<WorldPosition> hull;
};

// One complete, absolute frame. Nothing in this struct is a delta or a
// process address; a consumer that sees one frame needs no history.
struct GameSnapshot {
  std::uint64_t seq = 0;             // monotonically increasing frame number
  std::uint64_t t_capture_ms = 0;    // sampler capture time, unix ms
  std::string map;                   // e.g. "de_mirage"; empty when unknown
  LocalPlayerState local{};
  std::vector<PlayerState> players;
  std::vector<EntityEvent> entities;
  BombState bomb{};
};

// Sharing policy applied at serialization time (ledger R1: sanitize at the
// single choke point, before anything leaves the process).
enum class TeamViewPolicy: std::uint8_t {
  All = 0,
  LocalTeamOnly = 1,
  OpponentsOnly = 2,
};

struct SerializationOptions {
  bool include_player_names = true;
  bool include_steam_ids = false;            // opt-in, default off
  TeamViewPolicy team_view = TeamViewPolicy::All;
  bool operator==(const SerializationOptions&) const = default;
};

// Serializes an absolute state frame as protocol v1 with a stable field
// order. Non-finite floats are normalized to 0; strings are escaped; no
// process addresses are ever emitted. Returns the JSON document as a string.
std::string serialize_snapshot_v1(const GameSnapshot& snapshot,
                                  const SerializationOptions& options = {});

// JSON-escapes a string (quotes, backslash, control characters as \uXXXX).
std::string json_escape(const std::string& text);

}  // namespace radar_domain
