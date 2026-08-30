#include "red_example.hpp"

#include <cstdio>

namespace examples::parent_lineage {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  std::printf("[red:parent_lineage] step 1: validate game arena\n");
  if (game == 0 || target == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:parent_lineage] step 2: spawn trusted-looking shell parent\n");
  const auto shell = w.spawn("explorer.exe");
  if (shell == 0 || w.proc(shell) == nullptr) {
    r.detail = "shell parent spawn failed";
    return r;
  }
  if (auto* s = w.proc(shell)) s->looks_reputable = true;
  ++r.steps;

  std::printf("[red:parent_lineage] step 3: spawn radar under reputable parent lineage\n");
  const auto actor = w.spawn("parent_lineage-radar.exe");
  auto* actor_proc = w.proc(actor);
  if (actor == 0 || actor_proc == nullptr) {
    r.detail = "radar actor spawn failed";
    return r;
  }
  actor_proc->parent_pid = shell;
  actor_proc->looks_reputable = true;
  // Also plant implausible game-parent edge for lineage sensor (LESSON path B)
  // Prefer shell parent as primary scar; game parent is optional alternate.
  ++r.steps;

  std::printf("[red:parent_lineage] step 4: syscall OpenProcess to game\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, true)) {
    r.detail = "syscall OpenProcess failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:parent_lineage] step 5: read entities under spoofed lineage\n");
  const auto rr = w.read_mem(actor, game, target->base, 4, true);
  if (rr.status != ac::Status::Ok) {
    r.detail = "entity read failed";
    return r;
  }
  ++r.steps;

  actor_proc = w.proc(actor);
  bool syscall_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (h.owner_pid == actor && h.via_syscall_path) {
      syscall_handle = true;
      break;
    }
  }
  const bool parent_ok =
      actor_proc && actor_proc->looks_reputable && actor_proc->parent_pid == shell;
  if (!syscall_handle || !parent_ok) {
    r.detail = "post-condition: syscall handle + reputable parent required";
    return r;
  }

  r.achieved = true;
  r.detail = "reputable parent lineage + syscall VM_READ attach completed";
  w.note(r.detail);
  std::printf("[red:parent_lineage] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::parent_lineage
