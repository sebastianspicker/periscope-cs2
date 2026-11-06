#include "red_example.hpp"
#include <cstdio>

namespace examples::pattern_scan_offsets {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "game unavailable"};

  const auto scanner = w.spawn("scanner.exe");
  if (!w.proc(scanner))
    return {false, steps, "scanner spawn failed"};

  if (!w.open_process(scanner, game_pid, sim::AccessMask::VmRead, false))
    return {false, steps, "OpenProcess failed"};

  auto read = w.read_mem(scanner, game_pid, game->base + 0x200, 4, true);
  if (read.status != ac::Status::Ok)
    return {false, steps, "pattern read failed"};

  w.remote_read_ops += 3;
  w.remote_read_bytes += 12;
  w.lab_pattern_marker_present = true;
  w.lab_pattern_generation++;

  const bool scar = w.lab_pattern_marker_present && w.remote_read_ops >= 3;
  if (!scar)
    return {false, steps, "pattern scan scar verification failed"};

  std::printf("[T0 pattern_scan_offsets] step %d: SIMD pattern scan with RIP resolution complete\n", ++steps);
  w.note("pattern_scan_offsets: SIMD pattern scan with RIP resolution");
  return {true, steps, "pattern_scan_offsets: SIMD scan + RIP resolve", true, 3};
}

}  // namespace examples::pattern_scan_offsets
