#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "50_desktop_dup_capture", "Red: stream-proof present path; Blue: DXGI desktop-dup as capture sensor");
  const auto red = examples::desktop_dup_capture::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "50_desktop_dup_capture", "Blue: capture-sensor present-vs-capture mismatch deeper than streamproof 39");
  const auto blue = examples::desktop_dup_capture::detect(w);
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

StrategyEntry entry_50_desktop_dup_capture() {
  return {{ "50_desktop_dup_capture", "desktop dup capture", Family::Feature, "T4",
           "Red: stream-proof present path; Blue: DXGI desktop-dup as capture sensor",
           "Blue: capture-sensor present-vs-capture mismatch deeper than streamproof 39"},
          run};
}
}  // namespace strategies
