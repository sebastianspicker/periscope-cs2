#pragma once

#include "cs2/entities.hpp"
#include "cs2/radar_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace cs2 {

inline void render_radar_console(const char* title,
                                 const std::vector<PlayerData>& players,
                                 const PlayerData* local,
                                 const HudRadarState& hud,
                                 bool show_all) {
  constexpr int kRows = 13;
  constexpr int kCols = 29;
  char grid[kRows][kCols + 1];
  for (auto& row : grid) {
    std::fill(row, row + kCols, '.');
    row[kCols] = '\0';
  }

  const int center_row = kRows / 2;
  const int center_col = kCols / 2;
  grid[center_row][center_col] = 'L';
  if (local) {
    for (const auto& player : players) {
      if (player.pawn_index == local->pawn_index ||
          player.life_state != LifeState_Alive) continue;
      const bool teammate = player.team == local->team;
      if (!show_all && teammate) continue;
      const RadarLayout layout = compute_layout(hud, local->eye_angles.yaw);
      const auto point = translate_to_radar_position(
          hud, player.origin, layout.yaw_radians, layout.radar_to_texture_scale);
      // HUD transforms are map-texture scaled, while visibility_size is in HUD
      // pixels. Cap the console viewport so distinct transformed blips remain
      // visible in this compact textual representation.
      const float range = std::clamp(hud.visibility_size * 0.5f, 1.0f, 5.0f);
      int row = center_row - static_cast<int>(std::lround(
          point.position.y / range * center_row));
      int col = center_col + static_cast<int>(std::lround(
          point.position.x / range * center_col));
      row = std::clamp(row, 0, kRows - 1);
      col = std::clamp(col, 0, kCols - 1);
      grid[row][col] = teammate ? 'C' : 'T';
    }
  }

  std::printf("\n[CS2 HUD Radar] %s\n", title);
  for (const auto& row : grid) std::printf("  %s\n", row);
  std::printf("  L=local yaw=%.1f  C=CT (blue)  T=T (yellow/red)\n",
              local ? local->eye_angles.yaw : 0.0f);
  for (const auto& player : players) {
    std::printf("  %-8s %s hp=%3d pos=(%.0f,%.0f)\n", player.name.c_str(),
                player.team == Team_CT ? "CT" : "T ", player.health,
                player.origin.x, player.origin.y);
  }
}

}  // namespace cs2
