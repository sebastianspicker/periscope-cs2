// dma_defense.cpp — T4 blue DMA/IOMMU residual defense on World.trust.dma_* and structural fog.
// Simulated PCIe/DMA hardware residuals.

#include "t4_blue/dma_defense.hpp"

#include "depth/leakage_scorer.hpp"
#include "server/info_advantage.hpp"

#include <sstream>

namespace t4_blue {

// Educational T4 residual defense on sim::World: platform + residuals + fog.

DmaDefense::DmaDefense(sim::World& world) : world_(world) {}

// DmaDefense::detect_platform_signal: Detect DMA/platform residual signals on World.
bool DmaDefense::detect_platform_signal() const {
  return world_.trust.dma_device_present && !world_.trust.iommu_on;
}

// DmaDefense::apply_interest_mgmt: Apply structural fog/interest mitigate after DMA detect.
void DmaDefense::apply_interest_mgmt() {
  depth::LeakageScorer scorer;
  scorer.set_fog(depth::FogPolicy::StrictRadius);
  server::Observer obs{{0, 0, 0}, 0};
  std::vector<depth::EntityTruth> all = {
      {1, {0, 0, 0}, 1, true, true},
      {2, {999, 0, 999}, 2, true, false},
  };
  scorer.mitigate_world(world_, obs, 1, all);
  world_.trust.iommu_on = true;
  world_.trust.ranked_requires_iommu = true;
  world_.note("t4 blue: IOMMU+fog+stream multi-step residual defense");
}

// DmaDefense::detect: BLUE multi-reason inspect of World scars; may set mitigated policy flags.
BlueResult DmaDefense::detect() {
  BlueResult r;
  r.dma_device = world_.trust.dma_device_present;
  r.iommu_off = !world_.trust.iommu_on;
  r.detected = detect_platform_signal() || world_.trust.dma_device_present;
  apply_interest_mgmt();
  r.mitigated = !world_.server_sends_full_enemy_origin && world_.trust.iommu_on;
  r.detail = "dma_device=" + std::string(r.dma_device ? "yes" : "no") +
             " iommu=" + (world_.trust.iommu_on ? "on" : "off") +
             " signal=" + (r.detected ? "yes" : "no") +
             " fog=" + (r.mitigated ? "on" : "off");
  return r;
}

// DmaDefense::scan_platform: Scan World for platform residuals.
PlatformScan DmaDefense::scan_platform() const {
  PlatformScan s;
  s.dma_device = world_.trust.dma_device_present;
  s.iommu_off = !world_.trust.iommu_on;
  s.platform_signal = s.dma_device && s.iommu_off;
  s.ranked_allowed = !(world_.trust.ranked_requires_iommu && s.iommu_off);
  if (s.dma_device) {
    s.reasons.push_back("dma_device_present");
    s.risk += 3.0;
  }
  if (s.iommu_off) {
    s.reasons.push_back("iommu_off");
    s.risk += 2.0;
  }

  // T4: detect active DMA bypass techniques
  if (world_.iommu_bypass_active) {
    s.risk += 4.0;
    s.reasons.push_back("iommu_bypass_active");
  }
  if (world_.pcie_peer_dma_active) {
    s.risk += 3.5;
    s.reasons.push_back("pcie_peer_dma_active");
  }
  if (world_.fpga_smart_dma_active) {
    s.risk += 3.0;
    s.reasons.push_back("fpga_smart_dma_active");
  }
  if (world_.thunderbolt_dma_active && world_.thunderbolt_security_bypassed) {
    s.risk += 2.5;
    s.reasons.push_back("thunderbolt_dma_with_security_bypass");
  }

  s.detail = "platform dma=" + std::string(s.dma_device ? "1" : "0") +
             " iommu_off=" + (s.iommu_off ? "1" : "0");
  return s;
}

// DmaDefense::scan_residuals: Scan World for residuals residuals.
PlatformScan DmaDefense::scan_residuals() const {
  PlatformScan s;
  s.capture_card = world_.trust.capture_card_present;
  s.desktop_dup = world_.desktop_duplication;
  s.external_clone = world_.external_display_clone;
  s.lag_switch = world_.lag_switch_active;
  s.dual_boot = world_.trust.dual_boot_profile;
  s.multibox_aim = world_.multibox_net_aim;
  s.packet_loss = world_.packet_loss_faked;
  s.clipcursor = world_.clipcursor_confined;
  if (s.capture_card) {
    s.reasons.push_back("capture_card");
    s.risk += 1.5;
  }
  if (s.desktop_dup) {
    s.reasons.push_back("desktop_duplication");
    s.risk += 1.0;
  }
  if (s.external_clone) {
    s.reasons.push_back("external_clone");
    s.risk += 1.0;
  }
  if (s.lag_switch) {
    s.reasons.push_back("lag_switch");
    s.risk += 1.0;
  }
  if (s.dual_boot) {
    s.reasons.push_back("dual_boot");
    s.risk += 1.0;
  }
  s.detail = "residuals n=" + std::to_string(s.reasons.size());
  return s;
}

// DmaDefense::score_behavioral: Score info-advantage / aim residuals without local RPM.
PlatformScan DmaDefense::score_behavioral() {
  PlatformScan s;
  server::InfoAdvantageScorer ia;
  for (int i = 0; i < 4; ++i) {
    server::DemoFrame f;
    f.t = static_cast<double>(i);
    f.aim_on_hidden_target = true;
    f.has_vision_on_target = false;
    f.has_audio_on_target = false;
    ia.on_frame(f);
  }
  auto r = ia.result();
  s.info_advantage_hit = r.hits >= 3;
  s.ia_score = r.score;
  if (s.info_advantage_hit) {
    s.reasons.push_back("info_advantage");
    s.risk += r.score;
  }
  s.detail = "ia_hits=" + std::to_string(r.hits);
  return s;
}

// DmaDefense::full: Full T4 blue: platform + behavioral + fog mitigate.
PlatformScan DmaDefense::full() {
  auto plat = scan_platform();
  auto res = scan_residuals();
  auto beh = score_behavioral();

  // Structural mitigate
  apply_interest_mgmt();

  PlatformScan out;
  out.dma_device = plat.dma_device;
  out.iommu_off = !world_.trust.iommu_on ? false : plat.iommu_off;
  out.platform_signal = plat.platform_signal;
  out.capture_card = res.capture_card;
  out.desktop_dup = res.desktop_dup;
  out.external_clone = res.external_clone;
  out.lag_switch = res.lag_switch;
  out.dual_boot = res.dual_boot;
  out.multibox_aim = res.multibox_aim;
  out.packet_loss = res.packet_loss;
  out.clipcursor = res.clipcursor;
  out.info_advantage_hit = beh.info_advantage_hit;
  out.ia_score = beh.ia_score;
  out.fog_applied = !world_.server_sends_full_enemy_origin;
  out.stream_encrypted = world_.entity_stream_encrypted;
  out.iommu_enforced = world_.trust.iommu_on;
  out.structural_kill = out.fog_applied;
  out.ranked_allowed = !(world_.trust.ranked_requires_iommu && !world_.trust.iommu_on);
  out.risk = plat.risk + res.risk + beh.risk;
  out.reasons = plat.reasons;
  out.reasons.insert(out.reasons.end(), res.reasons.begin(), res.reasons.end());
  out.reasons.insert(out.reasons.end(), beh.reasons.begin(), beh.reasons.end());
  if (out.fog_applied) {
    out.reasons.push_back("structural_fog");
  }
  std::ostringstream oss;
  oss << "full risk=" << out.risk << " reasons=" << out.reasons.size()
      << " fog=" << (out.fog_applied ? 1 : 0);
  out.detail = oss.str();
  return out;
}

// BLUE entry: multi-reason detect/mitigate on World scars.
BlueResult detect(sim::World& world) {
  DmaDefense def(world);
  return def.detect();
}

}  // namespace t4_blue
