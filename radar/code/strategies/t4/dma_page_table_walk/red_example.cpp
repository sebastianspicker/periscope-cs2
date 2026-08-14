#include "red_example.hpp"
#include <cstdio>

namespace examples::dma_page_table_walk {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  w.dma_page_table_walked = true;
  w.dma_page_table_base_pfn = 0xABCD0000;
  w.dma_page_entries_found = 512;
  w.dma_page_levels_walked = 4;

  w.pcie_peer_dma_active = true;
  w.pcie_peer_transactions = 10;

  w.trust.dma_device_present = true;
  w.trust.iommu_on = false;

  const bool scar = w.dma_page_table_walked && w.dma_page_entries_found >= 256 && w.dma_page_levels_walked >= 4;
  if (!scar)
    return {false, steps, "DMA page table walk scar failed"};

  std::printf("[T4 dma_page_table_walk] step %d: page table walked at PFN 0x%llx, %d entries, %d levels\n", ++steps, w.dma_page_table_base_pfn, w.dma_page_entries_found, w.dma_page_levels_walked);
  w.note("dma_page_table_walk: physical memory page table walk via DMA");
  return {true, steps, "dma_page_table_walk: 4-level page table walk", w.dma_page_table_base_pfn, w.dma_page_entries_found, w.dma_page_levels_walked};
}

}  // namespace examples::dma_page_table_walk
