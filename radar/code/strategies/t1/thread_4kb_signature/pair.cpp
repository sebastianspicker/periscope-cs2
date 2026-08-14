// thread_4kb_signature — multi-step clean 4KB + chain residual; multi-reason blue.

#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red,
        "Multi-step: attach/read with clean 4KB RX prologue in legit module.");
  strategy::t1_thread_4kb_signature::Red red;
  red.apply(w);
  const bool red_ok = w.thread_start_in_legitimate_module &&
                      w.thread_start_4kb.size() == 4096 &&
                      w.thread_memory_protection == "PAGE_EXECUTE_READ" &&
                      !w.return_address_chain_clean;

  n.say(sim::Side::Blue,
        "Multi-reason: 4KB pattern + protection + return-chain residual + handle.");
  strategy::t1_thread_4kb_signature::Blue blue;
  auto det = blue.detect(w);
  auto mit = blue.mitigate(w);

  StrategyResult r;
  r.red_achieved = red_ok;
  r.blue_detected = det.detected || det.signals >= 2;
  r.blue_mitigated = mit.mitigated || mit.detected;
  r.summary = det.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_117_thread_4kb_signature() {
  return {{"117_thread_4kb_signature", "Thread 4KB Signature Buffer Evasion",
           Family::Evasion, "T1",
           "Multi-step clean 4KB RX prologue + return-chain residual",
           "Multi-reason 4KB/protection/chain/handle sensors"},
          run};
}

}  // namespace strategies
