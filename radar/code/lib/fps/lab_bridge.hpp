#pragma once

// Bridge dummy FPS players → lab-readable entity snapshots / sim::World.
// Enough for radar pedagogy; not a second strategy catalog.

#include "fps/scenario.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <vector>

namespace fps {

/// Convert living (or all) players to ac::EntitySnapshot list for interest mgmt /
/// info-advantage demos. team: 0=attacker, 1=defender.
std::vector<ac::EntitySnapshot> to_entity_snapshots(const Scenario& sc,
                                                    bool living_only = false);

/// Plant FPS player origins into a sim::World game process memory layout
/// (same plant_lab_entities-compatible count + Ent structs at base+0x10).
/// Returns false if no game process in world.
bool sync_entities_to_sim(const Scenario& sc, sim::World& w);

/// Spawn lab game + AC processes and write current scenario entities into game
/// memory so T0 RPM-style readers can pull positions (educational stand-in).
sim::World make_lab_world_from_scenario(const Scenario& sc,
                                        const char* game_name = "dusty-fps.exe");

}  // namespace fps
