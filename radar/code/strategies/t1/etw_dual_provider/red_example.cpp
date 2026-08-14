#include "red_example.hpp"

#include <cstdio>

namespace strategy::t1_etw_dual_provider {

void Red::apply(sim::World& w) noexcept {
  std::printf("[red:etw_dual_provider] step 1: validate game arena\n");
  const auto game = w.game_pid();
  if (game == 0 || w.proc(game) == nullptr) {
    w.note("etw_dual_provider: precondition failed — game missing");
    return;
  }

  std::printf("[red:etw_dual_provider] step 2: spawn lab reader under TI blind path\n");
  const auto actor = w.spawn("etw_dual-reader.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    w.note("etw_dual_provider: actor spawn failed");
    return;
  }

  std::printf("[red:etw_dual_provider] step 3: blind primary ETW Threat Intelligence\n");
  w.etw_enabled = false;
  w.etw_ti_blind = true;
  // Red believes secondary is also dark / unknown
  w.etw_secondary_active = false;
  if (!w.etw_ti_blind || w.etw_enabled) {
    w.note("etw_dual_provider: TI blind scars failed");
    return;
  }

  std::printf("[red:etw_dual_provider] step 4: open+read game while primary TI is dark\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, true)) {
    w.note("etw_dual_provider: syscall open failed");
    return;
  }
  const auto* target = w.proc(game);
  if (target == nullptr ||
      w.read_mem(actor, game, target->base, 4, true).status != ac::Status::Ok) {
    w.note("etw_dual_provider: entity read failed");
    return;
  }

  std::printf("[red:etw_dual_provider] step 5: verify primary TI residual + attach\n");
  bool syscall_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (h.owner_pid == actor && h.via_syscall_path) {
      syscall_handle = true;
      break;
    }
  }
  if (!(w.etw_ti_blind && !w.etw_enabled && syscall_handle)) {
    w.note("etw_dual_provider: post-condition failed");
    return;
  }

  w.note("etw_dual_provider: primary ETW TI blinded; open/read completed (secondary unknown to red)");
  std::printf("[red:etw_dual_provider] achieved: etw_ti_blind=1 secondary_active=%d handle=1\n",
              w.etw_secondary_active ? 1 : 0);
}

}  // namespace strategy::t1_etw_dual_provider
