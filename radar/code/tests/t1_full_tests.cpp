// Full T1 tests: drive shipped red/blue APIs on sim::World — no reimplementation.

#include "sim/world.hpp"
#include "ac/telemetry.hpp"
#include "lab/fixture_process.hpp"
#include "t1_red/syscall_backend.hpp"
#include "t1_red/syscall_cheat.hpp"
#include "t1_red/staged_loader.hpp"
#include "t1_red/crypto_offsets.hpp"
#include "t1_red/offset_blob.hpp"
#include "t1_blue/hook_trap.hpp"
#include "t1_blue/staging_detector.hpp"
#include "t1_blue/staging_watch.hpp"
#include "t1_blue/syscall_aware_monitor.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "strategies/t1/indirect_syscall/red_example.hpp"
#include "strategies/t1/indirect_syscall/blue_example.hpp"
#include "strategies/t1/manual_map_hide/red_example.hpp"
#include "strategies/t1/manual_map_hide/blue_example.hpp"
#include "strategies/t1/parent_lineage/red_example.hpp"
#include "strategies/t1/parent_lineage/blue_example.hpp"
#include "strategies/t1/stack_spoof/red_example.hpp"
#include "strategies/t1/stack_spoof/blue_example.hpp"
#include "strategies/t1/etw_blind/red_example.hpp"
#include "strategies/t1/etw_blind/blue_example.hpp"
#include "strategies/t1/process_hollow/red_example.hpp"
#include "strategies/t1/process_hollow/blue_example.hpp"
#include "strategies/t1/inmatch_only/red_example.hpp"
#include "strategies/t1/inmatch_only/blue_example.hpp"
#include "strategies/t1/module_stomp/red_example.hpp"
#include "strategies/t1/module_stomp/blue_example.hpp"
#include "strategies/t1/mapper_artifact/red_example.hpp"
#include "strategies/t1/mapper_artifact/blue_example.hpp"
#include "strategies/t1/raw_vs_sendinput/red_example.hpp"
#include "strategies/t1/raw_vs_sendinput/blue_example.hpp"
#include "strategies/t1/sedebug_priv/red_example.hpp"
#include "strategies/t1/sedebug_priv/blue_example.hpp"
#include "strategies/t1/speedhack_timescale/red_example.hpp"
#include "strategies/t1/speedhack_timescale/blue_example.hpp"
#include "strategies/t1/thread_hide_dbg/red_example.hpp"
#include "strategies/t1/thread_hide_dbg/blue_example.hpp"
// Note: strategy::t1::{Red,Blue} types are reused by multiple pairs
// (etw_dual_provider, bsecure_allowed_evade, ret_addr_spoof) and cannot
// share one TU. Those are driven via strategy_lab IDs 116/118/121.
#include "strategies/t1/vac_handle_enum/red_example.hpp"
#include "strategies/t1/vac_handle_enum/blue_example.hpp"
#include "strategies/t1/thread_monitor_evade/red_example.hpp"
#include "strategies/t1/thread_monitor_evade/blue_example.hpp"
#include "sim/narrative.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>

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
  // ── SyscallBackend fixture + world ───────────────────────────────────
  {
    t1_red::SyscallBackend be;
    expect(be.attach_fixture(lab::global_fixture().id()) == ac::Status::Ok,
           "fixture attach");
    expect(be.bypasses_usermode_api_hooks(), "bypasses hooks");
    expect(!be.exposes_usermode_handle(), "fixture no handle");
    auto rr = be.read({lab::global_fixture().base_address(), 4});
    expect(rr.status == ac::Status::Ok, "fixture read");

    auto w = sim::make_arena();
    auto reader = w.spawn("syscall-reader.exe");
    expect(be.attach_world(w, reader, w.game_pid()) == ac::Status::Ok,
           "world attach");
    expect(be.exposes_usermode_handle(), "world exposes handle");
    expect(be.handle_count() >= 1, "syscall handle count");
    // Verify via_syscall_path on the handle
    bool syscall = false;
    for (const auto& h : w.handles_to(w.game_pid(), true)) {
      if (h.owner_pid == reader && h.via_syscall_path) {
        syscall = true;
      }
    }
    expect(syscall, "via_syscall_path scar");
    auto* g = w.proc(w.game_pid());
    auto r2 = be.read({g->base, 4});
    expect(r2.status == ac::Status::Ok, "world syscall RPM read");
    be.detach();
    expect(be.handle_count() == 0, "detach closes");
  }

  // ── Staged loader multi-step ─────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t1_red::StagedLoader ld;
    auto rep = ld.run_on_world(w, "lab-token", {0x90, 0xC3}, "soft-radar.exe");
    expect(rep.state == t1_red::StageState::PayloadMapped, "stage mapped");
    expect(rep.private_rx && rep.short_lived_stub, "rx+stub scars");
    expect(rep.stub_pid != 0 && rep.payload_pid != 0, "stub and payload pids");
    expect(ld.authenticate_lab("") == ac::Status::Denied, "empty token denied");
  }

  // ── Crypto offsets ───────────────────────────────────────────────────
  {
    t1_red::CryptoOffsets off;
    auto sealed = t1_red::CryptoOffsets::seal({{"entity", 0x10}}, 0x3C);
    expect(off.open(sealed, 0x3C) == ac::Status::Ok, "crypto open");
    expect(off.get("entity") == 0x10, "crypto get");
  }

  // ── SyscallCheat full loop ───────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t1_red::SyscallCheat c(w);
    auto rep = c.run_full_loop(true, true);
    expect(rep.attached && rep.via_syscall, "full_loop attach syscall");
    expect(rep.entity_count >= 1, "entities");
    expect(rep.syscall_handles >= 1, "syscall handles");
    expect(rep.read_ops >= 1, "real reads");
    expect(rep.staged && rep.private_rx, "staging scars");
    expect(rep.stack_spoof || rep.etw_blind, "read evasions");
  }

  // ── Hook trap blind, handle truth sees ───────────────────────────────
  {
    auto w = sim::make_arena();
    t1_red::SyscallCheat c(w);
    c.attach_via_syscall();
    c.pull_entities();
    t1_blue::UsermodeHookTrap hooks;
    t1_blue::HandleTruthMonitor truth;
    auto game = w.game_pid();
    auto hr = hooks.analyze(w, game);
    auto tr = truth.analyze(w, game);
    expect(hr.blind_to_syscall_red || hr.visible_winapi_opens == 0,
           "hooks blind or no winapi");
    expect(tr.foreign_vm_read >= 1 && tr.syscall_path >= 1,
           "truth sees syscall VM_READ");
  }

  // ── T1Agent full_scan ────────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t1_red::SyscallCheat c(w);
    c.run_full_loop(true, true);
    ac::MemoryTelemetrySink sink;
    t1_blue::T1Agent agent(w, sink);
    auto d = agent.full_scan();
    expect(d.handle_truth_hit, "agent handle truth");
    expect(d.hooks_blind, "agent hooks blind");
    expect(d.syscall_handles >= 1, "agent syscall handles");
    expect(!d.reasons.empty() && d.risk > 0, "reasons+risk");
    expect(d.staging_hit, "staging hit");
  }

  // ── Fail path: no attach ─────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t1_red::SyscallCheat c(w);
    expect(!c.pull_entities(), "pull without attach fails");
  }

  // ── Strategy pairs ───────────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::indirect_syscall::apply(w);
    expect(rr.achieved, "strategy indirect_syscall red");
    auto br = examples::indirect_syscall::detect(w);
    expect(br.detected, "strategy indirect_syscall blue");
    expect(br.mitigated, "hooks blind + handle hit");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::manual_map_hide::apply(w);
    expect(rr.achieved, "manual_map red");
    auto br = examples::manual_map_hide::detect(w);
    expect(br.detected, "manual_map blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::parent_lineage::apply(w);
    expect(rr.achieved, "parent_lineage red");
    auto br = examples::parent_lineage::detect(w);
    expect(br.detected, "parent_lineage blue");
  }
  {
    auto w = sim::make_arena();
    // stack_spoof / etw may use run_red API
    // Try apply-style first via detecting headers
  }

  // stack_spoof, etw_blind, process_hollow, inmatch_only, module_stomp — call
  // if they use apply/detect or run_red/run_blue
  {
    auto w = sim::make_arena();
    auto rr = examples::stack_spoof::apply(w);
    expect(rr.achieved, "stack_spoof red");
    auto br = examples::stack_spoof::detect(w);
    expect(br.detected, "stack_spoof blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::etw_blind::apply(w);
    expect(rr.achieved, "etw_blind red");
    auto br = examples::etw_blind::detect(w);
    expect(br.detected, "etw_blind blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::process_hollow::apply(w);
    expect(rr.achieved, "process_hollow red");
    auto br = examples::process_hollow::detect(w);
    expect(br.detected, "process_hollow blue");
  }
  {
    auto w = sim::make_arena();
    w.match_active = true;
    auto rr = examples::inmatch_only::apply(w);
    expect(rr.achieved, "inmatch_only red");
    auto br = examples::inmatch_only::detect(w);
    expect(br.detected, "inmatch_only blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::module_stomp::apply(w);
    expect(rr.achieved, "module_stomp red");
    auto br = examples::module_stomp::detect(w);
    expect(br.detected, "module_stomp blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::mapper_artifact::apply(w);
    expect(rr.achieved, "mapper_artifact red");
    expect(rr.steps >= 3, "mapper_artifact multi-step");
    auto br = examples::mapper_artifact::detect(w);
    expect(br.detected, "mapper_artifact blue");
    expect(br.signals >= 2, "mapper_artifact multi-reason");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::raw_vs_sendinput::apply(w);
    expect(rr.achieved, "raw_vs_sendinput red");
    expect(rr.steps >= 3, "raw_vs_sendinput multi-step");
    auto br = examples::raw_vs_sendinput::detect(w);
    expect(br.detected, "raw_vs_sendinput blue");
    expect(br.signals >= 2, "raw_vs_sendinput multi-reason");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::sedebug_priv::apply(w);
    expect(rr.achieved, "sedebug_priv red");
    expect(rr.steps >= 3, "sedebug_priv multi-step");
    auto br = examples::sedebug_priv::detect(w);
    expect(br.detected, "sedebug_priv blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::speedhack_timescale::apply(w);
    expect(rr.achieved, "speedhack_timescale red");
    expect(rr.steps >= 3, "speedhack_timescale multi-step");
    auto br = examples::speedhack_timescale::detect(w);
    expect(br.detected, "speedhack_timescale blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::thread_hide_dbg::apply(w);
    expect(rr.achieved, "thread_hide_dbg red");
    expect(rr.steps >= 3, "thread_hide_dbg multi-step");
    auto br = examples::thread_hide_dbg::detect(w);
    expect(br.detected, "thread_hide_dbg blue");
    expect(br.signals >= 2, "thread_hide_dbg multi-reason");
  }
  // Catalogued free-function pairs (102/103). Class-based strategy::t1 pairs
  // 116/118/121 are verified via strategy_lab (shared Red/Blue type names).
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    expect(vac_handle_red_apply(w, n) == ac::Status::Ok, "vac_handle multi-step red");
    expect(vac_handle_blue_detect(w, n), "vac_handle multi-reason blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    expect(thread_monitor_red_apply(w, n), "thread_monitor multi-step red");
    expect(w.diagnostic_state.thread_start_obfuscated, "thread_monitor residual");
    expect(thread_monitor_blue_detect(w, n), "thread_monitor multi-reason blue");
  }

  if (fails) {
    std::fprintf(stderr, "t1_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("t1_full_tests: all passed\n");
  return 0;
}
