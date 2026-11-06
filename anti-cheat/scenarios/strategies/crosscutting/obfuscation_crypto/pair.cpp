#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::obfuscation_crypto::apply(w);
  const auto blue = examples::obfuscation_crypto::detect(w);
  StrategyResult r{red.achieved, blue.detected, blue.mitigated,
                   red.detail + " | " + blue.detail};
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}  // namespace

StrategyEntry entry_106_obfuscation_crypto() {
  return {{"106_obfuscation_crypto", "obfuscation crypto", Family::Evasion, "all",
           "Record encrypted offset and stream-key metadata in the simulator",
           "Correlate crypto metadata and offset-cache artifacts"}, run};
}
}  // namespace strategies
