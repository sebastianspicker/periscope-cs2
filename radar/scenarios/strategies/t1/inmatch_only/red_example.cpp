#include "red_example.hpp"

#include <cstdio>

namespace examples::inmatch_only {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  std::printf("[red:inmatch_only] step 1: validate game arena\n");
  if (game == 0 || target == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:inmatch_only] step 2: spawn reader (lobby-dark path)\n");
  const auto actor = w.spawn("inmatch_only-reader.exe");
  auto* actor_proc = w.proc(actor);
  if (actor == 0 || actor_proc == nullptr) {
    r.detail = "reader spawn failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:inmatch_only] step 3: wait for match window then activate\n");
  if (!w.match_active) w.match_active = true;
  if (!w.match_active) {
    r.detail = "match_active could not be set";
    return r;
  }
  ++r.steps;

  std::printf("[red:inmatch_only] step 4: open VM_READ only while match is live\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, false)) {
    r.detail = "in-match OpenProcess failed";
    return r;
  }
  actor_proc = w.proc(actor);
  if (actor_proc) actor_proc->reader_active = true;
  ++r.steps;

  std::printf("[red:inmatch_only] step 5: pull entities during match\n");
  const auto rr = w.read_mem(actor, game, target->base, 4, true);
  if (rr.status != ac::Status::Ok) {
    r.detail = "in-match entity read failed";
    return r;
  }
  ++r.steps;

  bool has_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (h.owner_pid == actor && sim::has(h.access, sim::AccessMask::VmRead)) {
      has_handle = true;
      break;
    }
  }
  actor_proc = w.proc(actor);
  const bool active_flag = actor_proc && actor_proc->reader_active;
  if (!(has_handle && active_flag && w.match_active)) {
    r.detail = "post-condition: handle + reader_active + match_active required";
    return r;
  }

  r.achieved = true;
  r.detail = "in-match-only reader active with VM_READ during match";
  w.note(r.detail);
  std::printf("[red:inmatch_only] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::inmatch_only
