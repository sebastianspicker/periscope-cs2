#pragma once

#include "cs2/entities.hpp"
#include "cs2/offsets.hpp"
#include "cs2/radar_math.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cs2 {

struct SimCs2Arena {
  sim::World world;
  std::uint32_t game_pid = 0;
  std::uint32_t local_player_id = 0;
  std::vector<std::uint32_t> player_ids;
  std::string map_name;
  BombData bomb;
};

// Build a full educational CS2 arena with entity list, HUD radar, bomb, and
// diagnostic baseline state. map_name selects spawn layout presets.
SimCs2Arena make_cs2_arena(int player_count = 10,
                           const char* map_name = "de_dust2");

void plant_cs2_entity_data(sim::World& world, std::uint32_t game_pid,
                           const std::vector<PlayerData>& players);
void plant_cs2_hud_data(sim::World& world, std::uint32_t game_pid,
                        const char* map_name = "de_dust2");
void plant_cs2_bomb_data(sim::World& world, std::uint32_t game_pid,
                         const BombData& bomb);
void plant_cs2_view_matrix(sim::World& world, std::uint32_t game_pid,
                           const ViewMatrix& matrix);
void plant_cs2_diagnostic_baseline(sim::World& world);

// Map-aware default bomb site origin (A site).
Vector3 default_bomb_origin(const char* map_name);
// Map-aware default HUD map texture position for polar radar.
Vector3 default_map_texture_position(const char* map_name);

struct Cs2GameReader {
  sim::World* world = nullptr;
  std::uint32_t game_pid = 0;

  bool read_raw(std::uint32_t reader_pid, std::uint64_t addr,
                std::vector<std::uint8_t>& out, std::size_t size,
                bool require_handle);

  int read_int(std::uint32_t reader_pid, std::uint64_t addr, bool require_handle);
  float read_float(std::uint32_t reader_pid, std::uint64_t addr,
                   bool require_handle);
  uint64_t read_ptr(std::uint32_t reader_pid, std::uint64_t addr,
                    bool require_handle);
  void read_buf(std::uint32_t reader_pid, std::uint64_t addr, void* out,
                std::size_t size, bool require_handle);
  std::string read_string(std::uint32_t reader_pid, std::uint64_t addr,
                          std::size_t max_len, bool require_handle);

  std::vector<PlayerData> resolve_players(std::uint32_t reader_pid,
                                          bool require_handle);
  PlayerData resolve_local_player(std::uint32_t reader_pid, bool require_handle);
  HudRadarState resolve_hud_radar(std::uint32_t reader_pid, bool require_handle);
  BombData resolve_bomb(std::uint32_t reader_pid, bool require_handle);
  ViewMatrix resolve_view_matrix(std::uint32_t reader_pid, bool require_handle);
  GameState resolve_game_state(std::uint32_t reader_pid, bool require_handle,
                               const char* map_name = nullptr);
};

// Educational helpers over a assembled GameState snapshot.
uintptr_t get_entity_list_ptr(const GameState& state);
uintptr_t get_local_player_ptr(const GameState& state);
uintptr_t get_local_controller_ptr(const GameState& state);

// Seed a minimal set of diagnostic ConVars / VMTs for blue sensor exercises.
void seed_diagnostic_fixtures(sim::World& world, bool include_cheat_scars = false);

}  // namespace cs2
