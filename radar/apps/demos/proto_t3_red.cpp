// T3 RED prototype — VBS gate, full HV path on World, fallback chain. 
#include "ac/proto_log.hpp"
#include "lab/fixture_process.hpp"
#include "sim/world.hpp"
#include "t0_red/entity_pipeline.hpp"
#include "t0_red/radar_ui.hpp"
#include "t0_red/rpm_backend.hpp"
#include "t2_red/ioctl_backend.hpp"
#include "t3_red/bridge_surface.hpp"
#include "t3_red/fallback_chain.hpp"
#include "t3_red/hv_backend.hpp"
#include "t3_red/hv_radar.hpp"
#include "t3_red/t3_hypercall_abi.h"

#include <cstdio>
#include <memory>

int main() {
  ac::proto_banner("RED", "T3", "HV read + bridge + T3→T2→T0 fallback");

  // Scenario A: World with VBS/HVCI on → personal HV denied (blue policy wins).
  {
    auto blocked = sim::make_arena();
    t3_red::HvReadBackend hv;
    const auto st = hv.simulate_hv_init(blocked, "ACLABHV");
    ac::proto_status("hv_init(vbs/hvci=on)", st);
    ac::proto_kv("hv_denied_by_trust", st == ac::Status::Denied);
  }

  // Scenario B: clear trust → HV → bridge → attach → entities (pure T3 path).
  auto world = sim::make_arena();
  t3_red::HvRadar radar(world);
  auto rep = radar.run_full_loop("ACLABHV", /*stealth=*/true);
  ac::proto_kv("trust_cleared", rep.trust_cleared);
  ac::proto_kv("hv_started", rep.hv_started);
  ac::proto_kv("bridge_open", rep.bridge_open);
  ac::proto_kv("entities_ok", rep.entities_ok);
  ac::proto_kv("entity_count", static_cast<std::size_t>(rep.entity_count));
  ac::proto_kv("no_game_handle", rep.no_game_handle);
  ac::proto_kv("ept_hide", rep.ept_hide);
  ac::proto_kv("timing_spoof", rep.timing_spoof);
  ac::proto_kv("attest_fail", rep.attest_fail);
  ac::proto_kv("read_ops", static_cast<std::size_t>(rep.read_ops));
  ac::proto_line("vendor", rep.vendor);
  ac::proto_line("bridge_device", radar.bridge_name());
  ac::proto_line("detail", rep.detail);
  std::printf("  %-28s %d (READ_VA)\n", "hypercall_op",
              static_cast<int>(T3_HC_READ_VA));

  t0_red::RadarUi ui;
  ui.set_title("t3-hv-radar");
  ui.update(radar.entities(), {});
  ac::proto_kv("blips", ui.blips().size());

  // Scenario C: FallbackChain with fixture path for degraded attach.
  t3_red::FallbackChain chain;
  auto hv = std::make_unique<t3_red::HvReadBackend>();
  // Fixture path self-activates HV for unit attach (no World required).
  chain.add(std::move(hv));
  chain.add(std::make_unique<t2_red::IoctlReadBackend>());
  chain.add(std::make_unique<t0_red::RpmBackend>());

  auto& fx = lab::global_fixture();
  ac::proto_status("fallback_attach_fixture",
                   chain.attach_first_available(fx.id()));
  if (chain.active()) {
    ac::proto_line("fallback_backend", chain.active()->name());
    // ac::to_string: Human-readable enum label for logs/tests.
    ac::proto_line("fallback_tier", ac::to_string(chain.active_tier()));
    t0_red::EntityPipeline pipe(*chain.active());
    ac::proto_status("fallback_refresh", pipe.refresh(fx.base_address()));
    ac::proto_kv("fallback_entities", pipe.entities().size());
  }

  // Degraded: no HV in chain → T2/T0 fixture attach.
  t3_red::FallbackChain degraded;
  degraded.add(std::make_unique<t2_red::IoctlReadBackend>());
  degraded.add(std::make_unique<t0_red::RpmBackend>());
  ac::proto_status("degraded_attach", degraded.attach_first_available(fx.id()));
  if (degraded.active()) {
    // ac::to_string: Human-readable enum label for logs/tests.
    ac::proto_line("degraded_tier", ac::to_string(degraded.active_tier()));
  }

  const bool pure_hv_ok =
      rep.hv_started && rep.entities_ok && rep.entity_count > 0 &&
      rep.no_game_handle && !radar.has_game_handle();
  ac::proto_kv("T3_RED_SCAR_OK", pure_hv_ok);
  std::puts(pure_hv_ok
                ? "prototype ok (full T3 pure HV path, no game handle)"
                : "prototype FAIL");
  std::puts(
      "\n[blue should counter] TrustPolicy(VBS/HVCI) + PlatformAc full + bridge");
  return pure_hv_ok ? 0 : 1;
}
