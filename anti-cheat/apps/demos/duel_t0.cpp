// T0 duel: full red external RPM radar vs full blue AcAgent on sim::World.

#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include "ac/telemetry.hpp"
#include "t0_red/cheat_client.hpp"
#include "t0_red/evasion_weak.hpp"
#include "t0_blue/ac_agent.hpp"

#include <cstdio>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "T0 duel — usermode VM_READ external radar vs handle-graph AC.");

  auto w = sim::make_arena("lab-game.exe");
  // Ensure AC process exists for self-skip in scans.
  if (w.ac_pid() == 0) {
    w.spawn("ac-agent.exe", false, true);
  }

  n.move(sim::Side::Red, "External RPM radar",
         "Spawn radar.exe, OpenProcess(VM_READ), read entity table, draw overlay.");
  t0_red::CheatClient red(w, "radar.exe");
  auto rep = red.run_full_loop(false, true);
  n.say(sim::Side::Red, rep.detail);
  std::printf("[red] entities=%d blips=%d handles=%d reads=%d\n",
              rep.entity_count, rep.blip_count, rep.foreign_vm_read_handles,
              rep.read_ops);

  n.move(sim::Side::Red, "Weak evasions",
         "Rename, hide from weak enum, throttle, hide handle, claim rep.");
  auto evade = t0_red::WeakEvasionKit::apply_common_stack(w, red.pid(),
                                                           red.game_pid());
  for (const auto& e : evade) {
    n.say(sim::Side::Red, e.attempt + " → fails: " + e.blue_still_sees);
  }

  n.counter(sim::Side::Blue, "AcAgent full_scan",
            "Multi-sample handles + co-run + overlay + integrity.");
  ac::MemoryTelemetrySink sink;
  t0_blue::AcAgent blue(w, sink);
  auto d1 = blue.scan_handles();
  auto d2 = blue.full_scan();
  for (const auto& r : d2.reasons) {
    n.say(sim::Side::Blue, r);
  }
  std::printf("[blue] %s risk=%.1f\n", d2.summary.c_str(), d2.risk);

  // After rename, co-occurrence may miss name but handle graph must still hit.
  const bool blue_wins = d2.handle_hit && d2.risk > 0 && !d2.reasons.empty();
  n.result(blue_wins,
           blue_wins
               ? "T0 closed by handle graph / multi-sensor agent. Red must leave "
                 "usermode handles → escalate T2/T3."
               : "Unexpected: blue missed VM_READ scar (lesson bug).");

  n.say(sim::Side::Lesson,
        "Next: T1 syscalls dodge API hooks — handles remain. Run duel_t1.");
  return blue_wins ? 0 : 1;
}
