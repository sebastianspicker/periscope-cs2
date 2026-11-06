#include "red_example.hpp"

namespace examples::pcie_peer_dma {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, 0, false, ""};
  const auto game = w.game_pid();
  const auto* game_process = w.proc(game);
  if (game == 0 || game_process == nullptr) {
    return {false, result.steps, result.transactions, result.iommu_bypassed,
            "game process is unavailable"};
  }

  ++result.steps;
  w.trust.dma_device_present = true;
  w.trust.iommu_on = true;

  // The peer channel is modeled by the dedicated scars rather than dma_read(),
  // which correctly remains blocked while the IOMMU is enabled.
  ++result.steps;
  const auto entity_offset = static_cast<std::size_t>(w.lab_entity_table_rel);
  const bool entities_obtained =
      entity_offset < game_process->memory.size() &&
      game_process->memory[entity_offset] != 0;
  if (entities_obtained) {
    w.pcie_peer_dma_active = true;
    w.pcie_peer_transactions = 4;
    w.pcie_peer_bypassed_iommu = true;
    result.transactions = w.pcie_peer_transactions;
    result.iommu_bypassed = w.pcie_peer_bypassed_iommu;
  }

  ++result.steps;
  result.achieved = w.pcie_peer_dma_active && result.iommu_bypassed &&
                    entities_obtained;
  result.detail = result.achieved
                      ? "PCIe peer DMA completed 4 device-to-device entity transfers while IOMMU remained enabled"
                      : "PCIe peer DMA entity transfer failed";
  w.note(result.detail);
  return result;
}

}  // namespace examples::pcie_peer_dma
