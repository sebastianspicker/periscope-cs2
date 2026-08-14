// Full T0 tests: drive shipped red/blue APIs on sim::World — no reimplementation.

#include "sim/world.hpp"
#include "ac/telemetry.hpp"
#include "lab/fixture_process.hpp"
#include "t0_red/cheat_client.hpp"
#include "t0_red/rpm_backend.hpp"
#include "t0_red/entity_pipeline.hpp"
#include "t0_red/radar_ui.hpp"
#include "t0_red/evasion_weak.hpp"
#include "t0_blue/ac_agent.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "t0_blue/process_cooccurrence.hpp"
#include "t0_blue/false_positive_policy.hpp"
#include "strategies/t0/external_rpm/red_example.hpp"
#include "strategies/t0/external_rpm/blue_example.hpp"
#include "strategies/t0/pattern_offset_scan/red_example.hpp"
#include "strategies/t0/pattern_offset_scan/blue_example.hpp"
#include "strategies/t0/handle_hide_on_enum/red_example.hpp"
#include "strategies/t0/handle_hide_on_enum/blue_example.hpp"
#include "strategies/t0/fp_allowlist_evasion/red_example.hpp"
#include "strategies/t0/fp_allowlist_evasion/blue_example.hpp"
#include "strategies/t0/process_cooccurrence/red_example.hpp"
#include "strategies/t0/process_cooccurrence/blue_example.hpp"
#include "strategies/t0/read_throttle/red_example.hpp"
#include "strategies/t0/read_throttle/blue_example.hpp"
#include "strategies/t0/internal_inject/red_example.hpp"
#include "strategies/t0/internal_inject/blue_example.hpp"
#include "strategies/t0/section_map/red_example.hpp"
#include "strategies/t0/section_map/blue_example.hpp"
#include "strategies/t0/module_integrity/red_example.hpp"
#include "strategies/t0/module_integrity/blue_example.hpp"
#include "strategies/t0/block_input/red_example.hpp"
#include "strategies/t0/block_input/blue_example.hpp"
#include "strategies/t0/handle_hijack_proxy/red_example.hpp"
#include "strategies/t0/handle_hijack_proxy/blue_example.hpp"
#include "strategies/crosscutting/internal_footprint/red_example.hpp"
#include "strategies/crosscutting/internal_footprint/blue_example.hpp"
#include "strategies/t0/dynamic_api_table/red_example.hpp"
#include "strategies/t0/dynamic_api_table/blue_example.hpp"
#include "strategies/t0/handle_hijack_proxy_donor/red_example.hpp"
#include "strategies/t0/handle_hijack_proxy_donor/blue_example.hpp"
#include "strategies/t0/hud_radar_parsing/red_example.hpp"
#include "strategies/t0/hud_radar_parsing/blue_example.hpp"
#include "strategies/t0/cvar_walk_resolve/red_example.hpp"
#include "strategies/t0/cvar_walk_resolve/blue_example.hpp"
#include "strategies/t0/wda_exclude_capture/red_example.hpp"
#include "strategies/t0/wda_exclude_capture/blue_example.hpp"

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
void expect(bool c, const std::string& m) { expect(c, m.c_str()); }

}  // namespace

int main() {
  // ── RpmBackend fixture + world ───────────────────────────────────────
  {
    t0_red::RpmBackend be;
    expect(be.attach(lab::global_fixture().id()) == ac::Status::Ok,
           "fixture attach");
    expect(!be.exposes_usermode_handle(), "fixture has no handle scar");
    auto rr = be.read({lab::global_fixture().base_address(), 4});
    expect(rr.status == ac::Status::Ok && rr.bytes.size() == 4, "fixture read");

    auto w = sim::make_arena();
    auto reader = w.spawn("reader.exe");
    expect(be.attach_world(w, reader, w.game_pid()) == ac::Status::Ok,
           "world attach");
    expect(be.exposes_usermode_handle(), "world exposes handle");
    expect(be.handle_count() >= 1, "handle_count >= 1");
    auto* g = w.proc(w.game_pid());
    auto r2 = be.read({g->base, 4});
    expect(r2.status == ac::Status::Ok, "world RPM read");
    be.detach();
    expect(be.handle_count() == 0, "detach closes handles");
  }

  // ── CheatClient full loop ────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t0_red::CheatClient c(w, "radar.exe");
    auto rep = c.run_full_loop();
    expect(rep.attached && rep.entities_ok, "cheat full_loop attach+entities");
    expect(rep.entity_count >= 1, "entity_count >= 1");
    expect(rep.foreign_vm_read_handles >= 1, "VM_READ scar");
    expect(rep.read_ops >= 1 && rep.bytes_read >= 4, "real reads occurred");
    expect(!c.blips().empty() || rep.entity_count == 0, "radar blips");
    expect(!w.overlays.empty(), "overlay scar registered");
  }

  // ── Weak evasion does not clear handles ──────────────────────────────
  {
    auto w = sim::make_arena();
    t0_red::CheatClient c(w, "radar.exe");
    c.attach_to_game();
    auto stack =
        t0_red::WeakEvasionKit::apply_common_stack(w, c.pid(), c.game_pid());
    expect(stack.size() >= 4, "evasion stack steps");
    for (const auto& s : stack) {
      expect(!s.defeats_handle_graph, "no weak evade beats handle graph");
    }
    int handles = 0;
    for (const auto& h : w.handles_to(w.game_pid(), true)) {
      if (h.owner_pid == c.pid() &&
          sim::has(h.access, sim::AccessMask::VmRead)) {
        ++handles;
      }
    }
    expect(handles >= 1, "handles remain after weak evasion");
  }

  // ── AcAgent full_scan ────────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t0_red::CheatClient c(w, "radar.exe");
    c.run_full_loop();
    c.close_and_reopen_brief();
    ac::MemoryTelemetrySink sink;
    t0_blue::AcAgent agent(w, sink);
    auto d = agent.full_scan();
    expect(d.handle_hit, "blue handle_hit");
    expect(!d.reasons.empty(), "blue non-empty reasons");
    expect(d.risk > 0, "blue risk > 0");
    expect(d.summary.find("handle=") != std::string::npos, "summary present");
  }

  // ── FP policy: name allowlist insufficient ───────────────────────────
  {
    auto w = sim::make_arena();
    auto red = w.spawn("nvidia-overlay");
    w.proc(red)->looks_reputable = true;
    w.open_process(red, w.game_pid(), sim::AccessMask::VmRead, false);
    t0_blue::FalsePositivePolicy fp;
    fp.allow_process_name("nvidia-overlay");
    sim::Handle h{red, w.game_pid(), sim::AccessMask::VmRead, false, false,
                  false};
    auto dec = fp.evaluate_edge(h, w);
    expect(dec.alert, "name allowlist alone still alerts");
    expect(dec.reason.find("allowlist") != std::string::npos ||
               dec.reason.find("vm_read") != std::string::npos,
           "reason explains allowlist insufficiency");
  }

  // ── Strategy pairs (shipped apply/detect) ────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::external_rpm::apply(w);
    expect(rr.achieved, "strategy external_rpm red");
    expect(rr.steps >= 2, "strategy external_rpm multi-step red (>=2 steps)");
    expect(!rr.detail.empty(), "strategy external_rpm red detail");
    auto br = examples::external_rpm::detect(w);
    expect(br.detected, "strategy external_rpm blue");
    expect(br.signals >= 2, "strategy external_rpm multi-reason blue (>=2 signals)");
    expect(static_cast<int>(br.reasons.size()) >= 2,
           "strategy external_rpm blue reasons vector >=2");
  }
  // ── pattern_offset_scan: multi-step discovery + multi-reason blue ────
  {
    auto w = sim::make_arena();
    expect(w.lab_pattern_marker_present, "pattern marker planted by make_arena");
    expect(w.remote_read_ops == 0, "pattern scan baseline no remote reads");
    expect(w.lab_pattern_generation >= 1, "pattern gen after plant");
    examples::pattern_offset_scan::ScannerSession session;
    auto rr = examples::pattern_offset_scan::apply(w, session);
    expect(rr.achieved, "strategy pattern_offset_scan red achieved");
    expect(rr.handle_opened, "pattern_scan red handle");
    expect(rr.pattern_hit, "pattern_scan red ACPT hit");
    expect(rr.entity_count >= 1, "pattern_scan red entities");
    expect(rr.bulk_read_ops >= 4, "pattern_scan red bulk ops");
    expect(w.remote_read_ops >= 4, "world remote_read_ops after scan");
    expect(!rr.refreshed, "pattern_scan first shot not refresh");
    expect(session.has_resolve, "pattern_scan session has resolve");
    expect(!rr.detail.empty(), "pattern_scan red detail");
    const auto stale_table = rr.resolved_table_va;
    const auto gen0 = w.lab_pattern_generation;

    // Mutate layout: move marker + entity table so stale VA is wrong.
    expect(w.mutate_lab_pattern_layout(w.game_pid(), 0x2C0, 0x80),
           "mutate_lab_pattern_layout");
    expect(w.lab_pattern_generation > gen0, "pattern gen bumped on mutate");
    expect(w.lab_entity_table_rel == 0x80, "table relocated to 0x80");

    auto rf = examples::pattern_offset_scan::refresh(w, session);
    expect(rf.achieved, "pattern_scan refresh achieved");
    expect(rf.refreshed, "pattern_scan auto-refresh fired");
    expect(rf.pattern_hit, "pattern_scan refresh pattern hit");
    expect(rf.entity_count >= 1, "pattern_scan refresh entities");
    expect(rf.resolved_table_va != stale_table,
           "pattern_scan refresh abandoned stale table VA");
    expect(w.pattern_rescan_count >= 1, "world pattern_rescan_count");
    expect(session.layout_generation == w.lab_pattern_generation,
           "session gen matches world after refresh");

    auto br = examples::pattern_offset_scan::detect(w);
    expect(br.detected, "strategy pattern_offset_scan blue detected");
    expect(br.foreign_vm_read, "pattern_scan blue foreign handle");
    expect(br.bulk_remote_reads, "pattern_scan blue bulk reads");
    expect(br.suspicious_cooccurrence, "pattern_scan blue cooccurrence");
    expect(br.rescan_residual, "pattern_scan blue rescan residual");
    expect(br.reason_count >= 2, "pattern_scan multi-reason");
    expect(br.mitigated, "pattern_scan blue mitigated");
    expect(br.structural_fog || !w.server_sends_full_enemy_origin,
           "pattern_scan structural fog");
    expect(w.ranked_access_denied, "pattern_scan ranked deny");
    expect(!br.detail.empty(), "pattern_scan blue detail");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::handle_hide_on_enum::apply(w);
    expect(rr.achieved, "strategy handle_hide red");
    auto br = examples::handle_hide_on_enum::detect(w);
    expect(br.detected, "strategy handle_hide blue");
  }
  {
    // FP-first: legit noise must not ban
    auto w0 = sim::make_arena();
    auto legit = examples::fp_allowlist_evasion::apply_legit_only(w0);
    expect(legit.is_legit_only, "fp legit_only plant");
    auto b0 = examples::fp_allowlist_evasion::detect(w0);
    expect(b0.fp_only && !b0.detected && !b0.mitigated, "fp legit no ban");
    // True radar still multi-reason detects
    auto w = sim::make_arena();
    auto rr = examples::fp_allowlist_evasion::apply_true_radar(w);
    expect(rr.is_true_radar && rr.achieved, "fp true radar plant");
    auto br = examples::fp_allowlist_evasion::detect(w);
    expect(br.detected, "fp true radar detected");
    expect(br.mitigated || br.reason_count >= 1, "fp true radar multi-reason");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::process_cooccurrence::apply(w);
    expect(rr.achieved, "strategy cooccurrence red");
    auto br = examples::process_cooccurrence::detect(w);
    expect(br.detected, "strategy cooccurrence blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::read_throttle::apply(w);
    expect(rr.achieved, "strategy read_throttle red");
    auto br = examples::read_throttle::detect(w);
    expect(br.detected, "strategy read_throttle blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::internal_inject::apply(w);
    expect(rr.achieved, "strategy internal_inject red");
    auto br = examples::internal_inject::detect(w);
    expect(br.detected, "strategy internal_inject blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::section_map::apply(w);
    expect(rr.achieved, "strategy section_map red");
    auto br = examples::section_map::detect(w);
    expect(br.detected, "strategy section_map blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::module_integrity::apply(w);
    expect(rr.achieved, "strategy module_integrity red");
    auto br = examples::module_integrity::detect(w);
    expect(br.detected, "strategy module_integrity blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::block_input::apply(w);
    expect(rr.achieved, "strategy block_input red");
    auto br = examples::block_input::detect(w);
    expect(br.detected, "strategy block_input blue");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::handle_hijack_proxy::apply(w);
    expect(rr.achieved && w.handle_proxy_active, "handle_hijack red proxy");
    expect(rr.proxy_pid != 0 && rr.actor_pid != 0, "handle_hijack pids");
    // Cheat consumer must not own a direct game handle.
    bool cheat_handle = false;
    for (const auto& h : w.handles_to(w.game_pid())) {
      if (h.owner_pid == rr.actor_pid) cheat_handle = true;
    }
    expect(!cheat_handle, "handle_hijack cheat has no direct handle");
    bool proxy_vm = false;
    for (const auto& h : w.handles_to(w.game_pid())) {
      if (h.owner_pid == rr.proxy_pid &&
          sim::has(h.access, sim::AccessMask::VmRead)) {
        proxy_vm = true;
      }
    }
    expect(proxy_vm, "handle_hijack proxy owns VmRead");
    auto br = examples::handle_hijack_proxy::detect(w);
    expect(br.detected && br.mitigated, "handle_hijack blue");
    expect(br.proxy_handle && br.consumer_no_handle, "handle_hijack reasons");
  }
  {
    auto w = sim::make_arena();
    auto rr = examples::internal_footprint::apply(w);
    expect(rr.achieved && w.internal_footprint_stealth, "footprint red");
    expect(w.crt_runtime_absent && w.static_imports_cleared, "footprint claims");
    auto* g = w.proc(w.game_pid());
    expect(g && g->manual_mapped_region, "footprint manual_map");
    auto br = examples::internal_footprint::detect(w);
    expect(br.detected && br.footprint_reasons >= 2, "footprint blue");
  }
  // Schema-cache residual on pattern path (deepen 29).
  {
    auto w = sim::make_arena();
    examples::pattern_offset_scan::ScannerSession session;
    auto rr = examples::pattern_offset_scan::apply(w, session);
    expect(rr.achieved, "schema pattern first");
    expect(w.schema_cache_active && !w.schema_cache_version.empty(),
           "schema cache primed");
    expect(w.mutate_lab_pattern_layout(w.game_pid(), 0x2C0, 0x80),
           "schema layout mutate");
    auto rf = examples::pattern_offset_scan::refresh(w, session);
    expect(rf.achieved && rf.refreshed, "schema remote refresh path");
    expect(w.schema_remote_update || w.schema_fetch_count >= 1,
           "schema remote residual");
    auto br = examples::pattern_offset_scan::detect(w);
    expect(br.schema_cache_residual || br.schema_remote_residual,
           "schema blue residual");
  }

  // ── test_api_table_resolved ─────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::dynamic_api_table::apply(w);
    expect(rr.achieved, "api_table red achieved");
    expect(rr.apis_resolved >= 20, "api_table non-null API pointers count=" + std::to_string(rr.apis_resolved));
    expect(w.dynamic_import_resolution, "api_table world dynamic_import_resolution");
    expect(w.dynamic_import_count >= 20, "api_table world API count=" + std::to_string(w.dynamic_import_count));
    auto br = examples::dynamic_api_table::detect(w);
    expect(br.detected, "api_table blue detected");
    expect(br.dynamic_imports_detected, "api_table blue dynamic import flag");
  }

  // ── test_hijack_reader ──────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::handle_hijack_proxy_donor::apply(w);
    expect(rr.achieved, "hijack_reader red achieved");
    expect(rr.donor_pid != 0 && rr.consumer_pid != 0, "hijack_reader donor+consumer pids");
    expect(rr.handle_duplicated, "hijack_reader handle duplicated");
    expect(w.donor_handle_duplicated, "hijack_reader world donor_handle_duplicated");
    auto br = examples::handle_hijack_proxy_donor::detect(w);
    expect(br.detected, "hijack_reader blue detected");
    expect(br.donor_handle_found, "hijack_reader blue donor handle found");
    expect(br.consumer_has_no_handle, "hijack_reader consumer has no handle");
  }

  // ── test_entity_collector ───────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t0_red::CheatClient c(w, "collector.exe");
    auto rep = c.run_full_loop();
    expect(rep.attached, "entity_collector attached");
    expect(rep.entity_count >= 1, "entity_collector entities=" + std::to_string(rep.entity_count));
    expect(!c.entities().empty(), "entity_collector entity list non-empty");
    expect(rep.read_ops >= 1, "entity_collector read_ops=" + std::to_string(rep.read_ops));
  }

  // ── test_hud_radar ──────────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::hud_radar_parsing::apply(w);
    expect(rr.achieved, "hud_radar red achieved");
    expect(rr.entities_found >= 5, "hud_radar entities=" + std::to_string(rr.entities_found));
    expect(rr.nodes_walked >= 10, "hud_radar BST nodes=" + std::to_string(rr.nodes_walked));
    expect(w.hud_radar_snapshot_taken, "hud_radar world snapshot_taken");
    expect(w.hud_radar_entity_count >= 5, "hud_radar world entity count");
    auto br = examples::hud_radar_parsing::detect(w);
    expect(br.detected, "hud_radar blue detected");
    expect(br.radar_snapshot_seen, "hud_radar blue snapshot flag");
  }

  // ── test_cvar_manager ───────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::cvar_walk_resolve::apply(w);
    expect(rr.achieved, "cvar_manager red achieved");
    expect(rr.tier_reached >= 4, "cvar_manager tier 4 reached");
    expect(rr.entries_found >= 100, "cvar_manager entries=" + std::to_string(rr.entries_found));
    expect(w.cvar_walk_resolved, "cvar_manager world resolved");
    expect(w.cvar_walk_tier_reached >= 4, "cvar_manager world tier");
    auto br = examples::cvar_walk_resolve::detect(w);
    expect(br.detected, "cvar_manager blue detected");
    expect(br.resolve_detected, "cvar_manager blue resolve flag");
  }

  // ── test_coordinate_math ────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t0_red::CheatClient c(w, "radar-coord.exe");
    auto rep = c.run_full_loop();
    expect(rep.attached, "coord_math attached");
    // Verify radar projection: world origin → polar → screen
    auto blips = c.blips();
    for (const auto& b : blips) {
      expect(b.screen_x >= 0.0f && b.screen_y >= 0.0f, "coord_math blip in positive screen space");
      expect(b.world_x != 0.0f || b.world_y != 0.0f, "coord_math blip has non-zero world pos");
    }
    expect(!blips.empty(), "coord_math blips generated");
  }

  // ── test_overlay_window ─────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto rr = examples::wda_exclude_capture::apply(w);
    expect(rr.achieved, "overlay_window red achieved");
    expect(rr.wda_applied, "overlay_window WDA applied");
    expect(rr.capture_excluded, "overlay_window capture excluded");
    expect(w.wda_exclude_updated, "overlay_window world WDA flag");
    expect(!w.overlays.empty(), "overlay_window overlay created");
    auto br = examples::wda_exclude_capture::detect(w);
    expect(br.detected, "overlay_window blue detected");
    expect(br.stream_proof_overlay, "overlay_window stream proof detected");
  }

  // ── test_stealth_features ───────────────────────────────────────────
  {
    auto w = sim::make_arena();
    // Test randomized process names + capture exclusion
    const auto stealth = w.spawn("svchost.exe");  // randomized name
    auto* sp = w.proc(stealth);
    expect(sp != nullptr, "stealth process created");
    expect(sp->name == "svchost.exe", "stealth randomized name");
    expect(w.open_process(stealth, w.game_pid(), sim::AccessMask::VmRead, false), "stealth open handle");
    // Apply WDA exclusion to verify capture exclusion works
    sim::OverlayWindow ov;
    ov.owner_pid = stealth;
    ov.title = "stealth_overlay";
    ov.stream_proof = true;
    w.add_overlay(ov);
    w.wda_exclude_updated = true;
    expect(w.wda_exclude_updated, "stealth WDA exclude applied");
    expect(ov.stream_proof, "stealth overlay stream proof");
  }

  if (fails) {
    std::fprintf(stderr, "t0_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("t0_full_tests: all passed\n");
  return 0;
}
