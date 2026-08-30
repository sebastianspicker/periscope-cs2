// radar.hpp — Real CS2 radar data reader.
//
// LESSON: In CS2, the HUD radar data is a separate structure from the
// entity list. It contains pre-computed 2D map positions, icons, and
// visibility state. Reading radar data directly gives the cheat a
// "legit" information source that mirrors what the player sees.
//
// CS2's built-in anti-cheat checks the radar structure for external
// reads (diagnostic system). This is one of the key detection vectors
// in CS2's trust factor / VACnet systems.
//
// Educational design:
//   REAL MODE:   Reads the real CS2 radar structure from process memory.
//   SIM MODE:    Uses cs2::HudRadarState from the sim radar system.
//   Both modes produce identical radar blips for visualization.

#pragma once

#include "real/cs2/entities.hpp"
#include "real/cs2/memory.hpp"
#include "real/cs2/offsets.hpp"
#include "real/error.hpp"
#include "real/mode/mode.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::cs2 {

/// A single blip on the radar.
struct RadarBlip {
  std::uint32_t entity_id = 0;
  float x = 0, y = 0;           ///< 2D map position
  float height = 0;             ///< Z height for elevation indicator
  std::uint8_t team = 0;
  bool is_alive = false;
  bool is_visible = false;       ///< Whether on-screen / in view frustum
  bool is_local = false;
  bool is_bomb_carrier = false;
  std::uint8_t icon = 0;         ///< Radar icon type

  std::string describe() const;
};

/// Full radar state snapshot.
struct RadarSnapshot {
  std::vector<RadarBlip> blips;
  float map_scale = 1.0f;
  float map_origin_x = 0, map_origin_y = 0;
  int map_size = 0;
  std::uint64_t read_time_us = 0;

  int alive_count() const;
  int visible_count() const;
  std::string describe() const;
};

/// Read the CS2 HUD radar data from the real process.
///
/// REAL MODE:   Reads from cs2.exe memory via the tier backend.
/// SIM MODE:    Reads from sim::World's radar simulation.
///
/// Parameters:
///   reader   - Attached Cs2MemoryReader
///   offsets  - Resolved CS2 offsets
RadarSnapshot read_radar(Cs2MemoryReader& reader, const Cs2Offsets& offsets);

/// Read just the local player's radar data.
Result<RadarBlip> read_local_radar_blip(Cs2MemoryReader& reader,
                                         const Cs2Offsets& offsets);

/// Convert entity list to radar blips (when direct radar read is not
/// available or when using entity positions instead of radar struct).
std::vector<RadarBlip> entities_to_blips(
    const std::vector<Cs2PlayerEntity>& entities,
    float map_scale, float map_origin_x, float map_origin_y);

}  // namespace real::cs2
