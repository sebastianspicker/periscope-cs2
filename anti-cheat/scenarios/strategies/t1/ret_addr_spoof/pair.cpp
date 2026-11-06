// ret_addr_spoof — multi-step open/read + spoofed chain; multi-reason blue walk.

#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red,
        "Multi-step: attach/read with clean thread start but dirty return chain.");
  strategy::t1_ret_addr_spoof::Red red;
  red.apply(w);
  // Capture before blue mitigate clears the chain residual.
  const bool red_ok = w.thread_start_clean && !w.return_address_chain_clean &&
                      w.thread_return_chain.size() >= 3;

  n.say(sim::Side::Blue, "Multi-reason: walk return chain + start/chain consistency + handle.");
  strategy::t1_ret_addr_spoof::Blue blue;
  auto det = blue.detect(w);
  auto mit = blue.mitigate(w);

  StrategyResult r;
  r.red_achieved = red_ok;
  r.blue_detected = det.detected || det.signals >= 2;
  r.blue_mitigated = mit.mitigated || mit.detected;
  r.summary = det.detail.empty()
                  ? ("suspicious_frames=" + std::to_string(det.suspicious_frames) +
                     " signals=" + std::to_string(det.signals))
                  : det.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_121_ret_addr_spoof() {
  return {{"121_ret_addr_spoof", "Return Address Chain Spoofing",
           Family::Evasion, "T1",
           "Multi-step open/read with clean start + spoofed return chain",
           "Multi-reason RtlWalkFrameChain residual sensors"},
          run};
}

}  // namespace strategies
