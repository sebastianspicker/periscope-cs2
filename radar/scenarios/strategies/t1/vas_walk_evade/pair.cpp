#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
    const auto red = examples::vas_walk_evade::apply(w);
    const auto blue = examples::vas_walk_evade::detect(w);
    StrategyResult result;
    result.red_achieved = red.achieved;
    result.blue_detected = blue.detected;
    result.blue_mitigated = blue.mitigated || (blue.detected && blue.signals >= 1);
    result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
    n.result(result.blue_detected || result.blue_mitigated, result.summary);
    return result;
}
}

StrategyEntry entry_119_vas_walk_evade() {
    return {{"119_vas_walk_evade", "VAS walk evade", Family::Evasion, "T1",
        "Red registers VEH handler to hide code pages during VAC VAS walk phase",
        "Blue detects VEH handler anomalies and module header erasure"}, run};
}
}
