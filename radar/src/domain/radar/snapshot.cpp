// snapshot.cpp: R1 protocol v1 serializer. See snapshot.hpp for provenance.
//
// Implementation notes:
//   - Stable field order (compare tiansongyu `web_radar_json.hpp`): the
//     document layout never changes for the same protocol version, so golden
//     tests are possible.
//   - Floats are emitted via std::to_chars (shortest round-trip form); a
//     non-finite float is normalized to 0 rather than emitted as NaN/Infinity,
//     which are not valid JSON.
//   - Player identifiers are emitted as JSON strings.

#include "radar/snapshot.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace radar_domain {
namespace {

std::string float_to_json(float value) {
  if (!std::isfinite(value)) {
    value = 0.0f;
  }
  char buffer[64];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return std::string(buffer, result.ptr);
}

std::string int_to_json(std::int64_t value) {
  char buffer[32];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return std::string(buffer, result.ptr);
}

std::string u64_to_json_string(std::uint64_t value) {
  char buffer[32];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return std::string("\"") + std::string(buffer, result.ptr) + "\"";
}

std::string quoted(const std::string& text) {
  return "\"" + text + "\"";
}

void append_position(std::string& out, const WorldPosition& pos) {
  out += '[';
  out += float_to_json(pos.x);
  out += ',';
  out += float_to_json(pos.y);
  out += ',';
  out += float_to_json(pos.z);
  out += ']';
}

// True when the player passes the team-view policy relative to `local_team`.
bool team_visible(const PlayerState& player, std::uint8_t local_team,
                  TeamViewPolicy policy) {
  switch (policy) {
    case TeamViewPolicy::All:
      return true;
    case TeamViewPolicy::LocalTeamOnly:
      return player.team == local_team;
    case TeamViewPolicy::OpponentsOnly:
      return player.team != local_team;
  }
  return true;
}

void append_player(std::string& out, const PlayerState& player,
                   const SerializationOptions& options) {
  out += "{\"id\":";
  out += u64_to_json_string(player.player_id);
  if (options.include_steam_ids) {
    out += ",\"steam_id\":";
    out += u64_to_json_string(player.steam_id);
  }
  out += ",\"team\":";
  out += int_to_json(player.team);
  out += ",\"name\":";
  out += options.include_player_names ? quoted(json_escape(player.name))
: std::string("\"\"");
  out += ",\"color\":";
  out += int_to_json(player.color);
  out += ",\"alive\":";
  out += player.alive ? "true": "false";
  out += ",\"dormant\":";
  out += player.dormant ? "true": "false";
  out += ",\"pos\":";
  append_position(out, player.pos);
  out += ",\"yaw\":";
  out += float_to_json(player.yaw);
  out += ",\"health\":";
  out += int_to_json(player.health);
  out += ",\"armor\":";
  out += int_to_json(player.armor);
  out += ",\"money\":";
  out += int_to_json(player.money);
  out += ",\"helmet\":";
  out += player.helmet ? "true": "false";
  out += ",\"defuser\":";
  out += player.defuser ? "true": "false";
  out += ",\"bomb\":";
  out += player.has_bomb ? "true": "false";
  out += ",\"weapon\":";
  out += int_to_json(player.weapon_id);
  out += ",\"ammo\":[";
  out += int_to_json(player.ammo_clip);
  out += ',';
  out += int_to_json(player.ammo_reserve);
  out += "]}";
}

const char* entity_kind_name(EntityKind kind) {
  switch (kind) {
    case EntityKind::Weapon: return "weapon";
    case EntityKind::Inferno: return "inferno";
    case EntityKind::Molotov: return "molotov";
    case EntityKind::Smoke: return "smoke";
    case EntityKind::Flashbang: return "flashbang";
    case EntityKind::HeGrenade: return "hegrenade";
    case EntityKind::Decoy: return "decoy";
    case EntityKind::Chicken: return "chicken";
  }
  return "weapon";
}

void append_entity(std::string& out, const EntityEvent& entity) {
  out += "{\"kind\":\"";
  out += entity_kind_name(entity.kind);
  out += "\",\"pos\":";
  append_position(out, entity.pos);
  out += ",\"weapon\":";
  out += int_to_json(entity.weapon_id);
  out += ",\"life_remaining\":";
  out += float_to_json(entity.life_remaining_s);
  out += ",\"hull\":[";
  for (std::size_t i = 0; i < entity.hull.size(); ++i) {
    if (i != 0) {
      out += ',';
    }
    append_position(out, entity.hull[i]);
  }
  out += "]}";
}

void append_bomb(std::string& out, const BombState& bomb) {
  out += "{\"planted\":";
  out += bomb.planted ? "true": "false";
  out += ",\"ticking\":";
  out += bomb.ticking ? "true": "false";
  out += ",\"being_defused\":";
  out += bomb.being_defused ? "true": "false";
  out += ",\"dropped\":";
  out += bomb.dropped ? "true": "false";
  out += ",\"pos\":";
  append_position(out, bomb.pos);
  out += ",\"blow_time\":";
  out += float_to_json(bomb.blow_time);
  out += ",\"defuse_end_time\":";
  out += float_to_json(bomb.defuse_end_time);
  out += ",\"timer_length\":";
  out += float_to_json(bomb.timer_length);
  out += '}';
}

}  // namespace

std::string json_escape(const std::string& text) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(text.size() + 2);
  for (const char raw: text) {
    const unsigned char c = static_cast<unsigned char>(raw);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out += kHex[(c >> 4) & 0xF];
          out += kHex[c & 0xF];
        } else {
          out += raw;
        }
    }
  }
  return out;
}

std::string serialize_snapshot_v1(const GameSnapshot& snapshot,
                                  const SerializationOptions& options) {
  std::string out;
  out.reserve(512 + snapshot.players.size() * 160);
  out += "{\"v\":1,\"seq\":";
  // seq/t_capture are u64 but realistic values fit int64; the cast keeps
  // to_chars on the signed path (a >2^63 sequence would wrap negative -
  // ≈4.9 billion years of 60 Hz frames, an acceptable documented bound).
  out += int_to_json(static_cast<std::int64_t>(snapshot.seq));
  out += ",\"t_capture_ms\":";
  out += int_to_json(static_cast<std::int64_t>(snapshot.t_capture_ms));
  out += ",\"map\":";
  out += quoted(json_escape(snapshot.map));

  out += ",\"local\":{\"id\":";
  out += u64_to_json_string(snapshot.local.player_id);
  if (options.include_steam_ids) {
    out += ",\"steam_id\":";
    out += u64_to_json_string(snapshot.local.steam_id);
  }
  out += ",\"team\":";
  out += int_to_json(snapshot.local.team);
  out += ",\"name\":";
  out += options.include_player_names ? quoted(json_escape(snapshot.local.name))
: std::string("\"\"");
  out += ",\"alive\":";
  out += snapshot.local.alive ? "true": "false";
  out += ",\"pos\":";
  append_position(out, snapshot.local.pos);
  out += ",\"yaw\":";
  out += float_to_json(snapshot.local.yaw);
  out += ",\"health\":";
  out += int_to_json(snapshot.local.health);
  out += ",\"armor\":";
  out += int_to_json(snapshot.local.armor);
  out += ",\"money\":";
  out += int_to_json(snapshot.local.money);
  out += ",\"defuser\":";
  out += snapshot.local.has_defuser ? "true": "false";
  out += ",\"bomb\":";
  out += snapshot.local.has_bomb ? "true": "false";
  out += ",\"weapon\":";
  out += int_to_json(snapshot.local.weapon_id);
  out += ",\"ammo\":[";
  out += int_to_json(snapshot.local.ammo_clip);
  out += ',';
  out += int_to_json(snapshot.local.ammo_reserve);
  out += "]}";

  out += ",\"players\":[";
  bool first_player = true;
  for (const PlayerState& player : snapshot.players) {
    if (!team_visible(player, snapshot.local.team, options.team_view)) {
      continue;
    }
    if (!first_player) {
      out += ',';
    }
    first_player = false;
    append_player(out, player, options);
  }
  out += ']';

  out += ",\"entities\":[";
  for (std::size_t i = 0; i < snapshot.entities.size(); ++i) {
    if (i != 0) {
      out += ',';
    }
    append_entity(out, snapshot.entities[i]);
  }
  out += "],\"bomb\":";
  append_bomb(out, snapshot.bomb);
  out += '}';
  return out;
}

}  // namespace radar_domain
