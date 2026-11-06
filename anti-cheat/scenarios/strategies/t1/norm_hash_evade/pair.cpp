#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
    const auto red = examples::norm_hash_evade::apply(w);
    const auto blue = examples::norm_hash_evade::detect(w);
    StrategyResult result;
    result.red_achieved = red.achieved;
    result.blue_detected = blue.detected;
    result.blue_mitigated = blue.detected && blue.signals >= 2;
    result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
    n.result(result.blue_detected || result.blue_mitigated, result.summary);
    return result;
}
}

StrategyEntry entry_122_norm_hash_evade() {
    return {{"122_norm_hash_evade", "norm hash evade", Family::Evasion, "T1",
        "Red hooks or patches PEFile::NormalizedHash to return cached clean hash",
        "Blue detects IAT hooks and suspiciously clean module hashes"}, run};
}
}
