#include "ac_sim/behavioral_filter.hpp"
#include "ac_sim/disguise.hpp"
#include "ac_sim/dll_watch.hpp"
#include "ac_sim/etl.hpp"
#include "ac_sim/forensic.hpp"
#include "ac_sim/xorshift.hpp"
#include "sim/world.hpp"

#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
}
}  // namespace

int main() {
  auto world = sim::make_arena("lab-game");
  const auto game = world.game_pid();
  auto* process = world.proc(game);
  expect(game != 0 && process != nullptr, "arena creates the game process");
  expect(world.lab_pattern_marker_present, "arena plants the marker");

  const auto reader = world.spawn("reader.exe");
  expect(world.open_process(reader, game, sim::AccessMask::VmRead, false),
         "reader can open the simulated game");
  const auto read = world.read_mem(reader, game, process->base, sizeof(std::uint32_t), true);
  std::uint32_t count = 0;
  if (read.bytes.size() == sizeof(count)) std::memcpy(&count, read.bytes.data(), sizeof(count));
  expect(read.status == ac::Status::Ok && count >= 2, "arena data is readable");

  std::vector<ac::EntitySnapshot> entities = {
      {1, {1.f, 2.f, 3.f}, 1, true}, {2, {9.f, 8.f, 7.f}, 2, false}};
  expect(world.plant_entity_snapshots(game, entities, 0x50), "plant snapshots");
  const auto restored = world.read_entity_snapshots(game, 0x50);
  expect(restored.size() == 2 && restored[0].origin.x == 1.f && !restored[1].alive,
         "read planted snapshots");

  const auto extent = process->memory.size();
  const std::uint8_t value = 0xA5;
  expect(!world.write_mem(game, std::numeric_limits<std::uint64_t>::max(), &value,
                          static_cast<std::size_t>(process->base) + 1),
         "overflow write is rejected");
  expect(process->memory.size() == extent, "overflow does not grow memory");
  const auto truncated = world.read_mem(reader, game, process->base + extent - 2, 4, true);
  expect(truncated.status == ac::Status::InvalidArgument && truncated.bytes.empty(),
         "truncated reads do not return partial data");

  sim::DisguiseEngine first_disguise;
  sim::DisguiseEngine replay_disguise;
  first_disguise.initialize(sim::DisguiseStyle::Afterburner, 0x1234u);
  replay_disguise.initialize(sim::DisguiseStyle::Afterburner, 0x1234u);
  const auto first_metrics = first_disguise.sample();
  const auto replay_metrics = replay_disguise.sample();
  expect(first_metrics.fakeGpuTemp == replay_metrics.fakeGpuTemp &&
             first_metrics.fakeFps == replay_metrics.fakeFps &&
             first_metrics.osdText == replay_metrics.osdText,
         "explicit disguise seed replays synthetic metrics");

  sim::BehavioralFilter first_filter;
  sim::BehavioralFilter replay_filter;
  sim::BehavioralFilterConfig filter_config;
  filter_config.applyDelay = true;
  first_filter.initialize(filter_config, 0x5678u);
  replay_filter.initialize(filter_config, 0x5678u);
  ac::EntitySnapshot first_entity(42, {90.f, 1.f, 0.f}, 2, true);
  ac::EntitySnapshot replay_entity = first_entity;
  first_filter.filter_entities(&first_entity, 1, {}, 0.f, false, 3.14f, 3.14f);
  replay_filter.filter_entities(&replay_entity, 1, {}, 0.f, false, 3.14f, 3.14f);
  expect(first_entity.alive == replay_entity.alive &&
             first_entity.origin.x == replay_entity.origin.x &&
             first_filter.stats().lastDelayMs == replay_filter.stats().lastDelayMs,
         "explicit behavioral seed replays filtering without host delay");

  sim::ForensicEngine forensic;
  forensic.initialize();
  const auto cleanup = forensic.execute_cleanup(true);
  expect(cleanup.prefetchCleaned && cleanup.recentItemsCleaned && cleanup.errors.empty(),
         "forensic simulation records a synthetic cleanup only");

  sim::DllWatch watcher;
  watcher.initialize();
  watcher.start_monitoring();
  watcher.push_notification("vac_module.dll");
  const auto notifications = watcher.pending_notifications();
  expect(watcher.poll_modules() == 0 && notifications.size() == 1 &&
             notifications[0].isSuspicious,
         "DLL watch accepts injected simulation events without host enumeration");

  sim::ExecutionTimeline first_timeline;
  sim::ExecutionTimeline replay_timeline;
  first_timeline.record(sim::EtlEventType::Frame, "frame", 7);
  replay_timeline.record(sim::EtlEventType::Frame, "frame", 7);
  expect(first_timeline.at(0) != nullptr && replay_timeline.at(0) != nullptr &&
             first_timeline.at(0)->timestampNs == replay_timeline.at(0)->timestampNs &&
             first_timeline.at(0)->timestampNs == 1'000'000,
         "each timeline starts with the same logical replay clock");

  sim::XorShiftPool::seed_all(0, true);
  const auto first_rng = sim::XorShiftPool::instance(sim::XorShiftInstance::Temporal).next();
  sim::XorShiftPool::seed_all(0, true);
  const auto replay_rng = sim::XorShiftPool::instance(sim::XorShiftInstance::Temporal).next();
  expect(first_rng == replay_rng,
         "zero-seed RNG fallback is deterministic across replay initialization");
  return failures;
}
