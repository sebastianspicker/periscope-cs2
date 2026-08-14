#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::iommu_policy {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:iommu_policy] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:iommu_policy] signal 2: evaluate strategy-specific posture\n");
  if (w.trust.dma_device_present) {
    sim::strategy_example::add_signal(outcome, "DMA-capable device is present", 0.45);
  }
  std::printf("[blue:iommu_policy] signal 3: corroborate independent residual\n");
  if (!w.trust.iommu_on) {
    sim::strategy_example::add_signal(outcome, "IOMMU policy is disabled", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("iommu_policy", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) {
    w.ranked_access_denied = true;
    w.trust.iommu_on = true;
    w.trust.ranked_requires_iommu = true;
  }
  r.detail = "iommu_policy blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

}  // namespace examples::iommu_policy
