#include "sim/fixture.hpp"

#include <map>

namespace sim {
namespace {

// In-memory fixture storage (in a real lab this would be files).
std::map<std::string, FixtureSnapshot>& registry() {
  static std::map<std::string, FixtureSnapshot> fixtures;
  return fixtures;
}

std::uint32_t apply_impl(const FixtureSnapshot& fixture, World& world,
                         FixtureSnapshot* mutable_fixture) {
  std::uint32_t game = world.game_pid();
  if (game == 0) {
    game = world.spawn("cs2.exe", true, false);
    world.plant_lab_entities(game);
  }
  if (world.ac_pid() == 0) {
    world.spawn("ac-agent", false, true);
  }

  // Plant fixture entities into the single game process memory image.
  if (!fixture.entities.empty()) {
    world.plant_entity_snapshots(game, fixture.entities, 0);
  }

  world.active_memory_model = fixture.acq_model;
  world.active_handle_model = fixture.handle_model;
  world.lab_match_tick = static_cast<int>(fixture.tick);

  if (fixture.evasion_enabled) {
    world.read_timing_jitter = true;
    world.scattered_read_pattern = true;
  }

  world.note("fixture applied name=" + fixture.name +
             " entities=" + std::to_string(fixture.entities.size()) +
             " game_pid=" + std::to_string(game));

  if (mutable_fixture != nullptr) {
    mutable_fixture->game_pid = game;
  }
  return game;
}

}  // namespace

FixtureSnapshot FixtureLoader::load(const std::string& fixture_name) {
  const auto it = registry().find(fixture_name);
  if (it != registry().end()) {
    return it->second;
  }
  return FixtureSnapshot{};
}

void FixtureLoader::save(const std::string& fixture_name,
                         const FixtureSnapshot& snap) {
  auto copy = snap;
  copy.name = fixture_name;
  registry()[fixture_name] = copy;
}

std::uint32_t FixtureLoader::apply_to_world(FixtureSnapshot& fixture,
                                            World& world) {
  return apply_impl(fixture, world, &fixture);
}

std::uint32_t FixtureLoader::apply_to_world(const FixtureSnapshot& fixture,
                                            World& world) {
  return apply_impl(fixture, world, nullptr);
}

FixtureSnapshot FixtureLoader::create_2v2() {
  FixtureSnapshot snap;
  snap.name = "2v2";
  snap.tick = 1;
  snap.local_origin = {0.0f, 0.0f, 0.0f};

  // Local player (team 1).
  snap.entities.push_back({1, {0.0f, 0.0f, 0.0f}, 1, true});
  // Ally (team 1).
  snap.entities.push_back({2, {500.0f, 200.0f, 0.0f}, 1, true});
  // Enemy 1 (team 2).
  snap.entities.push_back({3, {-1000.0f, 500.0f, 0.0f}, 2, true});
  // Enemy 2 (team 2).
  snap.entities.push_back({4, {2000.0f, -800.0f, 0.0f}, 2, true});

  snap.detail = "2 allies + 2 enemies";
  return snap;
}

FixtureSnapshot FixtureLoader::create_empty() {
  FixtureSnapshot snap;
  snap.name = "empty";
  snap.tick = 1;
  snap.local_origin = {0.0f, 0.0f, 0.0f};
  snap.entities.push_back({1, {0.0f, 0.0f, 0.0f}, 1, true});
  snap.detail = "Only local player";
  return snap;
}

FixtureSnapshot FixtureLoader::create_full_server() {
  FixtureSnapshot snap;
  snap.name = "full_server";
  snap.tick = 1;
  snap.local_origin = {0.0f, 0.0f, 0.0f};
  snap.entities.push_back({1, {0.0f, 0.0f, 0.0f}, 1, true});

  // 4 allies (team 1).
  snap.entities.push_back({2, {500.0f, 200.0f, 0.0f}, 1, true});
  snap.entities.push_back({3, {300.0f, -400.0f, 0.0f}, 1, true});
  snap.entities.push_back({4, {-200.0f, 600.0f, 0.0f}, 1, true});
  snap.entities.push_back({5, {800.0f, 100.0f, 0.0f}, 1, true});

  // 9 enemies (team 2).
  snap.entities.push_back({6, {-1000.0f, 500.0f, 0.0f}, 2, true});
  snap.entities.push_back({7, {2000.0f, -800.0f, 0.0f}, 2, true});
  snap.entities.push_back({8, {-1500.0f, -200.0f, 0.0f}, 2, true});
  snap.entities.push_back({9, {3000.0f, 1000.0f, 0.0f}, 2, true});
  snap.entities.push_back({10, {-500.0f, -1500.0f, 0.0f}, 2, true});
  snap.entities.push_back({11, {2500.0f, -500.0f, 0.0f}, 2, true});
  snap.entities.push_back({12, {-2000.0f, 1500.0f, 0.0f}, 2, true});
  snap.entities.push_back({13, {1000.0f, -2000.0f, 0.0f}, 2, true});
  snap.entities.push_back({14, {0.0f, 2000.0f, 0.0f}, 2, true});

  snap.detail = "Local + 4 allies + 9 enemies (full server)";
  return snap;
}

FixtureSnapshot FixtureLoader::create_for_model(
    ac::MemoryAcquisitionModel model) {
  auto snap = create_2v2();
  snap.acq_model = model;
  switch (model) {
    case ac::MemoryAcquisitionModel::DirectRpm:
      snap.handle_model = ac::HandleAcquisitionModel::DirectOpenProcess;
      snap.name = "model_direct_rpm";
      break;
    case ac::MemoryAcquisitionModel::Syscall:
      snap.handle_model = ac::HandleAcquisitionModel::DirectSyscall;
      snap.name = "model_syscall";
      break;
    case ac::MemoryAcquisitionModel::KernelIoctl:
      snap.handle_model = ac::HandleAcquisitionModel::KernelDriver;
      snap.name = "model_kernel_ioctl";
      break;
    case ac::MemoryAcquisitionModel::HvHypercall:
      snap.handle_model = ac::HandleAcquisitionModel::None;
      snap.name = "model_hv_hypercall";
      break;
    case ac::MemoryAcquisitionModel::DmaPhysical:
      snap.handle_model = ac::HandleAcquisitionModel::DmaPhysical;
      snap.name = "model_dma_physical";
      break;
    case ac::MemoryAcquisitionModel::HijackProxy:
      snap.handle_model = ac::HandleAcquisitionModel::HijackProxy;
      snap.name = "model_hijack_proxy";
      break;
  }
  snap.detail = std::string("fixture for ") + std::string(ac::to_string(model));
  return snap;
}

std::vector<std::string> FixtureLoader::list_saved() {
  std::vector<std::string> names;
  names.reserve(registry().size());
  for (const auto& [name, _] : registry()) {
    names.push_back(name);
  }
  return names;
}

void FixtureLoader::clear_registry() { registry().clear(); }

}  // namespace sim
