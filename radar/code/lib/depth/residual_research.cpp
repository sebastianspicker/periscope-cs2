// residual_research.cpp — depth analytics: multi-sample / fusion scoring for blue.
// Educational scorers over World residuals.

#include "depth/residual_research.hpp"
#include "depth/trust_aggregator.hpp"
#include "depth/leakage_scorer.hpp"

#include <sstream>
#include <vector>

namespace depth {

ResidualExampleResult run_vmx_research_example(sim::World& w,
                                               sim::Narrator* n) {
  ResidualExampleResult r;
  r.family = "vmx";
  if (n) {
    n->move(sim::Side::Red, "VMX residual (lab)",
            "Disable platform trust, start personal HV, plant thin bridge.");
  }

  // Red multi-step: trust off → personal HV → bridge driver/device → hv_read.
  w.trust.vbs = false;
  w.trust.hvci = false;
  w.trust.secure_boot = false;
  const bool hv = w.try_start_personal_hv("lab-vmx");
  w.load_driver(sim::Driver{"hvcomm.sys", "sha-hvcomm", "lab-self-signed", false,
                            false, false, true, true, 10});
  w.create_device(sim::Device{"\\Device\\HvComm", "hvcomm.sys", true});
  const auto bridge = w.spawn("bridge-ui.exe");
  std::vector<std::uint8_t> buf;
  const auto game = w.game_pid();
  bool read_ok = false;
  if (game) {
    read_ok = w.hv_read(bridge, game, w.proc(game)->base, 4, buf);
  }
  r.red_achieved = hv && read_ok;
  r.red_detail = std::string("personal_hv=") + (hv ? "1" : "0") +
                 " hv_read=" + (read_ok ? "1" : "0") + " bridge=hvcomm";

  // Blue: trust aggregate + bridge hunt + ranked/session policy enforce.
  TrustAggregator agg;
  // Timeline with flips: sample current hostile state thrice (inconsistent vs baseline claims).
  auto ar = agg.evaluate_world_timeline(w, 3);
  bool bridge_hit = false;
  for (const auto& d : w.drivers) {
    if (d.is_bridge) {
      bridge_hit = true;
    }
  }
  r.blue_detected = !ar.allow_ranked || bridge_hit || w.trust.personal_hv_active;
  // Real mitigate: deny ranked session + stop personal HV bridge utility.
  if (r.blue_detected && !ar.allow_ranked) {
    w.ranked_access_denied = true;
    w.trust.personal_hv_active = false;  // tear down personal HV after policy
    // Strip bridge device mem path so post-mitigate hv_read cannot succeed.
    for (auto& d : w.drivers) {
      if (d.is_bridge) {
        d.provides_mem_rw = false;
      }
    }
    for (auto& dev : w.devices) {
      for (const auto& drv : w.drivers) {
        if (drv.name == dev.owner_driver && drv.is_bridge) {
          dev.mem_rw_ioctl = false;
        }
      }
    }
  }
  r.blue_mitigated = !ar.allow_ranked && w.ranked_access_denied;
  r.blue_detail = ar.detail + " bridge=" + (bridge_hit ? "1" : "0") +
                  " ranked_denied=" + (w.ranked_access_denied ? "1" : "0");
  if (n) {
    n->say(sim::Side::Blue, r.blue_detail);
  }
  w.note("vmx_research " + r.red_detail + " | " + r.blue_detail);
  return r;
}

ResidualExampleResult run_byovd_research_example(sim::World& w,
                                                 sim::Narrator* n) {
  ResidualExampleResult r;
  r.family = "byovd";
  if (n) {
    n->move(sim::Side::Red, "BYOVD residual (lab)",
            "Load known-bad signed driver, IOCTL read without game handle.");
  }

  w.load_driver(sim::Driver{"vuln-cap.sys", "sha-byovd-bad", "Microsoft Windows",
                            false, true, false, false, true, 5});
  w.create_device(sim::Device{"\\Device\\VulnCap", "vuln-cap.sys", true});
  const auto ui = w.spawn("radar-ui.exe");
  const auto game = w.game_pid();
  std::vector<std::uint8_t> buf;
  bool ioctl_ok = false;
  if (game) {
    ioctl_ok = w.device_ioctl_read(ui, "\\Device\\VulnCap", game,
                                   w.proc(game)->base, 4, buf);
  }
  // No usermode handle to game.
  bool no_handle = true;
  for (const auto& h : w.handles_to(game)) {
    if (sim::has(h.access, sim::AccessMask::VmRead) && h.owner_pid == ui) {
      no_handle = false;
    }
  }
  r.red_achieved = ioctl_ok && no_handle;
  r.red_detail = std::string("ioctl=") + (ioctl_ok ? "1" : "0") +
                 " no_game_handle=" + (no_handle ? "1" : "0") +
                 " byovd_driver=vuln-cap.sys";

  // Blue: driver blocklist + device watch (multi-step).
  bool byovd_load = false;
  bool device_open = false;
  for (const auto& d : w.drivers) {
    if (d.byovd_known_bad) {
      byovd_load = true;
    }
  }
  for (const auto& d : w.devices) {
    if (d.mem_rw_ioctl) {
      device_open = true;
    }
  }
  r.blue_detected = byovd_load || device_open;

  // Real mitigate: enforce BYOVD policy (IOCTL denied) + ranked session deny.
  // Not a no-op flag — post-mitigate device_ioctl_read must fail.
  if (r.blue_detected) {
    w.byovd_policy_block = true;
    w.ranked_access_denied = true;
    // Also strip mem_rw on known-bad devices (defense in depth).
    for (auto& d : w.drivers) {
      if (d.byovd_known_bad) {
        d.provides_mem_rw = false;
      }
    }
    for (auto& dev : w.devices) {
      for (const auto& drv : w.drivers) {
        if (drv.name == dev.owner_driver && drv.byovd_known_bad) {
          dev.mem_rw_ioctl = false;
        }
      }
    }
    std::vector<std::uint8_t> after;
    bool ioctl_after = false;
    if (game) {
      ioctl_after = w.device_ioctl_read(ui, "\\Device\\VulnCap", game,
                                        w.proc(game)->base, 4, after);
    }
    r.blue_mitigated =
        !ioctl_after && w.byovd_policy_block && w.ranked_access_denied;
    r.blue_detail = std::string("byovd_load=") + (byovd_load ? "1" : "0") +
                    " mem_rw_device=" + (device_open ? "1" : "0") +
                    " ioctl_after_block=" + (ioctl_after ? "1" : "0") +
                    " ranked_denied=" + (w.ranked_access_denied ? "1" : "0");
  } else {
    r.blue_detail = "byovd_load=0 mem_rw_device=0";
  }
  if (n) {
    n->say(sim::Side::Blue, r.blue_detail);
  }
  w.note("byovd_research " + r.red_detail + " | " + r.blue_detail);
  return r;
}

ResidualExampleResult run_dma_research_example(sim::World& w,
                                               sim::Narrator* n) {
  ResidualExampleResult r;
  r.family = "dma";
  if (n) {
    n->move(sim::Side::Red, "DMA residual (lab)",
            "PCIe DMA-style read with IOMMU off; no local cheat process.");
  }

  w.trust.dma_device_present = true;
  w.trust.iommu_on = false;
  const auto game = w.game_pid();
  std::vector<std::uint8_t> buf;
  bool dma_ok = false;
  if (game) {
    dma_ok = w.dma_read(game, w.proc(game)->base, 4, buf);
  }
  r.red_achieved = dma_ok;
  r.red_detail = std::string("dma_read=") + (dma_ok ? "1" : "0") +
                 " iommu_off=1 device_present=1";

  // Detect on device presence / successful off-box read while IOMMU was off.
  r.blue_detected = w.trust.dma_device_present || dma_ok;

  // Blue multi-step mitigate: enable IOMMU + ranked requires IOMMU +
  // structural fog (no full enemy origin replication).
  w.trust.iommu_on = true;
  w.trust.ranked_requires_iommu = true;
  w.server_sends_full_enemy_origin = false;
  w.entity_stream_encrypted = true;
  w.client_has_stream_key = false;
  w.stream_key_exfiltrated = false;
  if (r.blue_detected) {
    w.ranked_access_denied = true;  // DMA residual without prior IOMMU
  }
  std::vector<std::uint8_t> buf2;
  bool dma_blocked = game && !w.dma_read(game, w.proc(game)->base, 4, buf2);

  LeakageScorer leak;
  leak.set_fog(FogPolicy::StrictRadius);
  server::Observer obs{{0, 0, 0}, 0};
  std::vector<EntityTruth> ents = {
      {1, {0, 0, 0}, 1, true, true},
      {2, {999, 0, 999}, 2, true, false},
  };
  StreamCryptoState st{true, false, false, true};
  auto ls = leak.score(obs, 1, ents, st);

  r.blue_mitigated = dma_blocked && ls.structural_kill && w.trust.iommu_on &&
                     !w.server_sends_full_enemy_origin;
  r.blue_detail = std::string("iommu_on=1 dma_blocked=") +
                  (dma_blocked ? "1" : "0") + " fog_kill=" +
                  (ls.structural_kill ? "1" : "0") +
                  " ranked_denied=" + (w.ranked_access_denied ? "1" : "0");
  if (n) {
    n->say(sim::Side::Blue, r.blue_detail);
  }
  w.note("dma_research " + r.red_detail + " | " + r.blue_detail);
  return r;
}

ResidualExampleResult run_smm_research_example(sim::World& w,
                                               sim::Narrator* n) {
  ResidualExampleResult r;
  r.family = "smm";
  if (n) {
    n->move(sim::Side::Red, "SMM residual (lab)",
            "Firmware-class residual scar + trust/attest interaction (sim only).");
  }

  // Educational residual: SMM-class implant leaves host trust timeline dirty.
  // 
  w.smm_residual = true;
  w.trust.attestation_valid = false;
  w.trust.attestation_pcr_ok = false;
  w.trust.secure_boot = true;  // claims secure boot but PCR/attest fail
  w.trust.secure_kernel_view_dirty = true;
  w.trust.unexpected_efi_entry = true;
  w.trust.efi_entry_name = "lab-smm-hook";
  r.red_achieved = w.smm_residual && !w.trust.attestation_valid;
  r.red_detail =
      "smm_residual=1 attest_fail=1 efi_entry=lab-smm-hook sk_dirty=1";

  // Blue: trust aggregator over time catches SMM residual + attest inconsistency.
  TrustAggregator agg;
  agg.require_no_smm_residual(true);
  // Tick 0: plant state
  agg.push(agg.capture(w, 0));
  // Tick 1: red tries to clear guest-visible bits but residual remains
  w.trust.secure_boot = true;
  w.smm_residual = true;  // persists below OS
  agg.push(agg.capture(w, 1));
  // Tick 2: dual-view still dirty
  agg.push(agg.capture(w, 2));
  auto ar = agg.evaluate();

  r.blue_detected = ar.spoof_signals > 0 || ar.policy_fails > 0 ||
                    w.smm_residual;
  // Real mitigate: ranked/session deny on SMM residual + trust failure.
  if (r.blue_detected && !ar.allow_ranked) {
    w.ranked_access_denied = true;
    w.trust.attestation_valid = false;  // keep attest fail latched
  }
  r.blue_mitigated = !ar.allow_ranked && w.ranked_access_denied;
  r.blue_detail = ar.detail + " smm=1 ranked_denied=" +
                  (w.ranked_access_denied ? "1" : "0");
  if (n) {
    n->say(sim::Side::Blue, r.blue_detail);
  }
  w.note("smm_research " + r.red_detail + " | " + r.blue_detail);
  return r;
}

// run_all_residual_research_examples: free function for this educational unit.
std::string run_all_residual_research_examples() {
  std::ostringstream oss;
  {
    auto w = sim::make_arena();
    auto r = run_vmx_research_example(w);
    oss << "vmx red=" << r.red_achieved << " det=" << r.blue_detected
        << " mit=" << r.blue_mitigated << "\n";
  }
  {
    auto w = sim::make_arena();
    auto r = run_byovd_research_example(w);
    oss << "byovd red=" << r.red_achieved << " det=" << r.blue_detected
        << " mit=" << r.blue_mitigated << "\n";
  }
  {
    auto w = sim::make_arena();
    auto r = run_dma_research_example(w);
    oss << "dma red=" << r.red_achieved << " det=" << r.blue_detected
        << " mit=" << r.blue_mitigated << "\n";
  }
  {
    auto w = sim::make_arena();
    auto r = run_smm_research_example(w);
    oss << "smm red=" << r.red_achieved << " det=" << r.blue_detected
        << " mit=" << r.blue_mitigated << "\n";
  }
  return oss.str();
}

}  // namespace depth
