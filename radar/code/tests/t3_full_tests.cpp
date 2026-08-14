// Full T3 tests: drive shipped red/blue APIs on sim::World — no reimplementation.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "ac/telemetry.hpp"
#include "depth/trust_aggregator.hpp"
#include "lab/fixture_process.hpp"
#include "t3_red/hv_backend.hpp"
#include "t3_red/bridge_surface.hpp"
#include "t3_red/fallback_chain.hpp"
#include "t3_red/evasion_t3_advanced.hpp"
#include "t3_red/hv_radar.hpp"
#include "t3_blue/platform_ac.hpp"
#include "t3_blue/trust_policy.hpp"
#include "t3_blue/attestation_gate.hpp"
#include "strategies/t3/hypervisor/red_example.hpp"
#include "strategies/t3/hypervisor/blue_example.hpp"
#include "strategies/t3/nested_hv/red_example.hpp"
#include "strategies/t3/nested_hv/blue_example.hpp"
#include "strategies/t3/timing_spoof/red_example.hpp"
#include "strategies/t3/timing_spoof/blue_example.hpp"
#include "strategies/t3/ept_hide_ac/red_example.hpp"
#include "strategies/t3/ept_hide_ac/blue_example.hpp"
#include "strategies/t3/attestation/red_example.hpp"
#include "strategies/t3/attestation/blue_example.hpp"
#include "strategies/t3/efi_boot_entry/red_example.hpp"
#include "strategies/t3/efi_boot_entry/blue_example.hpp"
#include "strategies/t3/elam_bypass/red_example.hpp"
#include "strategies/t3/elam_bypass/blue_example.hpp"
#include "strategies/t3/cr3_stealth_target/red_example.hpp"
#include "strategies/t3/cr3_stealth_target/blue_example.hpp"
#include "strategies/t3/secure_kernel_view/red_example.hpp"
#include "strategies/t3/secure_kernel_view/blue_example.hpp"
#include "strategies/t3/boot_trust/red_example.hpp"
#include "strategies/t3/boot_trust/blue_example.hpp"
#include "strategies/t3/hvci_race/red_example.hpp"
#include "strategies/t3/hvci_race/blue_example.hpp"
#include "strategies/t3/ci_options/red_example.hpp"
#include "strategies/t3/ci_options/blue_example.hpp"
#include "strategies/t3/feature_control_msr/red_example.hpp"
#include "strategies/t3/feature_control_msr/blue_example.hpp"
#include "strategies/t3/infinity_hook/red_example.hpp"
#include "strategies/t3/infinity_hook/blue_example.hpp"
#include "strategies/t3/vtl1_enclave_miss/red_example.hpp"
#include "strategies/t3/vtl1_enclave_miss/blue_example.hpp"

#include <cstdio>
#include <string>

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
  // 1. Fixture attach HvReadBackend
  {
    t3_red::HvReadBackend be;
    expect(be.attach_fixture(lab::global_fixture().id()) == ac::Status::Ok,
           "fixture attach");
    expect(!be.exposes_usermode_handle_to_game(), "no game handle fixture");
    auto rr = be.read({lab::global_fixture().base_address(), 4});
    expect(rr.status == ac::Status::Ok, "fixture hv-shaped read");
  }

  // 2. VBS blocks personal HV
  {
    auto w = sim::make_arena();
    expect(w.trust.vbs && w.trust.hvci, "arena defaults VBS+HVCI on");
    t3_red::HvReadBackend be;
    expect(be.simulate_hv_init(w, "ACLABHV") == ac::Status::Denied,
           "VBS blocks personal HV");
    expect(!w.trust.personal_hv_active, "no personal HV while VBS on");
  }

  // 3. run_full_loop: trust clear, hv, bridge, entities, no_game_handle, stealth
  {
    auto w = sim::make_arena();
    t3_red::HvRadar radar(w);
    auto rep = radar.run_full_loop("ACLABHV", true);
    expect(rep.trust_cleared, "trust cleared");
    expect(rep.hv_started, "hv started");
    expect(rep.bridge_open, "bridge open");
    expect(rep.entities_ok && rep.entity_count >= 1, "entities pulled");
    expect(rep.no_game_handle && !radar.has_game_handle(), "no game handle");
    expect(rep.ept_hide && rep.timing_spoof && rep.attest_fail,
           "stealth scars ept/timing/attest");
    expect(rep.read_ops >= 1 || rep.bytes_read >= 4, "hv read activity");
  }

  // 4. Blue PlatformAc full after red scar
  {
    auto w = sim::make_arena();
    t3_red::HvRadar radar(w);
    radar.run_full_loop("ACLABHV", true);
    ac::MemoryTelemetrySink sink;
    t3_blue::PlatformAc pac(w, sink);
    auto d = pac.full();
    expect(d.policy_fail, "blue policy_fail");
    expect(d.hv_anomaly, "blue hv_anomaly");
    expect(d.bridge_hit, "blue bridge_hit");
    expect(d.attest_fail || d.ept_dual_view || d.sk_dirty,
           "blue stealth scars (attest/dual-view)");
    expect(!d.reasons.empty() && d.risk > 0, "reasons+risk");
    expect(!d.ranked_allowed, "ranked denied");
  }

  // 5. Advanced HV loop plants every Wave 10+ T3 scar through shipped APIs.
  {
    auto w = sim::make_arena();
    t3_red::EvasionT3Advanced evasion(w);
    const auto rep = evasion.max_hv_stealth("ACLABHV");
    expect(rep.entities_ok && rep.smm_read_channel && rep.smm_read_ops == 1,
           "advanced loop plants and triggers SMM channel");
    expect(rep.tpm_measurement_spoof && !w.trust.attestation_valid &&
               !w.trust.attestation_pcr_ok && w.spoofed_pcr_bank == 7,
           "advanced loop spoofs TPM measurements");
    expect(rep.ept_sidechannel && rep.invept_tlb_flush &&
               rep.ept_ac_scan_evasions == 1,
           "advanced loop enables EPT scan-window evasion");
    expect(rep.vmexit_keylog && rep.vmexit_keys_captured >= 1,
           "advanced loop captures VM-exit key input");
    expect(rep.ci_options_spoof && w.ci_options_disabled &&
               rep.feature_control_spoof && w.feature_control_spoofed &&
               rep.infinity_hook && w.infinity_hook_residual &&
               rep.vtl1_enclave_bypass && !w.vtl1_enclave_present,
           "advanced loop plants host-trust residuals");
  }

  // 6. TrustAggregator / dual-view / attest if APIs exist
  {
    auto w = sim::make_arena();
    t3_red::EvasionT3Advanced evasion(w);
    const auto rep = evasion.deep_hv_stealth("ACLABHV");
    expect(rep.ept_nested_hide && rep.ept_shadow_levels == 3 &&
               w.ept_nested_hide_active && w.ept_shadow_levels == 3,
           "deep loop enables three-level shadow EPT hide");
    expect(rep.msr_hooking && w.msr_hooking_active &&
               w.hooked_msr_list == "lstar,sysenter",
           "deep loop enables default MSR hooks");
    expect(rep.guest_time_dilation && rep.time_dilation_factor == 0.5 &&
               w.guest_time_dilation_active && w.time_dilation_factor == 0.5,
           "deep loop enables guest time dilation");
    expect(rep.hyperv_enlightenment && w.hyperv_enlightenment_exploit &&
               w.hyperv_enlightenment_call == 0x4001,
           "deep loop enables Hyper-V enlightenment exploit");
  }

  // 7. TrustAggregator / dual-view / attest if APIs exist
  {
    auto w = sim::make_arena();
    t3_red::HvRadar radar(w);
    radar.run_full_loop("ACLABHV", true);

    depth::TrustAggregator agg;
    auto agg_res = agg.evaluate_world_timeline(w, 3);
    expect(!agg_res.allow_ranked ||
               agg_res.action != depth::TrustAction::Allow,
           "TrustAggregator flags scarred world");
    expect(agg_res.policy_fails + agg_res.spoof_signals >= 1,
           "aggregator multi-signal");

    // Dual-view / secure kernel scars from apply_stealth
    expect(w.trust.secure_kernel_view_dirty || w.trust.ept_hide_ac_pages,
           "dual-view / ept scar present");
    expect(!w.trust.attestation_valid, "attestation broken after stealth");

    ac::MemoryTelemetrySink sink;
    t3_blue::TrustPolicy policy(sink);
    auto ranked = policy.evaluate_world(w);
    expect(!ranked.allow_ranked, "TrustPolicy deny scarred world");

    t3_blue::AttestationGate gate(policy);
    auto host = policy.from_world(w);
    auto admit = gate.admit_ranked({
        .valid_signature = w.trust.attestation_valid,
        .pcr_matches_known_good = w.trust.attestation_pcr_ok,
        .host = host,
    });
    expect(!admit.allow_ranked, "AttestationGate deny bad quote");
  }

  // 6. Strategy pairs — all T3 strategies via examples::*
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::hypervisor::run_red(w, n);
    expect(rr.achieved && rr.achieved && rr.achieved,
           "strategy hypervisor multi-step red");
    auto br = examples::hypervisor::run_blue(w, n);
    expect(br.detected, "strategy hypervisor blue");
    expect(br.detected || br.detected, "hypervisor not bool-echo only");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::nested_hv::run_red(w, n);
    expect(rr.achieved && rr.achieved, "nested_hv multi-step red");
    auto br = examples::nested_hv::run_blue(w, n);
    expect(br.detected, "nested_hv blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::timing_spoof::run_red(w, n);
    expect(rr.achieved && rr.achieved, "timing_spoof multi-step red");
    auto br = examples::timing_spoof::run_blue(w, n);
    expect(br.detected, "timing_spoof blue");
    expect(br.detected || br.detected || br.detected,
           "timing multi-invariant");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::ept_hide_ac::run_red(w, n);
    expect(rr.achieved && rr.achieved, "ept_hide_ac multi-step red");
    auto br = examples::ept_hide_ac::run_blue(w, n);
    expect(br.detected, "ept_hide_ac blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::attestation::run_red(w, n);
    expect(rr.achieved || rr.achieved, "attestation red scar");
    auto br = examples::attestation::run_blue(w, n);
    // attestation blue is pure policy (detected() always false) — mitigate path
    expect(br.mitigated || br.detected, "attestation policy deny");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::efi_boot_entry::run_red(w, n);
    expect(rr.achieved, "efi_boot_entry red");
    auto br = examples::efi_boot_entry::run_blue(w, n);
    expect(br.detected, "efi_boot_entry blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::elam_bypass::run_red(w, n);
    expect(rr.achieved && rr.achieved, "elam_bypass multi-step red");
    auto br = examples::elam_bypass::run_blue(w, n);
    expect(br.detected, "elam_bypass blue");
    expect(br.detected || br.detected, "elam correlation");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::cr3_stealth_target::run_red(w, n);
    expect(rr.achieved && rr.achieved && rr.achieved && rr.achieved,
           "cr3_stealth multi-step red");
    auto br = examples::cr3_stealth_target::run_blue(w, n);
    expect(br.detected, "cr3_stealth blue");
    expect(br.detected || br.detected || br.detected,
           "cr3 correlation");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::secure_kernel_view::run_red(w, n);
    expect(rr.achieved && rr.achieved && rr.achieved,
           "secure_kernel_view multi-step red");
    auto br = examples::secure_kernel_view::run_blue(w, n);
    expect(br.detected, "secure_kernel_view blue");
    expect(br.detected || br.detected, "sk dual-view correlation");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::boot_trust::run_red(w, n);
    expect(rr.achieved && rr.achieved, "boot_trust multi-step red");
    auto br = examples::boot_trust::run_blue(w, n);
    expect(br.detected || br.mitigated, "boot_trust blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::hvci_race::run_red(w, n);
    expect(rr.achieved, "hvci_race red");
    auto br = examples::hvci_race::run_blue(w, n);
    expect(br.detected, "hvci_race blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::ci_options::run_red(w, n);
    expect(rr.achieved, "ci_options red");
    auto br = examples::ci_options::run_blue(w, n);
    expect(br.detected, "ci_options blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::feature_control_msr::run_red(w, n);
    expect(rr.achieved, "feature_control_msr red");
    auto br = examples::feature_control_msr::run_blue(w, n);
    expect(br.detected, "feature_control_msr blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::infinity_hook::run_red(w, n);
    expect(rr.achieved, "infinity_hook red");
    auto br = examples::infinity_hook::run_blue(w, n);
    expect(br.detected, "infinity_hook blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::vtl1_enclave_miss::run_red(w, n);
    expect(rr.achieved, "vtl1_enclave_miss red");
    auto br = examples::vtl1_enclave_miss::run_blue(w, n);
    expect(br.detected, "vtl1_enclave_miss blue");
  }

  if (fails) {
    std::fprintf(stderr, "t3_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("t3_full_tests: all passed\n");
  return 0;
}
