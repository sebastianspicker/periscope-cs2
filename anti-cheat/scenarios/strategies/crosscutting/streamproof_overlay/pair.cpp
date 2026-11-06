#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "streamproof_overlay",
         "Band-4 stream-proof overlay; capture path misses pixels.");
  const auto red = examples::streamproof_overlay::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "present vs capture",
            "Overlay on present path, empty capture path residual.");
  const auto blue = examples::streamproof_overlay::detect(w);
  n.say(sim::Side::Blue, blue.detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_39_streamproof_overlay() {
  return {{"39_streamproof_overlay", "streamproof overlay", Family::Feature, "T0",
           "Red: stream-proof/band-4 overlay invisible to capture path",
           "Blue: present vs capture path mismatch residual"},
          run};
}
}  // namespace strategies
