#pragma once

#include "cs2/offsets.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cs2 {

struct Vector3 {
  float x = 0, y = 0, z = 0;

  Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
  float length_sq() const { return x * x + y * y + z * z; }
  float length() const;
  float distance_to(const Vector3& o) const;
};

struct Vector2 {
  float x = 0, y = 0;

  Vector2 operator+(const Vector2& o) const { return {x + o.x, y + o.y}; }
  Vector2 operator-(const Vector2& o) const { return {x - o.x, y - o.y}; }
  Vector2 operator*(float s) const { return {x * s, y * s}; }
  float length_sq() const { return x * x + y * y; }
};

struct QAngle {
  // Source names. x/y/z aliases match older Vector2-style call sites
  // (eye_angles.y == yaw).
  float pitch = 0;
  float yaw = 0;
  float roll = 0;
  float& x() { return pitch; }
  float& y() { return yaw; }
  float& z() { return roll; }
  float x() const { return pitch; }
  float y() const { return yaw; }
  float z() const { return roll; }
  QAngle() = default;
  QAngle(float p, float yw, float r = 0.f) : pitch(p), yaw(yw), roll(r) {}
};

// Dual-representation player snapshot (controller + pawn), mirroring the
// Periscope entity collection model used by live radar.
struct PlayerData {
  uint32_t controller_index = 0;
  uint32_t pawn_index = 0;
  uint32_t controller_handle = 0;
  uint32_t pawn_handle = 0;
  int health = 0;
  int armor = 0;
  int team = 0;
  int life_state = 0;
  int desired_fov = 90;
  Vector3 origin{};           // preferred (scene-node abs origin)
  Vector3 old_origin{};       // m_vOldOrigin fallback
  QAngle eye_angles{};
  uint64_t spotted_mask = 0;
  bool is_scoped = false;
  bool is_defusing = false;
  bool is_local = false;
  bool is_alive = false;
  float flash_duration = 0.f;
  uint32_t active_weapon_handle = 0;
  std::string name;
  std::string weapon_name;
};

struct BombData {
  bool planted = false;
  bool ticking = false;
  bool defused = false;
  Vector3 origin{};
  float timer = 0.f;
  float defuse_countdown = 0.f;
  int site = 0;  // 0 = A, 1 = B
  std::string site_name;
  uint32_t entity_index = 0;
};

struct ViewMatrix {
  float m[4][4]{};
  bool valid = false;
};

struct GameState {
  std::vector<PlayerData> players;
  PlayerData local;
  BombData bomb;
  ViewMatrix view_matrix;
  int local_team = 0;
  int round_phase = 0;
  int player_count = 0;
  int tick = 0;
  std::string map_name;
  uintptr_t entity_list_ptr = SimLayout::ENTITY_LIST;
  uintptr_t local_player_ptr = 0;
  uintptr_t local_controller_ptr = 0;
  bool valid = false;
};

enum Team : int {
  Team_None = 0,
  Team_Spectate = 1,
  Team_T = 2,
  Team_CT = 3,
};

enum LifeState : int {
  LifeState_Alive = 0,
  LifeState_Dying = 1,
  LifeState_Dead = 2,
  LifeState_Respawnable = 3,
  LifeState_DiscardBody = 4,
};

enum RoundPhase : int {
  RoundPhase_Unknown = 0,
  RoundPhase_Freeze = 1,
  RoundPhase_Live = 2,
  RoundPhase_Over = 3,
};

inline bool is_player_valid(int health, int team) {
  return health > 0 && health <= 200 && (team == Team_T || team == Team_CT);
}

inline bool is_player_alive(const PlayerData& p) {
  return p.life_state == LifeState_Alive && p.health > 0;
}

inline const char* team_name(int team) {
  switch (team) {
    case Team_T: return "T";
    case Team_CT: return "CT";
    case Team_Spectate: return "SPEC";
    default: return "NONE";
  }
}

inline const char* bomb_site_name(int site) {
  return site == 0 ? "A" : (site == 1 ? "B" : "?");
}

// Stable health display: hold previous value for a few frames through bad reads.
int stable_display_health(int current_health, int previous_health,
                          int& hold_counter, int hold_frames = 8);

// Project enemies relative to local player for simple radar blips (world units).
struct RadarBlip {
  float x = 0;
  float y = 0;
  float height = 0;
  int team = 0;
  int health = 0;
  bool is_local = false;
  bool is_alive = false;
  bool is_enemy = false;
  std::string name;
};

std::vector<RadarBlip> players_to_blips(const std::vector<PlayerData>& players,
                                        const PlayerData* local,
                                        float map_scale = 1.f,
                                        float map_origin_x = 0.f,
                                        float map_origin_y = 0.f);

}  // namespace cs2
