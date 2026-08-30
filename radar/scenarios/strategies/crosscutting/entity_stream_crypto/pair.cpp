#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `entity_stream_crypto`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::entity_stream_crypto::apply(w);
  const auto blue = examples::entity_stream_crypto::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_40_entity_stream_crypto() {
  return {{ "40_entity_stream_crypto", "entity stream crypto", Family::Structural, "all",
           "Red multi-step lab path for entity_stream_crypto",
           "Blue multi-reason lab path for entity_stream_crypto"},
          run};
}
}  // namespace strategies
