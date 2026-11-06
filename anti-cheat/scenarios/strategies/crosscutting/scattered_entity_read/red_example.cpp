#include "red_example.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace examples::scattered_entity_read {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 scattered_entity_read] FAIL: game process is unavailable\n");
    return {false, steps, false, false, "precondition failed: game process unavailable"};
  }
  const auto game_base = game->base;
  std::printf("[T0 scattered_entity_read] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("radar-reader.exe");
  if (!w.proc(actor)) {
    std::printf("[T0 scattered_entity_read] FAIL: actor creation was not observable\n");
    return {false, steps, false, false, "actor spawn verification failed"};
  }
  std::printf("[T0 scattered_entity_read] step %d: reader pid=%u created\n", ++steps, actor);

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 scattered_entity_read] FAIL: VM_READ handle denied\n");
    return {false, steps, false, false, "OpenProcess VM_READ failed"};
  }
  bool has_handle = false;
  for (const auto& handle : w.handles_to(game_pid)) {
    has_handle = has_handle || (handle.owner_pid == actor &&
                                sim::has(handle.access, sim::AccessMask::VmRead));
  }
  if (!has_handle) return {false, steps, false, false, "VM_READ handle verification failed"};
  std::printf("[T0 scattered_entity_read] step %d: VM_READ edge verified\n", ++steps);

  const auto table_base = game_base + w.lab_entity_table_rel;
  const auto count_read = w.read_mem(actor, game_pid, table_base, sizeof(std::uint32_t), true);
  std::uint32_t entity_count = 0;
  if (count_read.status != ac::Status::Ok || count_read.bytes.size() != sizeof(entity_count)) {
    std::printf("[T0 scattered_entity_read] FAIL: entity count read failed\n");
    return {false, steps, false, false, "entity count read failed"};
  }
  std::memcpy(&entity_count, count_read.bytes.data(), sizeof(entity_count));
  if (entity_count == 0) return {false, steps, false, false, "entity count was zero"};
  std::printf("[T0 scattered_entity_read] step %d: entity count=%u read first\n", ++steps,
              entity_count);

  std::vector<std::uint32_t> order(entity_count);
  for (std::uint32_t index = 0; index < entity_count; ++index) order[index] = index;
  std::mt19937 rng(actor ^ game_pid);
  std::shuffle(order.begin(), order.end(), rng);
  if (std::is_sorted(order.begin(), order.end()) && order.size() > 1) {
    std::reverse(order.begin(), order.end());
  }

  w.read_timing_jitter = true;
  bool data_obtained = true;
  for (const auto index : order) {
    const auto entity_address = table_base + 0x10 + static_cast<std::uint64_t>(index) * 16;
    const auto entity_read = w.read_mem(actor, game_pid, entity_address, 16, true);
    data_obtained = data_obtained && entity_read.status == ac::Status::Ok &&
                    entity_read.bytes.size() == 16;
  }
  const bool non_sequential = order.size() < 2 ||
      !std::is_sorted(order.begin(), order.end());
  w.scattered_read_pattern = data_obtained && non_sequential;
  w.scattered_read_count = data_obtained ? static_cast<int>(entity_count) : 0;
  const bool scar_verified = w.scattered_read_pattern && w.scattered_read_count ==
      static_cast<int>(entity_count) && w.read_timing_jitter;
  if (!scar_verified) {
    std::printf("[T0 scattered_entity_read] FAIL: scattered read scar was not retained\n");
    return {false, steps, w.read_timing_jitter, non_sequential,
            "scar verification failed: scattered reads with timing jitter"};
  }
  std::printf("[T0 scattered_entity_read] step %d: %u scattered entity reads with jitter verified\n",
              ++steps, entity_count);
  const std::string detail = "scattered_entity_read: randomized entity order with timing jitter";
  w.note(detail);
  return {has_handle && scar_verified, steps, w.read_timing_jitter, non_sequential, detail};
}

}  // namespace examples::scattered_entity_read
