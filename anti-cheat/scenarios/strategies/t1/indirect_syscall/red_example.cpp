// RED example implementation for this strategy pair.
// Multi-step World scars for learners; pairs with blue_example detect/mitigate.

#include "red_example.hpp"

#include "t1_red/syscall_cheat.hpp"

#include <cstdio>

namespace examples::indirect_syscall {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game = w.game_pid();
  std::printf("[red:indirect_syscall] verify a game is present\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, steps, "precondition failed: game process missing"};
  }
  ++steps;

  std::printf("[red:indirect_syscall] stage + syscall attach + entity pull\n");
  t1_red::SyscallCheat client(w);
  // Staging on; stack/etw evasions off — pure syscall-path scar for the duel.
  auto rep = client.run_full_loop(true, false);
  ++steps;

  if (!rep.attached) {
    return {false, steps, "syscall attach failed"};
  }
  ++steps;

  if (!rep.via_syscall) {
    return {false, steps, "via_syscall_path missing"};
  }
  ++steps;

  if (rep.entity_count <= 0) {
    return {false, steps, "entity pull empty"};
  }
  ++steps;

  RedResult r{true, steps,
              "indirect syscall path attached and entities pulled"};
  w.note(r.detail);
  std::printf("[red:indirect_syscall] %s (handles=%d entities=%d)\n",
              r.detail.c_str(), rep.syscall_handles, rep.entity_count);
  return r;
}

}  // namespace examples::indirect_syscall
