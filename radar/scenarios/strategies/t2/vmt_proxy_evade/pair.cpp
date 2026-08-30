#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  StrategyResult result;
  result.red_achieved = vmt_proxy_evade_red_apply(w, n);
  result.blue_detected = vmt_proxy_evade_blue_detect(w, n);
  result.blue_mitigated = result.blue_detected;
  result.summary = result.blue_detected
      ? "VMT provenance and singleton-use analysis found the proxy dispatch"
      : "Proxy dispatch was not identified by the simulated VMT inventory";
  n.result(result.blue_detected, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_112_vmt_proxy_evade() {
  return {{"112_vmt_proxy_evade", "VMT Proxy Dispatch Evasion", Family::Evasion, "T2",
           "Route simulated red calls through an isolated proxy VMT",
           "Validate VMT CRC, module provenance, and use counts"}, run};
}
}  // namespace strategies
