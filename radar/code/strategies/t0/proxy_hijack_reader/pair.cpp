#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::proxy_hijack_reader::apply(w, n);
  const bool blue_detected = examples::proxy_hijack_reader::detect(w, n);

  // Multi-reason residual: even if handle-only scan misses attribution,
  // co-occurrence (proxy + remote reads / foreign edges) is still scored by detect.
  // Drive shipped detect path (no constant-true assignment).
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue_detected;
  result.blue_mitigated = blue_detected;
  result.summary = blue_detected
                       ? "Handle/proxy sensors found VM_READ or co-occurrence residual"
                       : "No proxy/handle residual observed on World";
  n.result(result.blue_detected, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_114_proxy_hijack_reader() {
  return {{"114_proxy_hijack_reader", "Handle Graph Proxy/Hijack Reader", Family::Evasion,
           "T0", "Relay game reads through a proxy over IPC",
           "Drive real detect on handle/proxy co-occurrence residual"}, run};
}

}  // namespace strategies
