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
  return failures;
}
