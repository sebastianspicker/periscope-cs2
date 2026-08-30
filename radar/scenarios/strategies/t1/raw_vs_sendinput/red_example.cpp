#include "red_example.hpp"

#include <cstdio>

namespace examples::raw_vs_sendinput {

RedResult apply(sim::World& w) {
  RedResult r{};
  std::printf("[red:raw_vs_sendinput] step 1: validate game arena\n");
  if (w.game_pid() == 0 || w.proc(w.game_pid()) == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:raw_vs_sendinput] step 2: mark mixed input provenance scar\n");
  w.raw_sendinput_mixed = true;
  if (!w.raw_sendinput_mixed) {
    r.detail = "raw_sendinput_mixed scar failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:raw_vs_sendinput] step 3: push raw HID input event\n");
  w.push_input({0.0, "raw_hid", 1, 0});
  ++r.steps;

  std::printf("[red:raw_vs_sendinput] step 4: push injected SendInput-style event\n");
  w.push_input({0.0, "injected", 1, 0});
  ++r.steps;

  int raw_count = 0;
  int injected_count = 0;
  for (const auto& e : w.inputs) {
    if (e.source == "raw_hid") ++raw_count;
    if (e.source == "injected") ++injected_count;
  }
  if (!w.raw_sendinput_mixed || raw_count <= 0 || injected_count <= 0) {
    r.detail = "post-condition: need flag + raw + injected events";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "raw_hid + injected mix planted (raw=" + std::to_string(raw_count) +
             " injected=" + std::to_string(injected_count) + ")";
  w.note(r.detail);
  std::printf("[red:raw_vs_sendinput] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::raw_vs_sendinput
