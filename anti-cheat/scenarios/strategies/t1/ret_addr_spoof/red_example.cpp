#include "red_example.hpp"

#include <cstdio>

namespace strategy::t1_ret_addr_spoof {

void Red::apply(sim::World& w) noexcept {
  std::printf("[red:ret_addr_spoof] step 1: validate game arena\n");
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  if (game == 0 || g == nullptr) {
    w.note("ret_addr_spoof: precondition failed — game missing");
    return;
  }

  std::printf("[red:ret_addr_spoof] step 2: spawn reader with clean-looking thread start\n");
  const auto actor = w.spawn("ret_addr_spoof-reader.exe");
  auto* ap = w.proc(actor);
  if (actor == 0 || ap == nullptr) {
    w.note("ret_addr_spoof: actor spawn failed");
    return;
  }

  // Ensure known modules exist so clean frames resolve
  if (ap->modules.empty()) {
    ap->modules.push_back({"ntdll.dll", 0x7ffa87650000ull, 0x100000, true, false, "clean"});
    ap->modules.push_back({"kernel32.dll", 0x7ffa12340000ull, 0x100000, true, false, "clean"});
  }

  std::printf("[red:ret_addr_spoof] step 3: open game via syscall and read entities\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, true)) {
    w.note("ret_addr_spoof: open_process failed");
    return;
  }
  if (w.read_mem(actor, game, g->base, 4, true).status != ac::Status::Ok) {
    w.note("ret_addr_spoof: read_mem failed");
    return;
  }

  std::printf("[red:ret_addr_spoof] step 4: plant clean start + spoofed return chain\n");
  w.thread_start_clean = true;
  w.return_address_chain_clean = false;
  w.thread_return_chain.clear();
  // Frames inside known modules
  w.thread_return_chain.push_back(0x7ffa12345678ull);  // kernel32
  w.thread_return_chain.push_back(0x7ffa87654321ull);  // ntdll
  // Frames in RWX private memory (no module) — red residual
  w.thread_return_chain.push_back(0x100000000000ull);
  w.thread_return_chain.push_back(0x7ffa12345678ull);
  w.thread_return_chain.push_back(0x100000000001ull);

  std::printf("[red:ret_addr_spoof] step 5: verify chain residual\n");
  int suspicious = 0;
  for (const auto addr : w.thread_return_chain) {
    bool in_mod = false;
    for (const auto& [pid, proc] : w.processes) {
      for (const auto& mod : proc.modules) {
        if (addr >= mod.base && addr < mod.base + mod.size) {
          in_mod = true;
          break;
        }
      }
      if (in_mod) break;
    }
    if (!in_mod) ++suspicious;
  }
  if (!(w.thread_start_clean && !w.return_address_chain_clean && suspicious >= 1 &&
        w.thread_return_chain.size() >= 3)) {
    w.note("ret_addr_spoof: post-condition failed");
    return;
  }

  w.note("ret_addr_spoof: clean start address; return chain includes RWX/no-module frames");
  std::printf("[red:ret_addr_spoof] achieved: frames=%zu suspicious=%d start_clean=1\n",
              w.thread_return_chain.size(), suspicious);
}

}  // namespace strategy::t1_ret_addr_spoof
