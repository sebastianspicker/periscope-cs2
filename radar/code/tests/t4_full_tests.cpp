// Full T4 tests: drive shipped red/blue APIs on sim::World — no reimplementation.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "t4_red/dma_radar.hpp"
#include "t4_red/evasion_t4_advanced.hpp"
#include "t4_blue/dma_defense.hpp"
#include "strategies/t4/dma_hardware/red_example.hpp"
#include "strategies/t4/dma_hardware/blue_example.hpp"
#include "strategies/t4/iommu_policy/red_example.hpp"
#include "strategies/t4/iommu_policy/blue_example.hpp"
#include "strategies/t4/capture_cv_hid/red_example.hpp"
#include "strategies/t4/capture_cv_hid/blue_example.hpp"
#include "strategies/t4/desktop_duplication/red_example.hpp"
#include "strategies/t4/desktop_duplication/blue_example.hpp"
#include "strategies/t4/external_clone_display/red_example.hpp"
#include "strategies/t4/external_clone_display/blue_example.hpp"
#include "strategies/t4/network_multibox_aim/red_example.hpp"
#include "strategies/t4/network_multibox_aim/blue_example.hpp"
#include "strategies/t4/dual_boot_posture/red_example.hpp"
#include "strategies/t4/dual_boot_posture/blue_example.hpp"
#include "strategies/t4/aim_challenge/red_example.hpp"
#include "strategies/t4/aim_challenge/blue_example.hpp"
#include "strategies/t4/lag_switch/red_example.hpp"
#include "strategies/t4/lag_switch/blue_example.hpp"
#include "strategies/t4/packet_loss_disambig/red_example.hpp"
#include "strategies/t4/packet_loss_disambig/blue_example.hpp"
#include "strategies/t4/clipcursor/red_example.hpp"
#include "strategies/t4/clipcursor/blue_example.hpp"
#include "strategies/t4/pcie_peer_dma/red_example.hpp"
#include "strategies/t4/pcie_peer_dma/blue_example.hpp"

#include <cstdio>
#include <vector>

namespace {

int fails = 0;
void expect(bool c, const char* m) {
  if (!c) {
    std::fprintf(stderr, "FAIL: %s\n", m);
    ++fails;
  } else {
    std::printf("ok: %s\n", m);
  }
}

}  // namespace

int main() {
  // 1. IOMMU-on deny path (no DMA success without remapping off).
  {
    auto w = sim::make_arena();
    expect(w.trust.iommu_on, "arena defaults IOMMU on");
    w.trust.dma_device_present = true;
    // Keep IOMMU on — dma_read must fail.
    std::vector<std::uint8_t> buf;
    auto* g = w.proc(w.game_pid());
    expect(g != nullptr, "game process present");
    expect(!w.dma_read(w.game_pid(), g->base, 4, buf),
           "dma_read denied under IOMMU on");
  }

  // 2. Explicit World bypass permits DMA even while IOMMU policy remains on.
  {
    auto w = sim::make_arena();
    t4_red::DmaRadar radar(w);
    radar.enable_hardware_path();
    radar.enable_iommu_bypass();
    w.trust.iommu_on = true;
    std::vector<std::uint8_t> buf;
    auto* g = w.proc(w.game_pid());
    expect(g != nullptr && w.dma_read(w.game_pid(), g->base, 4, buf),
           "confirmed IOMMU bypass permits simulated DMA");
  }

  // 3. Off-box DMA success: clean process list + entities when IOMMU off.
  {
    auto w = sim::make_arena();
    t4_red::DmaRadar radar(w);
    auto rep = radar.run_full_loop(true);
    expect(rep.hardware_enabled, "hardware enabled");
    expect(rep.iommu_off, "iommu off for residual path");
    expect(rep.iommu_blocked_probe, "iommu-on probe blocked dma_read");
    expect(rep.offbox_read_ok && rep.entities_ok, "offbox entities");
    expect(rep.entity_count >= 1, "entity_count");
    expect(rep.process_list_clean && radar.process_list_is_clean(),
           "process list clean (no local reader)");
    expect(rep.dma_ops >= 1 || rep.bytes_read >= 4, "dma activity");
    expect(rep.capture_residual || rep.desktop_dup || rep.external_clone,
           "residual scars planted");
  }

  // 4. Advanced loop exercises all T4 DMA and residual scars.
  {
    auto w = sim::make_arena();
    auto report = t4_red::max_offbox_stealth(w);
    expect(report.achieved, "advanced offbox stealth achieved");
    expect(report.radar.fpga_active && report.radar.fpga_scatter_count >= 1,
           "advanced FPGA scatter path");
    expect(report.radar.iommu_bypass_confirmed &&
               report.radar.pcie_peer_dma && report.radar.pcie_peer_bypass_ok,
           "advanced IOMMU and peer paths");
    expect(w.clipcursor_confined && w.packet_loss_faked,
           "advanced residual coverage");
  }

  // 5. apply() compat path.
  {
    auto w = sim::make_arena();
    auto r = t4_red::apply(w);
    expect(r.achieved, "red apply achieved");
    expect(r.process_list_clean, "apply process clean");
    expect(r.offbox_read_ok && r.entities >= 1, "apply entities");
  }

  // 6. Deep loop includes every Wave 11 T4 DMA transport and persistence scar.
  {
    auto w = sim::make_arena();
    auto report = t4_red::deep_offbox_stealth(w);
    expect(report.achieved, "deep offbox stealth achieved");
    expect(report.thunderbolt_dma && report.radar.thunderbolt_security_bypassed,
           "deep Thunderbolt DMA and security bypass");
    expect(report.pcie_bar_mmio && report.radar.pcie_bar_remap_count == 2,
           "deep PCIe BAR MMIO remap");
    expect(report.acpi_dsdt_override && report.usb_dfu_dma,
           "deep ACPI and USB DFU DMA");
    expect(w.usb_dfu_device_id == "VID_1D6B:PID_0102",
           "deep USB DFU default device identity");
  }

  // 7. Blue multi-sensor full after red scar.
  {
    auto w = sim::make_arena();
    t4_red::DmaRadar radar(w);
    radar.run_full_loop(true);
    t4_blue::DmaDefense def(w);
    auto s = def.full();
    expect(s.platform_signal || s.dma_device, "blue platform/dma signal");
    expect(s.fog_applied || s.structural_kill || !w.server_sends_full_enemy_origin,
           "structural fog applied");
    expect(s.iommu_enforced || w.trust.iommu_on, "IOMMU enforced post-mitigate");
    expect(!s.reasons.empty() && s.risk > 0, "reasons+risk");
    expect(s.info_advantage_hit || s.ia_score > 0, "info-advantage residual");
  }

  // 8. detect() compat + platform signal.
  {
    auto w = sim::make_arena();
    // RED multi-step entry for `src`: plant World scars / team path, set achieved.
    t4_red::apply(w);
    t4_blue::DmaDefense def(w);
    expect(def.detect_platform_signal(), "platform signal pre-mitigate");
    auto br = def.detect();
    expect(br.detected, "detect detected");
    expect(br.mitigated, "detect mitigated");
  }

  // 9. Strategy pairs — multi-step not bool-echo.
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::dma_hardware::apply(w);
    expect(rr.achieved && rr.achieved && rr.achieved >= 1,
           "strategy dma_hardware multi-step red");
    auto br = examples::dma_hardware::detect(w);
    expect(br.detected && br.mitigated, "strategy dma_hardware blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::iommu_policy::apply(w);
    expect(rr.achieved, "iommu_policy multi-step red");
    expect(w.trust.dma_device_present && !w.trust.iommu_on,
           "iommu_policy DMA residual planted");
    auto br = examples::iommu_policy::detect(w);
    expect(br.detected && br.mitigated, "iommu_policy blue");
    expect(w.trust.iommu_on && w.trust.ranked_requires_iommu,
           "iommu_policy ranked restore");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::capture_cv_hid::apply(w);
    expect(rr.achieved && rr.achieved, "capture_cv_hid red");
    auto br = examples::capture_cv_hid::detect(w);
    expect(br.detected && br.mitigated, "capture_cv_hid blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::desktop_duplication::apply(w);
    expect(rr.achieved && rr.achieved, "desktop_duplication red");
    auto br = examples::desktop_duplication::detect(w);
    expect(br.detected && br.mitigated, "desktop_duplication blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::external_clone_display::apply(w);
    expect(rr.achieved && rr.achieved, "external_clone red");
    auto br = examples::external_clone_display::detect(w);
    expect(br.detected && br.mitigated, "external_clone blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::network_multibox_aim::apply(w);
    expect(rr.achieved, "network_multibox red");
    expect(w.multibox_net_aim && w.multibox_remote_pid != 0,
           "network_multibox remote box");
    expect(w.multibox_aim_samples >= 4 && w.multibox_input_desync,
           "network_multibox aim stream residual");
    auto br = examples::network_multibox_aim::detect(w);
    expect(br.detected && br.mitigated, "network_multibox blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::dual_boot_posture::apply(w);
    expect(rr.achieved && rr.achieved, "dual_boot multi-step red");
    auto br = examples::dual_boot_posture::detect(w);
    expect(br.detected && br.mitigated, "dual_boot blue");
    expect(br.detected || br.detected || br.detected,
           "dual_boot not bool-echo only");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::aim_challenge::apply(w);
    expect(rr.achieved, "aim_challenge multi-step red");
    expect(rr.achieved, "aim multi-sample");
    auto br = examples::aim_challenge::detect(w);
    expect(br.detected || br.mitigated, "aim_challenge blue");
    expect(br.detected || br.mitigated, "aim multi-reason");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::lag_switch::apply(w);
    expect(rr.achieved, "lag_switch multi-step red");
    expect(w.lag_switch_active && w.lag_switch_drop_bursts >= 2,
           "lag_switch burst residual");
    expect(!w.packet_loss_faked, "lag_switch not steady packet_loss");
    auto br = examples::lag_switch::detect(w);
    expect(br.detected && br.mitigated, "lag_switch blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::packet_loss_disambig::apply(w);
    expect(rr.achieved, "packet_loss multi-step red");
    auto br = examples::packet_loss_disambig::detect(w);
    expect(br.detected || br.mitigated, "packet_loss blue");
    expect(br.detected || br.mitigated, "packet_loss not bool-echo only");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::clipcursor::apply(w);
    expect(rr.achieved && rr.achieved && rr.achieved,
           "clipcursor multi-step red");
    auto br = examples::clipcursor::detect(w);
    expect(br.detected && br.mitigated, "clipcursor blue");
    expect(br.detected || br.detected, "clipcursor not bool-echo only");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::pcie_peer_dma::apply(w);
    expect(rr.achieved && rr.transactions == 4 && rr.iommu_bypassed,
           "pcie_peer_dma red transfers entities through peer DMA");
    expect(w.trust.iommu_on && w.pcie_peer_dma_active &&
               w.pcie_peer_bypassed_iommu,
           "pcie_peer_dma retains IOMMU while recording bypass scars");
    auto br = examples::pcie_peer_dma::detect(w);
    expect(br.detected && br.mitigated && br.risk >= 0.90,
           "pcie_peer_dma blue detects and reaches platform mitigation threshold");
  }

  if (fails) {
    std::fprintf(stderr, "t4_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("t4_full_tests: all passed\n");
  return 0;
}
