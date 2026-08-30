#include "red_example.hpp"

#include <cstdio>

namespace examples::stack_spoof {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  std::printf("[red:stack_spoof] step 1: validate game arena\n");
  if (game == 0 || target == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:stack_spoof] step 2: spawn soft reader\n");
  const auto actor = w.spawn("stack_spoof-reader.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    r.detail = "reader spawn failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:stack_spoof] step 3: open game with syscall-path handle\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, true)) {
    r.detail = "syscall-path OpenProcess failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:stack_spoof] step 4: plant stack_spoof_on_read scar\n");
  w.stack_spoof_on_read = true;
  if (!w.stack_spoof_on_read) {
    r.detail = "stack spoof scar failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:stack_spoof] step 5: read entities under spoofed stack walk\n");
  const auto rr = w.read_mem(actor, game, target->base, 4, true);
  if (rr.status != ac::Status::Ok) {
    r.detail = "entity read failed";
    return r;
  }
  ++r.steps;

  bool syscall_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (h.owner_pid == actor && h.via_syscall_path) {
      syscall_handle = true;
      break;
    }
  }
  if (!syscall_handle || !w.stack_spoof_on_read) {
    r.detail = "post-condition: need syscall handle and stack_spoof_on_read";
    return r;
  }

  r.achieved = true;
  r.detail = "syscall open/read with stack_spoof_on_read planted";
  w.note(r.detail);
  std::printf("[red:stack_spoof] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::stack_spoof
