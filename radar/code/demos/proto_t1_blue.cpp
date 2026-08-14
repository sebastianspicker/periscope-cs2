// T1 BLUE prototype — detect syscall-path red even when usermode hooks are blind.

#include "ac/proto_log.hpp"
#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "sim/world.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "t1_blue/hook_trap.hpp"
#include "t1_blue/staging_watch.hpp"
#include "t1_blue/syscall_aware_monitor.hpp"
#include "t1_red/syscall_cheat.hpp"

#include <cstdio>

int main() {
  ac::proto_banner("BLUE", "T1", "Syscall-aware handles + staging + T1Agent");

  // Plant real T1 red scar on World.
  auto world = sim::make_arena();
  t1_red::SyscallCheat red(world, "soft-radar.exe");
  auto rep = red.run_full_loop(true, true);
  ac::proto_kv("red_entities", static_cast<std::size_t>(rep.entity_count));
  ac::proto_kv("red_syscall_handles",
               static_cast<std::size_t>(rep.syscall_handles));

  ac::MemoryTelemetrySink sink;
  t1_blue::T1Agent agent(world, sink);
  auto d = agent.full_scan();

  ac::proto_kv("handle_truth_hit", d.handle_truth_hit);
  ac::proto_kv("hooks_blind", d.hooks_blind);
  ac::proto_kv("staging_hit", d.staging_hit);
  ac::proto_kv("syscall_handles", static_cast<std::size_t>(d.syscall_handles));
  ac::proto_kv("hook_visible", static_cast<std::size_t>(d.hook_visible));
  ac::proto_kv("risk_score", d.risk);
  ac::proto_line("summary", d.summary);
  for (const auto& r : d.reasons) {
    std::printf("    reason: %s\n", r.c_str());
  }

  // Edge-list path still works (legacy proto).
  t0_blue::HandleGraphMonitor handles(sink);
  t1_blue::SyscallAwareHandleMonitor mon(handles);
  mon.evaluate(world.game_pid(),
               {{red.pid(), world.game_pid(), t0_blue::ProcessAccess::VmRead,
                 "soft-radar.exe"}},
               /*hooks_saw_rpm=*/false);
  ac::proto_kv("depends_on_ntdll_hooks", mon.detection_depends_on_ntdll_hooks());
  ac::proto_kv("edge_list_hits", mon.last_hit_count());

  ac::proto_events(sink);

  const bool detected =
      d.handle_truth_hit && d.hooks_blind && d.risk > 0 && !d.reasons.empty();
  ac::proto_kv("T1_DETECTED", detected);
  std::puts(detected
                ? "prototype ok (t1 surface caught without API hooks)"
                : "prototype FAIL");
  return detected ? 0 : 1;
}
