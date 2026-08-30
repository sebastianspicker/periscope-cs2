#include "red_example.hpp"
#include <cstdio>

namespace examples::page_oscillation_mem {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto oscillator = w.spawn("oscillator.exe");
  if (!w.proc(oscillator))
    return {false, steps, "oscillator spawn failed"};

  w.page_oscillation_active = true;
  w.page_oscillation_cycles = 8;
  w.code_page_flipped_to_rw = true;
  w.code_page_flipped_to_rx = true;

  const bool scar = w.page_oscillation_active && w.page_oscillation_cycles >= 4;
  if (!scar)
    return {false, steps, "page oscillation scar failed"};

  std::printf("[T1 page_oscillation_mem] step %d: %d oscillation cycles, RW<->RX flips\n", ++steps, w.page_oscillation_cycles);
  w.note("page_oscillation_mem: code page oscillation for VAS walk evasion");
  return {true, steps, "page_oscillation_mem: " + std::to_string(w.page_oscillation_cycles) + " cycles", w.page_oscillation_cycles, true, true};
}

}  // namespace examples::page_oscillation_mem
