#include "blue_example.hpp"

bool thread_monitor_blue_detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue,
        "Multi-reason: trampoline start + obfuscation + dirty return chain.");
  const auto& thread = w.diagnostic_state.thread_capture;

  int signals = 0;
  const bool system_trampoline =
      thread.module_name == "kernel32.dll" || thread.module_name == "ntdll.dll";
  if (system_trampoline) {
    ++signals;
    n.say(sim::Side::Blue, "reason: start attributed to system trampoline module");
  }
  if (w.diagnostic_state.thread_start_obfuscated) {
    ++signals;
    n.say(sim::Side::Blue, "reason: thread start obfuscation residual");
  }
  if (!w.return_address_chain_clean) {
    ++signals;
    n.say(sim::Side::Blue, "reason: return-address chain dirty residual");
  }
  int unmapped = 0;
  for (const auto addr : w.thread_return_chain) {
    bool in_mod = false;
    for (const auto& [pid, proc] : w.processes) {
      (void)pid;
      for (const auto& mod : proc.modules) {
        if (addr >= mod.base && addr < mod.base + mod.size) {
          in_mod = true;
          break;
        }
      }
      if (in_mod) break;
    }
    if (!in_mod) ++unmapped;
  }
  if (unmapped > 0) {
    ++signals;
    n.say(sim::Side::Blue, "reason: unmapped return frames present");
  }
  if (thread.memory_protection == 0x40 || thread.from_rwx_memory) {
    ++signals;
    n.say(sim::Side::Blue, "reason: RWX thread start residual");
  }

  const bool detected = signals >= 2;
  n.say(sim::Side::Blue, detected ? "Thread start multi-reason residual fired."
                                  : "Thread start passes multi-reason threshold.");
  return detected;
}
