#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `packet_loss_disambig`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::packet_loss_disambig::apply(w);
  const auto blue = examples::packet_loss_disambig::detect(w);

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

StrategyEntry entry_96_packet_loss_disambig() {
  return {{ "96_packet_loss_disambig", "packet loss disambig", Family::Delivery, "T4",
           "Red multi-step lab path for packet_loss_disambig",
           "Blue multi-reason lab path for packet_loss_disambig"},
          run};
}
}  // namespace strategies
