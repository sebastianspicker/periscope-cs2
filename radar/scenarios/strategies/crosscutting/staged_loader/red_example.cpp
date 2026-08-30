#include "red_example.hpp"

namespace examples::staged_loader {

RedResult apply(sim::World& w) {
  RedResult r;

  // Phase 1: stage-one dropper process (no payload yet).
  r.actor_pid = w.spawn("stage-one-lab.exe");
  ++r.steps;

  // Phase 2: simulated stage-two fetch metadata (lab-only, no real network).
  w.add_net({r.actor_pid, "lab.invalid:443", true, false});
  ++r.steps;

  // Phase 3: encrypted stage-two section residual in memory.
  w.add_section({"lab.stage-two.encrypted", r.actor_pid, r.actor_pid, false});
  ++r.steps;

  // Phase 4: memory-only transition (manual-map region, no PE disk image).
  if (auto* actor = w.proc(r.actor_pid)) {
    actor->manual_mapped_region = true;
    actor->modules.push_back({"stage-two.bin", 0, 0x10000, false, true,
                              "stage-payload-mapped", false, false, false});
  }
  w.mapper_process_present = true;
  ++r.steps;

  r.achieved = r.steps >= 2 && !w.net.empty() && !w.sections.empty();
  r.detail = "staged_loader red steps=" + std::to_string(r.steps) +
             " stage metadata + encrypted section + manual-map recorded";
  w.note(r.detail);
  return r;
}

}  // namespace examples::staged_loader
