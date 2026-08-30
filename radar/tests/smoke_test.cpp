// smoke_test.cpp — scaffold smoke: build/link sanity for shared + tier libs.

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "lab/fixture_process.hpp"
#include "server/ban_correlator.hpp"
#include "server/info_advantage.hpp"
#include "server/interest_mgmt.hpp"
#include "sim/world.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "t0_red/entity_pipeline.hpp"
#include "t0_red/radar_ui.hpp"
#include "t0_red/rpm_backend.hpp"
#include "t1_blue/syscall_aware_monitor.hpp"
#include "t1_red/syscall_backend.hpp"
#include "t2_blue/byovd_blocklist.hpp"
#include "t2_blue/device_watch.hpp"
#include "t2_red/ioctl_backend.hpp"
#include "t3_blue/trust_policy.hpp"
#include "t3_red/fallback_chain.hpp"
#include "t3_red/hv_backend.hpp"

#include <cstdio>
#include <memory>

namespace {

int fails = 0;

// expect: free function for this educational unit.
void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++fails;
  } else {
    std::printf("ok: %s\n", msg);
  }
}

}  // namespace

// CLI entry: dispatch list/all/run to the strategy catalog harness.
int main() {
  auto& fx = lab::global_fixture();

  // --- T0 red/blue pair ---
  {
    t0_red::RpmBackend rpm;
    expect(rpm.attach(fx.id()) == ac::Status::Ok, "t0 attach lab");
    t0_red::EntityPipeline pipe(rpm);
    expect(pipe.refresh(fx.base_address()) == ac::Status::Ok, "t0 entity parse");
    // Fixture plants ≥2 rows (2 alive + optional dead residual); radar uses living().
    expect(pipe.entities().size() >= 2, "t0 entities parsed");
    expect(pipe.living().size() == 2, "t0 two entities");

    t0_red::RadarUi ui;
    ui.update(pipe.living(), ac::Vec3{0, 0, 0});
    expect(ui.blips().size() == 2, "t0 radar blips");

    ac::MemoryTelemetrySink sink;
    t0_blue::HandleGraphMonitor mon(sink);
    mon.ingest_edges(100, {{200, 100, t0_blue::ProcessAccess::VmRead, "lab-radar"}});
    expect(mon.suspicious().size() == 1, "t0 handle hit");
    expect(!sink.events().empty(), "t0 telemetry");
  }

  // --- T1: hooks blind, handles still see (World syscall path) ---
  {
    auto w = sim::make_arena();
    const auto reader = w.spawn("t1-smoke.exe");
    t1_red::SyscallBackend sc;
    expect(sc.attach_world(w, reader, w.game_pid()) == ac::Status::Ok,
           "t1 attach_world");
    expect(sc.bypasses_usermode_api_hooks(), "t1 bypasses api hooks");
    // Lesson: syscall-shaped open still leaves a usermode-visible handle scar.
    expect(sc.exposes_usermode_handle(), "t1 still has handle");
    expect(sc.last_via_syscall(), "t1 via_syscall path");

    ac::MemoryTelemetrySink sink;
    t0_blue::HandleGraphMonitor handles(sink);
    t1_blue::SyscallAwareHandleMonitor mon(handles);
    mon.evaluate(w.game_pid(),
                 {{reader, w.game_pid(), t0_blue::ProcessAccess::VmRead,
                   "packed"}},
                 /*usermode_hooks_saw_rpm=*/false);
    expect(mon.last_hit_count() == 1, "t1 handle despite no hooks");
    expect(!mon.detection_depends_on_ntdll_hooks(), "t1 design rule");
  }

  // --- T2: no game handle, device + byovd ---
  {
    t2_red::IoctlReadBackend ioctl;
    expect(ioctl.attach(fx.id()) == ac::Status::Ok, "t2 ioctl attach");
    expect(!ioctl.exposes_usermode_handle_to_game(), "t2 no game handle");
    expect(ioctl.opens_device(), "t2 opens device");

    ac::MemoryTelemetrySink sink;
    t2_blue::DeviceWatch dw(sink);
    dw.set_suspicious_names({"AcLabMemRw"});
    dw.on_device_open({9, ioctl.device_name()});
    expect(!sink.events().empty(), "t2 device watch");

    t2_blue::ByovdBlocklist bl(sink);
    bl.add("deadbeef");
    expect(bl.check_and_emit("deadbeef", "vuln.sys"), "t2 byovd block");
  }

  // --- T3: policy + fallback chain ---
  {
    ac::MemoryTelemetrySink sink;
    t3_blue::TrustPolicy policy(sink);
    auto denied = policy.evaluate_ranked({.secure_boot = true,
                                          .vbs = false,
                                          .hvci = false,
                                          .unexpected_hypervisor = false});
    expect(!denied.allow_ranked, "t3 deny ranked without vbs");

    t3_red::FallbackChain chain;
    auto w_block = sim::make_arena();
    auto hv = std::make_unique<t3_red::HvReadBackend>();
    expect(hv->simulate_hv_init(w_block, "ACLABHV") == ac::Status::Denied,
           "t3 hv blocked when vbs on");
    chain.add(std::move(hv));
    chain.add(std::make_unique<t0_red::RpmBackend>());
    // HV active path (fixture self-activates without World)
    {
      t3_red::FallbackChain chain2;
      auto hv2 = std::make_unique<t3_red::HvReadBackend>();
      chain2.add(std::move(hv2));
      expect(chain2.attach_first_available(fx.id()) == ac::Status::Ok,
             "t3 fallback hv ok");
      expect(chain2.active_tier() == ac::Tier::T3_Hypervisor, "t3 active tier");
    }
  }

  // --- Server residual ---
  {
    server::InterestManager im;
    std::vector<server::WorldEntity> all = {
        {1, {0, 0, 0}, 1, true},
        {2, {1000, 0, 0}, 2, true},
    };
    auto filtered =
        im.filter_for_client({{0, 0, 0}, 0}, 1, all, /*enemy_radius=*/50.f);
    expect(filtered.size() == 1, "server fog hides far enemy");

    server::InfoAdvantageScorer scorer;
    scorer.on_frame({0, {}, 0, false, false, true});
    expect(scorer.result().hits == 1, "info advantage hit");

    ac::RiskAggregator risk;
    risk.ingest({.kind = ac::EventKind::HandleToGame, .risk_delta = 3});
    risk.ingest({.kind = ac::EventKind::InfoAdvantageHit, .risk_delta = 2});
    server::BanCorrelator ban;
    // client flag_overwatch (info hit) + scorer hits → overwatch / soft action
    auto d = ban.evaluate(risk.state(), scorer.result().score);
    expect(d.action == server::BanAction::FlagOverwatch ||
               d.action == server::BanAction::DelayedBanCandidate,
           "ban correlator flags");
  }

  if (fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", fails);
    return 1;
  }
  std::puts("all scaffold smoke checks passed");
  return 0;
}
