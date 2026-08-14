#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `account_graph`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::account_graph::apply(w);
  const auto blue = examples::account_graph::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_23_account_graph() {
  return {{ "23_account_graph", "account graph", Family::Detection, "all",
           "Red multi-step lab path for account_graph",
           "Blue multi-reason lab path for account_graph"},
          run};
}
}  // namespace strategies
