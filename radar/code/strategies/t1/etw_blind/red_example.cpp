#include "red_example.hpp"

#include <cstdio>

namespace examples::etw_blind {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  std::printf("[red:etw_blind] step 1: validate simulated game arena\n");
  if (game == 0 || target == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:etw_blind] step 2: spawn reader actor\n");
  const auto actor = w.spawn("etw_blind-reader.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    r.detail = "reader actor spawn failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:etw_blind] step 3: disable ETW / TI telemetry pipeline\n");
  w.etw_enabled = false;
  w.etw_ti_blind = true;
  if (w.etw_enabled || !w.etw_ti_blind) {
    r.detail = "ETW blind scars failed to plant";
    return r;
  }
  ++r.steps;

  std::printf("[red:etw_blind] step 4: open game via syscall-path OpenProcess\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, true)) {
    r.detail = "syscall-path VM_READ open failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:etw_blind] step 5: read entities while ETW consumers are dark\n");
  const auto rr = w.read_mem(actor, game, target->base, 4, true);
  if (rr.status != ac::Status::Ok) {
    r.detail = "entity read failed under ETW blind";
    return r;
  }
  ++r.steps;

  bool syscall_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (h.owner_pid == actor && h.via_syscall_path &&
        sim::has(h.access, sim::AccessMask::VmRead)) {
      syscall_handle = true;
      break;
    }
  }
  if (!syscall_handle || w.etw_enabled) {
    r.detail = "post-condition failed: need syscall handle and ETW dark";
    return r;
  }

  r.achieved = true;
  r.detail = "ETW pipeline blinded; syscall open/read completed with entities";
  w.note(r.detail);
  std::printf("[red:etw_blind] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::etw_blind
