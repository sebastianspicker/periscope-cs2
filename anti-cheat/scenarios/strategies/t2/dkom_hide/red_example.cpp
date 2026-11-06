// RED example implementation for strategy `dkom_hide`.
// Multi-step World scars: hide a process from weak enumeration and verify divergence.

#include "red_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::dkom_hide {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "DKOM precondition failed: game unavailable";

  std::printf("[red:dkom_hide] verify a game process is present\n");
  if (!w.game_pid()) {
    return r;
  }

  std::printf("[red:dkom_hide] spawn a lab process that will be hidden from weak walks\n");
  const auto actor = w.spawn("dkom-hidden.exe");
  if (!w.proc(actor)) {
    r.detail = "spawn of dkom-hidden.exe failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:dkom_hide] mark the process hidden_from_weak_enum\n");
  w.proc(actor)->hidden_from_weak_enum = true;
  if (!w.proc(actor)->hidden_from_weak_enum) {
    r.detail = "failed to set hidden_from_weak_enum scar";
    return r;
  }
  ++r.steps;

  std::printf("[red:dkom_hide] compare weak vs full process inventories\n");
  const auto weak = w.list_processes(true);
  const auto full = w.list_processes(false);
  const bool absent = std::none_of(weak.begin(), weak.end(),
                                   [&](const auto& p) { return p.pid == actor; });
  const bool present = std::any_of(full.begin(), full.end(),
                                   [&](const auto& p) { return p.pid == actor; });
  if (!absent || !present) {
    r.detail = "weak vs full list divergence not verified";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "weak enumeration divergence verified for hidden process";
  w.note(r.detail);
  std::printf("[red:dkom_hide] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "dkom_hide", "Create a process-list discrepancy.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::dkom_hide
