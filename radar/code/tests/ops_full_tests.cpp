// Ops full tests: theme-specific account/report/VPN/C2/input/overwatch scars.

#include "sim/world.hpp"
#include "strategies/crosscutting/account_graph/red_example.hpp"
#include "strategies/crosscutting/account_graph/blue_example.hpp"
#include "strategies/crosscutting/report_velocity/red_example.hpp"
#include "strategies/crosscutting/report_velocity/blue_example.hpp"
#include "strategies/crosscutting/vpn_proxy_graph/red_example.hpp"
#include "strategies/crosscutting/vpn_proxy_graph/blue_example.hpp"
#include "strategies/crosscutting/network_c2_intel/red_example.hpp"
#include "strategies/crosscutting/network_c2_intel/blue_example.hpp"
#include "strategies/crosscutting/ac_self_integrity/red_example.hpp"
#include "strategies/crosscutting/ac_self_integrity/blue_example.hpp"
#include "strategies/crosscutting/input_provenance/red_example.hpp"
#include "strategies/crosscutting/input_provenance/blue_example.hpp"
#include "strategies/crosscutting/overwatch_queue/red_example.hpp"
#include "strategies/crosscutting/overwatch_queue/blue_example.hpp"
#include "strategies/crosscutting/overwatch_multisignal/red_example.hpp"
#include "strategies/crosscutting/overwatch_multisignal/blue_example.hpp"

#include <cstdio>

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
  {
    auto w = sim::make_arena();
    auto rr = examples::account_graph::apply(w);
    expect(rr.achieved && rr.accounts >= 3, "account_graph accounts plant");
    expect(w.vpn_proxy_active, "account_graph vpn scar");
    auto br = examples::account_graph::detect(w);
    expect(br.detected && br.linked >= 2, "account_graph linked payment");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::report_velocity::apply(w);
    expect(rr.report_sum >= 50, "report_velocity elevated reports");
    auto br = examples::report_velocity::detect(w);
    expect(br.detected && br.high_report >= 2, "report_velocity cluster");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::vpn_proxy_graph::apply(w);
    expect(rr.achieved && w.vpn_proxy_active, "vpn graph red");
    auto br = examples::vpn_proxy_graph::detect(w);
    expect(br.detected, "vpn graph blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::network_c2_intel::apply(w);
    expect(rr.intel_nets >= 1, "c2 intel nets");
    auto br = examples::network_c2_intel::detect(w);
    expect(br.detected && br.handle_hit && br.intel_hit, "c2+handle not C2 alone");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::ac_self_integrity::apply(w);
    expect(rr.dirty >= 1, "ac integrity tamper");
    auto br = examples::ac_self_integrity::detect(w);
    expect(br.detected && br.mitigated, "ac integrity fail-closed");
    expect(w.ranked_access_denied, "ac integrity ranked deny");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::input_provenance::apply(w);
    expect(rr.injected >= 2 && w.raw_sendinput_mixed, "input multi sources");
    auto br = examples::input_provenance::detect(w);
    expect(br.detected, "input provenance blue");
  }
  {
    auto w = sim::make_arena();
    const auto build0 = w.binary_build_id;
    auto rr = examples::overwatch_queue::apply(w);
    expect(rr.achieved, "overwatch queue red mild");
    // Second weak scar must come from red, not blue inventing during detect.
    expect(w.binary_build_id != "shared" && !w.binary_build_id.empty(),
           "overwatch red planted second scar (unique build)");
    const auto build_after_red = w.binary_build_id;
    auto br = examples::overwatch_queue::detect(w);
    expect(br.detected && w.overwatch_queued, "overwatch queued");
    expect(br.mitigated, "overwatch multi-weak mitigate");
    expect(w.binary_build_id == build_after_red,
           "overwatch blue did not mutate build id");
    (void)build0;
  }
  {
    // Handle-only: blue must not invent second scar; mild single-signal path.
    auto w = sim::make_arena();
    const auto red = w.spawn("handle-only.exe");
    expect(w.open_process(red, w.game_pid(), sim::AccessMask::VmRead, false),
           "handle-only open");
    expect(w.binary_build_id == "shared", "handle-only baseline shared build");
    auto br = examples::overwatch_queue::detect(w);
    expect(w.binary_build_id == "shared",
           "handle-only blue did not invent unique build");
    expect(br.detected, "handle-only mild detect");
    expect(!br.mitigated, "handle-only no full multi-weak mitigate");
    expect(!w.overwatch_queued, "handle-only not fully queued");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::overwatch_multisignal::apply(w);
    expect(w.vpn_proxy_active && w.binary_build_id != "shared", "multisignal scars");
    auto br = examples::overwatch_multisignal::detect(w);
    expect(br.detected && w.overwatch_score >= 2, "multisignal score>=2");
  }

  if (fails) {
    std::fprintf(stderr, "ops_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("ops_full_tests: all passed\n");
  return 0;
}
