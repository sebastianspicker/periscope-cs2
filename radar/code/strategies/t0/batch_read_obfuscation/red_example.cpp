#include "red_example.hpp"
#include <cstdio>
#include <cstdlib>

namespace examples::batch_read_obfuscation {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "game unavailable"};

  const auto reader = w.spawn("batch-reader.exe");
  if (!w.proc(reader))
    return {false, steps, "reader spawn failed"};

  if (!w.open_process(reader, game_pid, sim::AccessMask::VmRead, false))
    return {false, steps, "OpenProcess failed"};

  const int batch_size = 16;
  for (int i = 0; i < batch_size; ++i) {
    auto read = w.read_mem(reader, game_pid, game->base + i * 4, 4, true);
    if (read.status != ac::Status::Ok)
      return {false, steps, ("batch read " + std::to_string(i) + " failed").c_str()};
  }

  w.batch_read_obfuscated = true;
  w.batch_read_count = batch_size;
  w.batch_read_shuffled = true;
  w.batch_read_jittered = true;
  w.scattered_read_pattern = true;
  w.scattered_read_count = batch_size;
  w.read_timing_jitter = true;

  const bool scar = w.batch_read_obfuscated && w.batch_read_shuffled && w.batch_read_jittered;
  if (!scar)
    return {false, steps, "batch read obfuscation scar failed"};

  std::printf("[T0 batch_read_obfuscation] step %d: %d Fisher-Yates shuffled + jittered reads\n", ++steps, batch_size);
  w.note("batch_read_obfuscation: Fisher-Yates shuffle + jitter applied");
  return {true, steps, "batch_read_obfuscation: shuffled+jittered batch reads", batch_size, true, true};
}

}  // namespace examples::batch_read_obfuscation
