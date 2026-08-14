#include "blue_example.hpp"

#include <utility>

namespace examples::pcie_peer_dma {
namespace {

void add_signal(BlueResult& result, std::string reason, double weight) {
  ++result.signals;
  result.risk += weight;
  result.reasons.push_back(std::move(reason));
}

}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult result{false, false, 0, 0.0, {}, ""};
  const bool clean_process_list = w.processes.size() == 2 &&
                                  w.proc(w.game_pid()) != nullptr &&
                                  w.proc(w.ac_pid()) != nullptr;

  if (w.pcie_peer_dma_active) {
    add_signal(result, "PCIe peer-to-peer DMA channel is active", 0.42);
  }
  if (w.pcie_peer_transactions > 0) {
    add_signal(result, "PCIe peer-to-peer transactions were observed", 0.16);
  }
  if (w.pcie_peer_bypassed_iommu) {
    add_signal(result, "peer-to-peer TLPs bypassed IOMMU translation", 0.20);
  }
  if (w.trust.dma_device_present) {
    add_signal(result, "DMA-capable PCIe device is present", 0.07);
  }
  if (clean_process_list) {
    add_signal(result, "no local cheat process or game-memory handle is present", 0.05);
  }
  if (w.trust.iommu_on) {
    add_signal(result, "IOMMU is enabled despite the peer-to-peer bypass", 0.10);
  }

  result.detected = result.signals >= 2;
  result.mitigated = result.risk >= 0.90;
  result.detail = "pcie_peer_dma blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  w.note(result.detail);
  return result;
}

}  // namespace examples::pcie_peer_dma