#include "red_example.hpp"

#include <cstdio>
#include <cstring>

namespace strategy::t1_thread_4kb_signature {

void Red::apply(sim::World& w) noexcept {
  std::printf("[red:thread_4kb_signature] step 1: validate game arena\n");
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  if (game == 0 || g == nullptr) {
    w.note("thread_4kb_signature: precondition failed — game missing");
    return;
  }

  std::printf("[red:thread_4kb_signature] step 2: spawn worker actor\n");
  const auto actor = w.spawn("thread_4kb-worker.exe");
  auto* ap = w.proc(actor);
  if (actor == 0 || ap == nullptr) {
    w.note("thread_4kb_signature: actor spawn failed");
    return;
  }

  std::printf("[red:thread_4kb_signature] step 3: open game + read (thread residual accompanies attach)\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, true)) {
    w.note("thread_4kb_signature: open_process failed");
    return;
  }
  if (w.read_mem(actor, game, g->base, 4, true).status != ac::Status::Ok) {
    w.note("thread_4kb_signature: read_mem failed");
    return;
  }

  std::printf("[red:thread_4kb_signature] step 4: plant clean 4KB prologue at legit module start\n");
  // Ensure a legitimate module exists for the start address facade
  if (ap->modules.empty()) {
    ap->modules.push_back(
        {"kernel32.dll", 0x7ffa10000000ull, 0x100000, true, false, "clean"});
  }
  w.thread_start_in_legitimate_module = true;
  w.thread_memory_protection = "PAGE_EXECUTE_READ";  // RX, not RWX
  w.thread_start_4kb.assign(4096, 0x90);             // NOP sled
  // Legitimate-looking x64 prologue
  w.thread_start_4kb[0] = 0x48;
  w.thread_start_4kb[1] = 0x89;
  w.thread_start_4kb[2] = 0x5C;
  w.thread_start_4kb[3] = 0x24;
  w.thread_start_4kb[4] = 0x48;
  w.thread_start_4kb[5] = 0x83;
  w.thread_start_4kb[6] = 0xEC;
  w.thread_start_4kb[7] = 0x28;

  std::printf("[red:thread_4kb_signature] step 5: plant return-chain residual (LESSON co-scar)\n");
  // Clean 4KB but return chain still betrays RWX/no-module frames (MonitorThreadContext)
  w.thread_start_clean = true;
  w.return_address_chain_clean = false;
  w.thread_return_chain.clear();
  w.thread_return_chain.push_back(0x7ffa10000100ull);  // inside kernel32 facade
  w.thread_return_chain.push_back(0x100000000000ull);  // red private RWX — no module
  w.thread_return_chain.push_back(0x7ffa10000200ull);

  std::printf("[red:thread_4kb_signature] step 6: verify clean buffer + dirty chain\n");
  const bool clean_buf =
      w.thread_start_4kb.size() == 4096 &&
      w.thread_memory_protection == "PAGE_EXECUTE_READ" &&
      w.thread_start_in_legitimate_module;
  const bool chain_scar =
      !w.return_address_chain_clean && w.thread_return_chain.size() >= 2;
  if (!(clean_buf && chain_scar)) {
    w.note("thread_4kb_signature: post-condition failed");
    return;
  }

  w.note("thread_4kb_signature: clean 4KB RX prologue in legit module; return chain residual remains");
  std::printf("[red:thread_4kb_signature] achieved: clean_4kb=1 rx=1 chain_dirty=1\n");
}

}  // namespace strategy::t1_thread_4kb_signature
