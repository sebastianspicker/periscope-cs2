#include "real/cs2/radar.hpp"

#include "real/cs2/entities.hpp"

#include <chrono>
#include <cstring>
#include <sstream>

namespace real::cs2 {
namespace {

template <typename T>
bool read_value(Cs2MemoryReader& reader, std::uint64_t address, T& value) {
  const auto read = reader.read(address, sizeof(T));
  if (read.status != ac::Status::Ok || read.bytes.size() != sizeof(T)) return false;
  std::memcpy(&value, read.bytes.data(), sizeof(value));
  return true;
}

}  // namespace

std::string RadarBlip::describe() const {
  std::ostringstream out;
  out << "entity=" << entity_id << " pos=(" << x << ',' << y << ") team="
      << static_cast<int>(team) << " alive=" << (is_alive ? "true" : "false");
  return out.str();
}

int RadarSnapshot::alive_count() const {
  int count = 0;
  for (const auto& blip : blips) count += blip.is_alive ? 1 : 0;
  return count;
}

int RadarSnapshot::visible_count() const {
  int count = 0;
  for (const auto& blip : blips) count += blip.is_visible ? 1 : 0;
  return count;
}

std::string RadarSnapshot::describe() const {
  std::ostringstream out;
  out << "blips=" << blips.size() << " alive=" << alive_count()
      << " visible=" << visible_count() << " scale=" << map_scale
      << " read_time_us=" << read_time_us;
  return out.str();
}

RadarSnapshot read_radar(Cs2MemoryReader& reader, const Cs2Offsets& offsets) {
  RadarSnapshot snapshot;
  const auto start = std::chrono::high_resolution_clock::now();

#if LR_PLATFORM_WINDOWS
  // Reading the HUD radar header confirms that the game's radar state is mapped.
  // Its public offsets describe map transform metadata, not a stable blip layout.
  float radar_header[4]{};
  const auto header = reader.read(offsets.radar_base, sizeof(radar_header));
  if (header.status != ac::Status::Ok || header.bytes.size() != sizeof(radar_header)) {
    snapshot.read_time_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start).count());
    return snapshot;
  }
  std::memcpy(radar_header, header.bytes.data(), sizeof(radar_header));

  // Reading the map origin translates world X/Y positions into radar coordinates.
  snapshot.map_origin_x = radar_header[0];
  snapshot.map_origin_y = radar_header[1];
  if (offsets.radar_pos_x != 0) read_value(reader, offsets.radar_base + offsets.radar_pos_x,
                                            snapshot.map_origin_x);
  if (offsets.radar_pos_y != 0) read_value(reader, offsets.radar_base + offsets.radar_pos_y,
                                            snapshot.map_origin_y);

  // Reading the map scale preserves the HUD's world-to-texture conversion.
  snapshot.map_scale = radar_header[2] > 0.0f ? radar_header[2] : 1.0f;
  if (offsets.radar_scale != 0) read_value(reader, offsets.radar_base + offsets.radar_scale,
                                           snapshot.map_scale);

  // Reading map size tells consumers the coordinate extent of the loaded overview.
  float map_size = radar_header[3];
  if (offsets.radar_size != 0) read_value(reader, offsets.radar_base + offsets.radar_size,
                                          map_size);
  snapshot.map_size = map_size > 0.0f ? static_cast<int>(map_size) : 0;
#else
  (void)reader;
  (void)offsets;
#endif

  snapshot.read_time_us = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() - start).count());
  return snapshot;
}

Result<RadarBlip> read_local_radar_blip(Cs2MemoryReader& reader,
                                        const Cs2Offsets& offsets) {
#if LR_PLATFORM_WINDOWS
  // The local entity supplies the world position; the HUD transform supplies map coordinates.
  const auto local = read_local_player(reader, offsets);
  if (!local) return {{}, local.error_msg};
  const auto radar = read_radar(reader, offsets);
  const auto blips = entities_to_blips({*local}, radar.map_scale,
                                       radar.map_origin_x, radar.map_origin_y);
  if (blips.empty()) return {{}, "Failed to convert local player to a radar blip"};
  return Result<RadarBlip>(blips.front());
#else
  (void)reader;
  (void)offsets;
  return {{}, "Real CS2 radar reads are supported only on Windows"};
#endif
}

std::vector<RadarBlip> entities_to_blips(const std::vector<Cs2PlayerEntity>& entities,
                                         float map_scale, float map_origin_x,
                                         float map_origin_y) {
  std::vector<RadarBlip> blips;
  blips.reserve(entities.size());
  for (const auto& entity : entities) {
    // World X/Y are shifted by the overview origin and scaled into map pixels.
    RadarBlip blip;
    blip.entity_id = entity.controller_handle;
    blip.x = (entity.origin.x - map_origin_x) * map_scale;
    blip.y = (map_origin_y - entity.origin.y) * map_scale;
    blip.height = entity.origin.z;
    blip.team = static_cast<std::uint8_t>(entity.team);
    blip.is_alive = entity.is_alive;
    blip.is_visible = entity.is_alive;
    blip.is_local = entity.is_local_player;
    blips.push_back(blip);
  }
  return blips;
}

}  // namespace real::cs2
