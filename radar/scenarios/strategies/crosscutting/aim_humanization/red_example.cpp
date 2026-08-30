#include "red_example.hpp"

#include <cstdio>

namespace examples::aim_humanization {

RedResult apply(sim::World& w) {
  RedResult r;
  int steps = 0;

  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    r.detail = "precondition failed: game process unavailable";
    return r;
  }
  ++steps;

  r.actor_pid = w.spawn("soft-aim.exe");
  auto* actor = w.proc(r.actor_pid);
  if (!actor) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 1: soft humanized deltas (not snap) — injected provenance residual.
  const std::size_t before = w.inputs.size();
  for (int i = 0; i < 10; ++i) {
    w.push_input(sim::InputEvent{0.05 * i, "injected", 3.f + (i % 3), 2.f});
  }
  const bool soft_inputs = (w.inputs.size() - before) >= 8;
  if (!soft_inputs) {
    r.detail = "soft humanized input plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: residual radar SaaS channel (soft aim alone is weak; product needs intel).
  w.add_net(sim::NetFlow{r.actor_pid, "radar-saas.example:443", false, true});
  bool saas = false;
  for (const auto& n : w.net) {
    if (n.looks_like_radar_saas && n.pid == r.actor_pid) {
      saas = true;
      break;
    }
  }
  if (!saas) {
    r.detail = "radar SaaS residual plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  r.achieved = soft_inputs && saas;
  r.steps = steps;
  r.detail = "aim_humanization red soft_inputs=" +
             std::to_string(static_cast<int>(w.inputs.size() - before)) +
             " saas=1 steps=" + std::to_string(steps);
  w.note(r.detail);
  std::printf("[aim_humanization] RED achieved=%s steps=%d\n",
              r.achieved ? "true" : "false", steps);
  return r;
}

}  // namespace examples::aim_humanization
