#include "cs2/simulator.hpp"
#include "cs2/schema.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <string>
#include <type_traits>
#include <vector>

namespace cs2 {

namespace {
constexpr std::size_t kMaxPlayerName = 128;

template <typename T>
bool decode_scalar(const std::vector<std::uint8_t>& buffer, T* out) {
  static_assert(std::is_trivially_copyable_v<T>);
  if (out == nullptr || buffer.size() != sizeof(T)) return false;
  std::array<std::uint8_t, sizeof(T)> bytes{};
  std::copy_n(buffer.begin(), bytes.size(), bytes.begin());
  *out = std::bit_cast<T>(bytes);
  return true;
}
}  // namespace

bool Cs2GameReader::read_raw(std::uint32_t reader_pid, std::uint64_t address,
                             std::vector<std::uint8_t>& out, std::size_t size,
                             bool require_handle) {
  if (world == nullptr) return false;
  if (const auto* game = world->proc(game_pid); game && address < game->base) {
    address += game->base;
  }
  auto result =
      world->read_mem(reader_pid, game_pid, address, size, require_handle);
  // A successful transport result is not sufficient for a typed caller: every
  // subsequent decode assumes the requested extent is present.
  if (result.status != ac::Status::Ok || result.bytes.size() != size) {
    return false;
  }
  out = std::move(result.bytes);
  return true;
}

int Cs2GameReader::read_int(std::uint32_t reader_pid, std::uint64_t address,
                            bool require_handle) {
  std::vector<std::uint8_t> buffer;
  int value = 0;
  (void)(read_raw(reader_pid, address, buffer, sizeof(value), require_handle) &&
         decode_scalar(buffer, &value));
  return value;
}

float Cs2GameReader::read_float(std::uint32_t reader_pid, std::uint64_t address,
                                bool require_handle) {
  std::vector<std::uint8_t> buffer;
  float value = 0;
  (void)(read_raw(reader_pid, address, buffer, sizeof(value), require_handle) &&
         decode_scalar(buffer, &value));
  return value;
}

uint64_t Cs2GameReader::read_ptr(std::uint32_t reader_pid, std::uint64_t address,
                                 bool require_handle) {
  std::vector<std::uint8_t> buffer;
  uint64_t value = 0;
  (void)(read_raw(reader_pid, address, buffer, sizeof(value), require_handle) &&
         decode_scalar(buffer, &value));
  return value;
}

void Cs2GameReader::read_buf(std::uint32_t reader_pid, std::uint64_t address,
                             void* out, std::size_t size, bool require_handle) {
  if (out == nullptr && size != 0) return;
  std::vector<std::uint8_t> buffer;
  if (read_raw(reader_pid, address, buffer, size, require_handle) &&
      buffer.size() >= size) {
    std::copy_n(buffer.begin(), size, static_cast<std::uint8_t*>(out));
  }
}

std::string Cs2GameReader::read_string(std::uint32_t reader_pid,
                                       std::uint64_t address,
                                       std::size_t max_len,
                                       bool require_handle) {
  std::vector<std::uint8_t> buffer;
  if (!read_raw(reader_pid, address, buffer, max_len, require_handle) ||
      buffer.empty()) {
    return {};
  }
  buffer.back() = 0;
  return std::string(reinterpret_cast<const char*>(buffer.data()));
}

std::vector<PlayerData> Cs2GameReader::resolve_players(
    std::uint32_t reader_pid, bool require_handle) {
  std::vector<PlayerData> players;
  const auto& fields = FieldOffsets::get();

  for (uint32_t controller_index = 1; controller_index <= MAX_CONTROLLERS;
       ++controller_index) {
    const uintptr_t controller_chunk = read_ptr(
        reader_pid, list_entry_addr(SimLayout::ENTITY_LIST, controller_index),
        require_handle);
    if (!controller_chunk) continue;

    const uintptr_t controller = read_ptr(
        reader_pid,
        identity_slot_addr(controller_chunk, controller_index) + 0x08,
        require_handle);
    if (!controller) continue;

    const uint32_t pawn_index =
        static_cast<uint32_t>(read_int(
            reader_pid, controller + fields.controller_pawn_handle,
            require_handle)) &
        ENTITY_HANDLE_INDEX_MASK;
    if (!pawn_index) continue;

    const uintptr_t pawn_chunk = read_ptr(
        reader_pid, list_entry_addr(SimLayout::ENTITY_LIST, pawn_index),
        require_handle);
    if (!pawn_chunk) continue;
    const uintptr_t pawn = read_ptr(
        reader_pid, identity_slot_addr(pawn_chunk, pawn_index) + 0x08,
        require_handle);
    if (!pawn) continue;

    const int health =
        read_int(reader_pid, pawn + fields.health, require_handle);
    const int team = read_int(reader_pid, pawn + fields.team, require_handle);
    if (!is_player_valid(health, team) &&
        read_int(reader_pid, pawn + fields.life_state, require_handle) ==
            LifeState_Alive) {
      // Keep dead players with valid team for radar corpse dots when team is
      // T/CT even if health is zero.
      if (team != Team_T && team != Team_CT) continue;
    } else if (!is_player_valid(health, team) && health <= 0 &&
               (team != Team_T && team != Team_CT)) {
      continue;
    }
    if (team != Team_T && team != Team_CT) continue;

    PlayerData player{};
    player.controller_index = controller_index;
    player.pawn_index = pawn_index;
    player.controller_handle = controller_index;
    player.pawn_handle = pawn_index;
    player.health = health;
    player.team = team;
    player.life_state =
        read_int(reader_pid, pawn + fields.life_state, require_handle);
    player.armor = read_int(reader_pid, pawn + fields.armor, require_handle);
    player.spotted_mask =
        read_ptr(reader_pid, pawn + fields.spotted_mask, require_handle);
    player.eye_angles.pitch =
        read_float(reader_pid, pawn + fields.eye_angles, require_handle);
    player.eye_angles.yaw =
        read_float(reader_pid, pawn + fields.eye_angles + 4, require_handle);
    player.eye_angles.roll =
        read_float(reader_pid, pawn + fields.eye_angles + 8, require_handle);
    player.is_scoped =
        read_int(reader_pid, pawn + fields.is_scoped, require_handle) != 0;
    player.is_defusing =
        read_int(reader_pid, pawn + fields.is_defusing, require_handle) != 0;
    player.flash_duration =
        read_float(reader_pid, pawn + fields.flash_duration, require_handle);
    player.desired_fov =
        read_int(reader_pid, controller + fields.desired_fov, require_handle);
    player.is_local =
        read_int(reader_pid, controller + fields.controller_is_local,
                 require_handle) != 0;
    player.name =
        read_string(reader_pid, controller + fields.controller_name,
                    kMaxPlayerName, require_handle);
    if (player.name.empty()) {
      player.name = "player" + std::to_string(controller_index);
    }

    player.old_origin.x =
        read_float(reader_pid, pawn + fields.old_origin, require_handle);
    player.old_origin.y =
        read_float(reader_pid, pawn + fields.old_origin + 4, require_handle);
    player.old_origin.z =
        read_float(reader_pid, pawn + fields.old_origin + 8, require_handle);

    const uintptr_t scene_node =
        read_ptr(reader_pid, pawn + fields.scene_node, require_handle);
    if (scene_node) {
      player.origin.x =
          read_float(reader_pid, scene_node + fields.node_origin, require_handle);
      player.origin.y = read_float(
          reader_pid, scene_node + fields.node_origin + 4, require_handle);
      player.origin.z = read_float(
          reader_pid, scene_node + fields.node_origin + 8, require_handle);
    } else {
      player.origin = player.old_origin;
    }

    const uintptr_t weapon_services =
        read_ptr(reader_pid, pawn + fields.weapon_services, require_handle);
    if (weapon_services) {
      player.active_weapon_handle = static_cast<uint32_t>(read_int(
          reader_pid, weapon_services + fields.active_weapon, require_handle));
    }

    player.is_alive = is_player_alive(player);
    players.push_back(std::move(player));
  }
  return players;
}

PlayerData Cs2GameReader::resolve_local_player(std::uint32_t reader_pid,
                                               bool require_handle) {
  auto players = resolve_players(reader_pid, require_handle);
  for (auto& player : players) {
    if (player.is_local) return player;
  }
  if (!players.empty()) {
    players.front().is_local = true;
    return players.front();
  }
  return {};
}

HudRadarState Cs2GameReader::resolve_hud_radar(std::uint32_t reader_pid,
                                               bool require_handle) {
  const auto& offsets = HudRadarOffsets::get();
  const uintptr_t hud = SimLayout::HUD_RADAR;
  HudRadarState state;
  state.is_round_active =
      read_int(reader_pid, hud + offsets.is_round, require_handle) != 0;
  state.map_texture_position = {
      read_float(reader_pid, hud + offsets.map_texture_position, require_handle),
      read_float(reader_pid, hud + offsets.map_texture_position + 4,
                 require_handle),
      read_float(reader_pid, hud + offsets.map_texture_position + 8,
                 require_handle),
  };
  state.visibility_size_max =
      read_float(reader_pid, hud + offsets.visibility_size_max, require_handle);
  state.visibility_size =
      read_float(reader_pid, hud + offsets.visibility_size, require_handle);
  state.map_texture_scale_stored =
      read_float(reader_pid, hud + offsets.map_texture_scale, require_handle);
  state.max_visibility_sq =
      read_float(reader_pid, hud + offsets.max_visibility_sq, require_handle);
  state.origin_tex_diff = {
      read_float(reader_pid, hud + offsets.origin_tex_diff, require_handle),
      read_float(reader_pid, hud + offsets.origin_tex_diff + 4, require_handle),
      read_float(reader_pid, hud + offsets.origin_tex_diff + 8, require_handle),
  };
  state.radar_scale =
      read_float(reader_pid, hud + offsets.radar_scale, require_handle);
  state.is_square =
      read_int(reader_pid, hud + offsets.is_square, require_handle) != 0;
  state.valid = state.map_texture_scale_stored > 0.f || state.radar_scale > 0.f;
  return state;
}

BombData Cs2GameReader::resolve_bomb(std::uint32_t reader_pid,
                                     bool require_handle) {
  const auto& fields = FieldOffsets::get();
  BombData bomb;
  const uintptr_t bomb_ptr =
      read_ptr(reader_pid, SimLayout::GLOBALS_AREA + 0x18, require_handle);
  if (!bomb_ptr) return bomb;

  bomb.entity_index = 128;
  bomb.planted = true;
  bomb.ticking =
      read_int(reader_pid, bomb_ptr + fields.bomb_ticking, require_handle) != 0;
  bomb.defused =
      read_int(reader_pid, bomb_ptr + fields.bomb_defused, require_handle) != 0;
  bomb.site = read_int(reader_pid, bomb_ptr + fields.bomb_site, require_handle);
  bomb.timer =
      read_float(reader_pid, bomb_ptr + fields.bomb_blow, require_handle);
  bomb.defuse_countdown = read_float(
      reader_pid, bomb_ptr + fields.bomb_defuse_count, require_handle);
  bomb.origin.x =
      read_float(reader_pid, bomb_ptr + fields.old_origin, require_handle);
  bomb.origin.y =
      read_float(reader_pid, bomb_ptr + fields.old_origin + 4, require_handle);
  bomb.origin.z =
      read_float(reader_pid, bomb_ptr + fields.old_origin + 8, require_handle);
  bomb.site_name = bomb_site_name(bomb.site);
  return bomb;
}

ViewMatrix Cs2GameReader::resolve_view_matrix(std::uint32_t reader_pid,
                                              bool require_handle) {
  ViewMatrix matrix;
  read_buf(reader_pid, SimLayout::VIEW_MATRIX_AREA, matrix.m, sizeof(matrix.m),
           require_handle);
  matrix.valid =
      read_int(reader_pid, SimLayout::VIEW_MATRIX_AREA + sizeof(matrix.m),
               require_handle) != 0;
  return matrix;
}

GameState Cs2GameReader::resolve_game_state(std::uint32_t reader_pid,
                                            bool require_handle,
                                            const char* map_name) {
  GameState state;
  state.players = resolve_players(reader_pid, require_handle);
  state.local = resolve_local_player(reader_pid, require_handle);
  state.bomb = resolve_bomb(reader_pid, require_handle);
  state.view_matrix = resolve_view_matrix(reader_pid, require_handle);
  state.local_team = state.local.team;
  state.player_count = static_cast<int>(state.players.size());
  state.round_phase =
      state.bomb.planted ? RoundPhase_Live : RoundPhase_Unknown;
  state.map_name = map_name ? map_name : "de_dust2";
  state.entity_list_ptr = SimLayout::ENTITY_LIST;
  state.local_player_ptr = get_local_player_ptr(state);
  state.local_controller_ptr =
      state.local.controller_index
          ? SimLayout::CONTROLLER_AREA +
                state.local.controller_index * SimLayout::CONTROLLER_STRIDE
          : 0;
  state.valid = !state.players.empty();
  return state;
}

uintptr_t get_entity_list_ptr(const GameState& state) {
  return state.entity_list_ptr ? state.entity_list_ptr : SimLayout::ENTITY_LIST;
}

uintptr_t get_local_player_ptr(const GameState& state) {
  if (state.local_player_ptr) return state.local_player_ptr;
  if (state.local.pawn_index == 0) return 0;
  return SimLayout::PAWN_AREA +
         state.local.pawn_index * SimLayout::PAWN_STRIDE;
}

uintptr_t get_local_controller_ptr(const GameState& state) {
  if (state.local_controller_ptr) return state.local_controller_ptr;
  if (state.local.controller_index == 0) return 0;
  return SimLayout::CONTROLLER_AREA +
         state.local.controller_index * SimLayout::CONTROLLER_STRIDE;
}


}  // namespace cs2
