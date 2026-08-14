#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "53_multimap_radar_share", "Red: multi-map radar share UX + share token (beyond plain web radar 10)");
  const auto red = examples::multimap_radar_share::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "53_multimap_radar_share", "Blue: multi-map share residual + SaaS net multi-reason");
  const auto blue = examples::multimap_radar_share::detect(w);
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

StrategyEntry entry_53_multimap_radar_share() {
  return {{ "53_multimap_radar_share", "multimap radar share", Family::Feature, "all",
           "Red: multi-map radar share UX + share token (beyond plain web radar 10)",
           "Blue: multi-map share residual + SaaS net multi-reason"},
          run};
}
}  // namespace strategies
