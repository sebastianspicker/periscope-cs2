#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `vpn_proxy_graph`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::vpn_proxy_graph::apply(w);
  const auto blue = examples::vpn_proxy_graph::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_86_vpn_proxy_graph() {
  return {{ "86_vpn_proxy_graph", "vpn proxy graph", Family::Detection, "all",
           "Red multi-step lab path for vpn_proxy_graph",
           "Blue multi-reason lab path for vpn_proxy_graph"},
          run};
}
}  // namespace strategies
