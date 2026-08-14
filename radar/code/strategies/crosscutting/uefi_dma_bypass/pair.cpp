#include "strategies/pair_util.hpp"
#include "red_example.hpp"
#include "blue_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  examples::uefi_dma_bypass::Red red;
  red.apply(w);
  const bool red_ok = w.iommu_bypass_active && !w.trust.iommu_on &&
                      w.trust.dma_device_present;

  examples::uefi_dma_bypass::Blue blue;
  const auto blue_result = blue.detect(w);
  const auto mitigation =
      blue_result.detected ? blue.mitigate(w) : blue_result;

  StrategyResult r;
  r.red_achieved = red_ok;
  r.blue_detected = blue_result.detected;
  r.blue_mitigated = mitigation.mitigated;
  r.summary = "uefi_dma_bypass: signals=" +
              std::to_string(blue_result.signals) +
              " detected=" + std::to_string(r.blue_detected) +
              " mitigated=" + std::to_string(r.blue_mitigated);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_151_uefi_dma_bypass() {
  return {{"151_uefi_dma_bypass", "UEFI DMA Remapping Bypass",
           Family::Evasion, "crosscutting",
           "Bypass DMA remapping (IOMMU/VT-d) via UEFI DMAR table analysis, "
           "ACS clearing, and IOMMU register manipulation",
           "Detect IOMMU bypass via DMAR integrity checks, ACS polling, "
           "and PCIe traffic anomaly analysis"},
          run};
}

}  // namespace strategies
