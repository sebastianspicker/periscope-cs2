#include "red_example.hpp"

#include <array>
#include <vector>

namespace examples::fpga_smart_dma {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, 0, false, ""};
  const auto game = w.game_pid();
  const auto* game_process = w.proc(game);
  if (game == 0 || game_process == nullptr) {
    return {false, result.steps, result.scatter_reads, result.hidden_rescan,
            "game process is unavailable"};
  }

  ++result.steps;
  w.trust.dma_device_present = true;
  w.trust.iommu_on = false;
  w.fpga_smart_dma_active = true;

  // Model page-table-guided, bounded reads from the resolved entity-table page.
  constexpr std::array<std::size_t, 3> kPageFragments = {0, 16, 32};
  bool entities_obtained = true;
  for (const auto page_fragment : kPageFragments) {
    std::vector<std::uint8_t> fragment;
    if (!w.dma_read(game, game_process->base + w.lab_entity_table_rel +
                         page_fragment,
                    16, fragment) ||
        fragment.empty()) {
      entities_obtained = false;
      break;
    }
    ++result.scatter_reads;
  }

  ++result.steps;
  w.fpga_scatter_reads = result.scatter_reads;
  w.fpga_hidden_rescan = entities_obtained;
  result.hidden_rescan = w.fpga_hidden_rescan;
  ++result.steps;

  result.achieved = w.fpga_smart_dma_active &&
                    result.scatter_reads == static_cast<int>(kPageFragments.size()) &&
                    entities_obtained;
  result.detail = result.achieved
                      ? "FPGA completed 3 autonomous scatter-gather entity reads without OS scars"
                      : "FPGA scatter-gather entity read failed";
  w.note(result.detail);
  return result;
}

}  // namespace examples::fpga_smart_dma
