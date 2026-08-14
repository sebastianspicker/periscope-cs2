// teams_primary_tests — drives only shipped code/teams APIs on sim::World.
// No strategies dependency: pure red/blue tier contracts for T0–T4.

#include "sim/world.hpp"
#include "ac/telemetry.hpp"
#include "lab/fixture_process.hpp"

#include "t0_red/cheat_client.hpp"
#include "t0_red/rpm_backend.hpp"
#include "t0_blue/ac_agent.hpp"

#include "t1_red/syscall_cheat.hpp"
#include "t1_blue/hook_trap.hpp"
#include "t1_blue/syscall_aware_monitor.hpp"

#include "t2_red/kernel_radar.hpp"
#include "t2_blue/kernel_ac.hpp"

#include "t3_red/hv_radar.hpp"
#include "t3_red/hv_backend.hpp"
#include "t3_red/evasion_t3_advanced.hpp"
#include "t3_red/fallback_chain.hpp"
#include "t3_blue/platform_ac.hpp"
#include "t3_blue/attestation_gate.hpp"
#include "t3_blue/trust_policy.hpp"

#include "t4_red/dma_radar.hpp"
#include "t4_red/evasion_t4_advanced.hpp"
#include "t4_blue/dma_defense.hpp"

#include <cstdio>
#include <memory>

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
  // ── T0: handle RPM scar + multi-reason blue ─────────────────────────
  {
    auto w = sim::make_arena();
    t0_red::CheatClient red(w, "api-radar.exe");
    expect(red.attach_to_game(), "t0 red attach");
    expect(red.pull_entities() && !red.entities().empty(), "t0 red entities");
    auto rep = red.run_full_loop();
    expect(rep.attached && rep.entities_ok && rep.foreign_vm_read_handles >= 1,
           "t0 full_loop scars");
    ac::MemoryTelemetrySink sink;
    t0_blue::AcAgent blue(w, sink);
    auto d = blue.full_scan();
    expect(d.handle_hit && !d.reasons.empty() && d.risk > 0,
           "t0 blue multi-reason");
  }

  // ── T1: syscall-path handle + hooks blind + handle truth ────────────
  {
    auto w = sim::make_arena();
    t1_red::SyscallCheat red(w);
    expect(red.attach_via_syscall() && red.pull_entities(), "t1 red");
    auto rep = red.run_full_loop(true, true);
    expect(rep.attached && rep.via_syscall && rep.syscall_handles >= 1,
           "t1 full_loop");
    t1_blue::UsermodeHookTrap hooks;
    t1_blue::HandleTruthMonitor truth;
    const auto g = w.game_pid();
    expect(hooks.count_visible_opens(w, g) == 0, "t1 hooks blind");
    expect(truth.count_vm_read_handles(w, g) >= 1, "t1 handle truth");
    ac::MemoryTelemetrySink sink;
    t1_blue::T1Agent agent(w, sink);
    auto d = agent.full_scan();
    expect(d.handle_truth_hit && d.hooks_blind && !d.reasons.empty(),
           "t1 blue multi-reason");
  }

  // ── T2: no game handle + BYOVD/device/callback blue ─────────────────
  {
    auto w = sim::make_arena();
    t2_red::KernelRadar red(w);
    expect(red.bring_up(t2_red::KernelPath::Byovd), "t2 red bring_up");
    expect(red.pull_entities() && !red.has_game_handle(), "t2 red no handle");
    auto rep = red.run_full_loop(t2_red::KernelPath::Byovd, true);
    expect(rep.brought_up && rep.entities_ok && rep.no_game_handle && rep.byovd,
           "t2 full_loop scars");
    ac::MemoryTelemetrySink sink;
    t2_blue::KernelAc blue(w, sink);
    blue.blocklist_add("lab_byovd_hash_001");
    auto d = blue.full_scan();
    expect(d.byovd && d.suspicious_device && !d.reasons.empty(),
           "t2 blue multi-reason");
  }

  // ── T3: HV+bridge + advanced multi-step + attestation multi-reason ──
  {
    auto w = sim::make_arena();
    t3_red::HvRadar red(w);
    red.force_disable_vbs_for_lab();
    expect(red.try_hv("ACLABHV") && red.open_bridge(), "t3 red hv+bridge");
    expect(red.attach_best().ok && red.pull_entities(), "t3 red entities");

    t3_red::EvasionT3Advanced adv(w);
    auto mx = adv.max_hv_stealth("ACLABHV");
    expect(mx.entities_ok && mx.smm_read_channel && mx.tpm_measurement_spoof,
           "t3 max wave10");

    auto w2 = sim::make_arena();
    t3_red::EvasionT3Advanced adv2(w2);
    auto dp = adv2.deep_hv_stealth("ACLABHV");
    expect(dp.ept_nested_hide && dp.msr_hooking && dp.guest_time_dilation,
           "t3 deep wave11");

    ac::MemoryTelemetrySink sink;
    t3_blue::PlatformAc blue(w, sink);
    auto det = blue.full();
    expect(!det.ranked_allowed || det.hv_anomaly || det.bridge_hit, "t3 blue");

    t3_blue::TrustPolicy pol(sink);
    t3_blue::AttestationGate gate(pol);
    auto admit = gate.evaluate_world(w);
    expect(!admit.allow_ranked && admit.reasons.size() >= 1,
           "t3 attest multi-reason");

    t3_red::FallbackChain chain;
    chain.add(std::make_unique<t3_red::HvReadBackend>());
    chain.add(std::make_unique<t0_red::RpmBackend>());
    expect(chain.attach_first_available(lab::global_fixture().id()) ==
               ac::Status::Ok,
           "t3 fallback attach");
    expect(chain.last_report().ok && chain.last_report().attempts >= 1,
           "t3 fallback report");
  }

  // ── T4: clean process + multi-sensor DMA defense ────────────────────
  {
    auto w = sim::make_arena();
    auto rr = t4_red::apply(w);
    expect(rr.achieved && rr.process_list_clean, "t4 red apply");

    auto mx = t4_red::max_offbox_stealth(w);
    expect(mx.achieved && mx.radar.fpga_active, "t4 max offbox");

    auto w2 = sim::make_arena();
    auto dp = t4_red::deep_offbox_stealth(w2);
    expect(dp.achieved && dp.thunderbolt_dma && dp.usb_dfu_dma, "t4 deep offbox");

    t4_blue::DmaDefense def(w);
    auto s = def.full();
    expect((s.platform_signal || s.dma_device) && !s.reasons.empty() && s.risk > 0,
           "t4 blue multi-sensor");
  }

  if (fails) {
    std::fprintf(stderr, "teams_primary_tests: %d FAIL\n", fails);
    return 1;
  }
  std::puts("teams_primary_tests: all passed");
  return 0;
}
