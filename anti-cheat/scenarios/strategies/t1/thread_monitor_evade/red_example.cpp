#include "red_example.hpp"

#include <cstdio>

bool thread_monitor_red_apply(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "step 1: validate game process");
  auto* game = w.proc(w.game_pid());
  if (game == nullptr) return false;

  n.say(sim::Side::Red, "step 2: ensure loaded system module facade (kernel32)");
  game->modules.push_back({"kernel32.dll", game->base + 0x500000, 0x10000,
                           true, false, "clean"});
  const auto& module = game->modules.back();

  n.say(sim::Side::Red, "step 3: create thread with start address inside loaded kernel32");
  auto& thread = w.diagnostic_state.thread_capture;
  thread.thread_id = w.game_pid();
  thread.module_name = module.name;
  thread.start_address = module.base + 0x100;
  thread.memory_protection = 0x20;  // PAGE_EXECUTE_READ
  thread.from_suspicious_module = false;
  thread.from_rwx_memory = false;
  thread.signature = {0x48, 0x89, 0x5C, 0x24};

  n.say(sim::Side::Red, "step 4: mark thread-start obfuscation residual");
  w.diagnostic_state.thread_start_obfuscated = true;
  w.thread_start_in_legitimate_module = true;
  w.thread_start_clean = true;
  w.thread_memory_protection = "PAGE_EXECUTE_READ";

  n.say(sim::Side::Red, "step 5: plant return-chain residual that MonitorThreadContext still sees");
  w.return_address_chain_clean = false;
  w.thread_return_chain.clear();
  w.thread_return_chain.push_back(module.base + 0x200);
  w.thread_return_chain.push_back(0x100000000000ull);  // private RWX / no module

  n.say(sim::Side::Red, "step 6: verify start address resolves into loaded system module");
  bool in_module = thread.start_address >= module.base &&
                   thread.start_address < module.base + module.size;
  if (!(in_module && w.diagnostic_state.thread_start_obfuscated &&
        thread.module_name == "kernel32.dll" && !w.return_address_chain_clean)) {
    return false;
  }

  std::printf("[red:thread_monitor_evade] achieved: start in %s obfuscated=1 chain_dirty=1\n",
              thread.module_name.c_str());
  w.note("thread_monitor_evade: system-DLL trampoline start + obfuscation residual");
  return true;
}
