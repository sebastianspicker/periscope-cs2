// lab_bridge.cpp — map fps::Scenario players → EntitySnapshot + sim::World.
// plant_lab_entities-compatible layout for RPM/lab radar readers.

#include "fps/lab_bridge.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>
#include <limits>

namespace fps {
namespace {

// Map scenario weapon class to a stable lab weapon_id for snapshot consumers.
std::int32_t weapon_id_from_class(WeaponClass w) {
  switch (w) {
    case WeaponClass::None:
      return 0;
    case WeaponClass::Pistol:
      return 1;
    case WeaponClass::Rifle:
      return 2;
    case WeaponClass::AWP:
      return 3;
  }
  return 0;
}

// plant_lab_entities-compatible raw row (count @ base, ents @ base+0x10).
struct LabEnt {
  float x, y, z;
  std::uint8_t team, alive, pad[2];
};

}  // namespace

std::vector<ac::EntitySnapshot> to_entity_snapshots(const Scenario& sc,
                                                    bool living_only) {
  std::vector<ac::EntitySnapshot> out;
  out.reserve(sc.players().size());
  for (const auto& p : sc.players()) {
    if (living_only && !p.alive) {
      continue;
    }
    ac::EntitySnapshot e;
    e.id = p.id;
    e.origin = p.origin;
    e.team = static_cast<std::uint8_t>(p.team);
    e.health = static_cast<std::int32_t>(p.health);
    e.alive = p.alive;
    e.dormant = !p.alive;
    e.is_local_player = false;
    e.is_bomb_carrier = p.carries_bomb;
    e.weapon_id = weapon_id_from_class(p.weapon);
    // Copy display name (null-terminated, truncated to field size).
    std::fill(std::begin(e.name), std::end(e.name), '\0');
    if (!p.name.empty()) {
      const std::size_t n =
          std::min(p.name.size(), sizeof(e.name) - 1);
      std::copy_n(p.name.begin(), n, e.name);
    }
    out.push_back(e);
  }
  return out;
}

bool sync_entities_to_sim(const Scenario& sc, sim::World& w) {
  const auto gpid = w.game_pid();
  auto* g = w.proc(gpid);
  // Clean failure when no game process is present (no throw, no partial write).
  if (!g || !g->is_game) {
    return false;
  }
  const auto& players = sc.players();
  const std::uint32_t count =
      static_cast<std::uint32_t>(std::min<std::size_t>(players.size(), 32));
  static_assert(32 <=
                (std::numeric_limits<std::size_t>::max() - 0x10) / sizeof(LabEnt));
  const std::size_t need = 0x10 + static_cast<std::size_t>(count) * sizeof(LabEnt);
  if (g->memory.size() < need) {
    g->memory.resize(std::max(need, static_cast<std::size_t>(0x10 + 32 * sizeof(LabEnt))),
                     0);
  }
  const auto count_bytes =
      std::bit_cast<std::array<std::uint8_t, sizeof(count)>>(count);
  std::copy(count_bytes.begin(), count_bytes.end(), g->memory.begin());
  for (std::uint32_t i = 0; i < count; ++i) {
    LabEnt e{};
    e.x = players[i].origin.x;
    e.y = players[i].origin.y;
    e.z = players[i].origin.z;
    e.team = static_cast<std::uint8_t>(players[i].team);
    e.alive = players[i].alive ? 1 : 0;
    const auto entity_bytes =
        std::bit_cast<std::array<std::uint8_t, sizeof(LabEnt)>>(e);
    const auto offset = 0x10 + static_cast<std::size_t>(i) * sizeof(LabEnt);
    std::copy(entity_bytes.begin(), entity_bytes.end(),
              g->memory.begin() + static_cast<std::ptrdiff_t>(offset));
  }
  // Keep table rel at base (0) so RPM readers match plant_lab_entities default.
  w.lab_entity_table_rel = 0;
  w.note("fps lab_bridge synced entities=" + std::to_string(count));
  return true;
}

sim::World make_lab_world_from_scenario(const Scenario& sc,
                                        const char* game_name) {
  auto w = sim::make_arena(game_name ? game_name : "dusty-fps.exe");
  // Replace default planted entities with scenario players.
  if (!sync_entities_to_sim(sc, w)) {
    w.note("fps lab_bridge sync failed (no game process)");
  }
  return w;
}

}  // namespace fps
