#pragma once

// Educational simulation: fixture-based testing with reproducible game state.
// Fixtures provide synthetic entity snapshots and red strategy configuration
// so that detection tests produce consistent results across runs.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sim {

/// A synthetic game state snapshot for reproducible testing.
struct FixtureSnapshot {
  std::string name;                           // Fixture name for reporting
  std::uint32_t game_pid = 0;
  std::vector<ac::EntitySnapshot> entities;   // Entity data (positions, teams, alive)
  ac::Vec3 local_origin{};                    // Local player position
  std::uint64_t tick = 0;

  // Red team configuration for this fixture
  ac::MemoryAcquisitionModel acq_model = ac::MemoryAcquisitionModel::DirectRpm;
  ac::HandleAcquisitionModel handle_model = ac::HandleAcquisitionModel::DirectOpenProcess;
  bool evasion_enabled = false;
  ac::DisguiseProfile disguise = ac::DisguiseProfile::None;

  std::string detail;
};

/// Load and save fixtures for reproducible testing.
class FixtureLoader {
 public:
  /// Load a fixture by name. Returns empty if not found.
  static FixtureSnapshot load(const std::string& fixture_name);

  /// Save a fixture for later reloading.
  static void save(const std::string& fixture_name, const FixtureSnapshot& snap);

  /// Apply a fixture to a World (set up game state exactly as fixture specifies).
  /// Spawns a single game process if needed, plants entity table into game memory,
  /// and records acquisition model scars. Returns game_pid (also written to fixture).
  static std::uint32_t apply_to_world(FixtureSnapshot& fixture, World& world);

  /// Const overload: plants without mutating fixture.game_pid when already set.
  static std::uint32_t apply_to_world(const FixtureSnapshot& fixture, World& world);

  /// Create a standard "2v2" fixture: 2 allies, 2 enemies, local player, all alive.
  static FixtureSnapshot create_2v2();

  /// Create an "empty" fixture: only local player, no enemies.
  static FixtureSnapshot create_empty();

  /// Create a "full server" fixture: 9 enemies, 4 allies, local player.
  static FixtureSnapshot create_full_server();

  /// Create a fixture tailored for a given acquisition model.
  static FixtureSnapshot create_for_model(ac::MemoryAcquisitionModel model);

  /// List names currently stored in the in-memory registry.
  static std::vector<std::string> list_saved();

  /// Clear the in-memory fixture registry.
  static void clear_registry();
};

}  // namespace sim
