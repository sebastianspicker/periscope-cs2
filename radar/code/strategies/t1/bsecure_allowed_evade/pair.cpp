// bsecure_allowed_evade — multi-step stomp path; multi-reason BSecureAllowed sensors.

#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "Multi-step: stomp legitimate module to inherit signature façade.");
  strategy::t1_bsecure_allowed_evade::Red red;
  red.apply(w);
  const bool red_ok =
      w.module_loaded && w.module_is_signed && w.module_is_stomped;

  n.say(sim::Side::Blue, "Multi-reason: stomp + signature façade + host integrity.");
  strategy::t1_bsecure_allowed_evade::Blue blue;
  auto det = blue.detect(w);
  auto mit = blue.mitigate(w);

  StrategyResult r;
  r.red_achieved = red_ok;
  r.blue_detected = det.detected || (det.stomp_detected && det.signals >= 2);
  r.blue_mitigated = mit.mitigated || (mit.detection_count >= 2 && mit.stomp_detected);
  r.summary = det.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_118_bsecure_allowed_evade() {
  return {{"118_bsecure_allowed_evade", "BSecureAllowed Trust Chain Evasion",
           Family::Evasion, "T1-T2",
           "Multi-step module stomp inheriting signature façade",
           "Multi-reason BSecureAllowed stomp/signature/integrity sensors"},
          run};
}

}  // namespace strategies
