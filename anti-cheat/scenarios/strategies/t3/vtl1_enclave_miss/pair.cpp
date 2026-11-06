#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `vtl1_enclave_miss`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::vtl1_enclave_miss::run_red(w, n);
  const auto blue = examples::vtl1_enclave_miss::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.65;
  r.summary = red.detail + " | " + blue.detail + " | signals=" +
              std::to_string(blue.signals) + " risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_95_vtl1_enclave_miss() {
  return {{ "95_vtl1_enclave_miss", "vtl1 enclave miss", Family::Delivery, "T3",
           "Red multi-step lab path for vtl1_enclave_miss",
           "Blue multi-reason lab path for vtl1_enclave_miss"},
          run};
}
}  // namespace strategies
