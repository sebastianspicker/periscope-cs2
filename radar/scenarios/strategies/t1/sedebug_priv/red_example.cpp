#include "red_example.hpp"

#include <cstdio>

namespace examples::sedebug_priv {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  std::printf("[red:sedebug_priv] step 1: validate game arena\n");
  if (game == 0 || w.proc(game) == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:sedebug_priv] step 2: spawn privileged lab actor\n");
  const auto actor = w.spawn("sedebug_priv-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    r.detail = "actor spawn failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:sedebug_priv] step 3: enable SeDebugPrivilege scar\n");
  w.sedebug_privilege = true;
  if (!w.sedebug_privilege) {
    r.detail = "sedebug privilege scar failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:sedebug_priv] step 4: open game handle under elevated privilege\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, false)) {
    r.detail = "OpenProcess under SeDebug failed";
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
  if (!w.sedebug_privilege || !has_handle) {
    r.detail = "post-condition: need SeDebug + game handle";
    return r;
  }

  r.achieved = true;
  r.detail = "SeDebugPrivilege enabled and game VM_READ handle opened";
  w.note(r.detail);
  std::printf("[red:sedebug_priv] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::sedebug_priv
