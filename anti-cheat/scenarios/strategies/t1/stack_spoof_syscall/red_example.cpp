#include "red_example.hpp"
#include <cstdio>

namespace examples::stack_spoof_syscall {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto spoof = w.spawn("stack-spoof.exe");
  if (!w.proc(spoof))
    return {false, steps, "spoof spawn failed"};

  w.stack_spoof_syscall_active = true;
  w.stack_spoof_call_depth = 5;
  w.stack_spoof_ret_addr_forged = true;
  w.stack_spoof_on_read = true;
  w.enhanced_stack_spoof = true;
  w.spoofed_call_depth = 5;

  const bool scar = w.stack_spoof_syscall_active && w.stack_spoof_ret_addr_forged;
  if (!scar)
    return {false, steps, "stack spoof syscall scar failed"};

  std::printf("[T1 stack_spoof_syscall] step %d: call depth %d, return address forged\n", ++steps, w.stack_spoof_call_depth);
  w.note("stack_spoof_syscall: return address spoofing for syscalls");
  return {true, steps, "stack_spoof_syscall: ret addr spoofing depth " + std::to_string(w.stack_spoof_call_depth), w.stack_spoof_call_depth, true};
}

}  // namespace examples::stack_spoof_syscall
