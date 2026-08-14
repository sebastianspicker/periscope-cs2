#include "strategies/framework.hpp"
#include "sim/world.hpp"
#include "red_example.hpp"
#include "blue_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  auto red = examples::convars_temp_restore::apply(w, n);
  const bool blue = examples::convars_temp_restore::detect(w, n);

  // Drive shipped detect (no constant-true assignment).
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue;
  r.blue_mitigated = blue;
  r.summary = blue
    ? "Multi-reason: continuous window residual + co-scars after restore"
    : "No multi-reason ConVar modify/restore residual observed";
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_115_convars_temp_restore() {
  return {{"115_convars_temp_restore", "ConVar Temporary Modification + Restoration",
           Family::Evasion, "T2",
           "Temp modify ConVar, perform technique, restore before CRC check",
           "Drive real detect on ConVar modify/restore residual"}, run};
}

}  // namespace strategies
