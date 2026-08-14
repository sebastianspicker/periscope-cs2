#include "red_example.hpp"
#include <cstdio>

namespace examples::ept_memory_hiding {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto hv = w.spawn("ept-hv.sys");
  if (!w.proc(hv))
    return {false, steps, "hypervisor spawn failed"};

  w.ept_memory_hiding_active = true;
  w.ept_hidden_page_count = 64;
  w.ept_invept_used = true;
  w.trust.ept_hide_ac_pages = true;
  w.ept_sidechannel_active = true;
  w.invept_tlb_flush_used = true;

  const bool scar = w.ept_memory_hiding_active && w.ept_hidden_page_count >= 64 && w.ept_invept_used;
  if (!scar)
    return {false, steps, "EPT memory hiding scar failed"};

  std::printf("[T3 ept_memory_hiding] step %d: %d pages hidden via EPT, INVEPT used\n", ++steps, w.ept_hidden_page_count);
  w.note("ept_memory_hiding: EPT-based physical memory hiding");
  return {true, steps, "ept_memory_hiding: " + std::to_string(w.ept_hidden_page_count) + " pages hidden", w.ept_hidden_page_count, true};
}

}  // namespace examples::ept_memory_hiding
