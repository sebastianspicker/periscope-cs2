#include "cs2/entities.hpp"

#include <cmath>

namespace cs2 {

float Vector3::length() const {
  return std::sqrt(length_sq());
}

float Vector3::distance_to(const Vector3& o) const {
  return (*this - o).length();
}

int stable_display_health(int current_health, int previous_health,
                          int& hold_counter, int hold_frames) {
  if (hold_frames < 1) hold_frames = 1;
  // Accept a good read immediately.
  if (current_health > 0 && current_health <= 200) {
    hold_counter = 0;
    return current_health;
  }
  // Bad/zero frame: hold previous for a few ticks so radar dots don't flicker.
  if (previous_health > 0 && hold_counter < hold_frames) {
    ++hold_counter;
    return previous_health;
  }
  hold_counter = hold_frames;
  return current_health > 0 ? current_health : 0;
}

std::vector<RadarBlip> players_to_blips(const std::vector<PlayerData>& players,
                                        const PlayerData* local,
                                        float map_scale,
                                        float map_origin_x,
                                        float map_origin_y) {
  std::vector<RadarBlip> blips;
  blips.reserve(players.size());
  const float scale = map_scale != 0.f ? map_scale : 1.f;
  for (const auto& player : players) {
    RadarBlip blip;
    blip.x = (player.origin.x - map_origin_x) * scale;
    // Source Y is left; invert so +Y maps "up" when drawn like a mini-map.
    blip.y = (map_origin_y - player.origin.y) * scale;
    blip.height = player.origin.z;
    blip.team = player.team;
    blip.health = player.health;
    blip.is_local = player.is_local ||
                    (local && player.pawn_index == local->pawn_index);
    blip.is_alive = player.is_alive || is_player_alive(player);
    blip.is_enemy = local && player.team != local->team &&
                    (player.team == Team_T || player.team == Team_CT);
    blip.name = player.name;
    blips.push_back(std::move(blip));
  }
  return blips;
}

}  // namespace cs2
