#include "red_example.hpp"

namespace examples::polymorphic_build {

RedResult apply(sim::World& w) {
  RedResult r;

  // Phase 1: spawn per-build layout lab actor.
  r.actor_pid = w.spawn("layout-variant-lab.exe");
  ++r.steps;

  // Phase 2: plant polymorphic build identity (not shared baseline).
  w.binary_build_id = "layout-2026-07-25-a7";
  ++r.steps;

  // Phase 3: distribution watermark cohort (lineage, not file hash alone).
  w.build_watermark = "poly:layout-a7:buyer-cohort";
  ++r.steps;

  // Phase 4: module text-hash layout marker + shared section residual.
  if (auto* actor = w.proc(r.actor_pid)) {
    actor->modules.push_back({"variant-code.bin", 0x140000000, 0x20000, true, false,
                              "layout-a7-import-order-3", false, false, false});
  }
  w.add_section({"lab.poly.layout-a7", r.actor_pid, r.actor_pid, false});
  ++r.steps;

  r.achieved = w.binary_build_id != "shared" && !w.build_watermark.empty() &&
               r.steps >= 2;
  r.detail = "polymorphic_build red steps=" + std::to_string(r.steps) +
             " build_id=" + w.binary_build_id +
             " watermark=" + w.build_watermark;
  w.note(r.detail);
  return r;
}

}  // namespace examples::polymorphic_build
