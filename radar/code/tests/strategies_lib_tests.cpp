// strategies_lib_tests.cpp — drives shipped code/lib/strategies APIs end-to-end.
// Plants real World scars, then asserts sensors / multi-reason / pair_util /
// catalog_util observe them. No re-implementation of inventory logic.

#include "strategies/catalog_util.hpp"
#include "strategies/framework.hpp"
#include "strategies/multi_reason.hpp"
#include "strategies/pair_util.hpp"
#include "strategies/scar_sensors.hpp"
#include "strategies/strategy_support.hpp"
#include "strategies/support_api.hpp"

#include "sim/world.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++fails;
  } else {
    std::printf("ok: %s\n", msg);
  }
}

// Minimal pair run for catalog_util validate / filter tests.
strategies::StrategyResult dummy_run(sim::World&, sim::Narrator&) {
  strategies::StrategyResult r;
  r.summary = "dummy";
  return r;
}

strategies::StrategyEntry make_entry(const char* id, strategies::Family fam,
                                     const char* tiers) {
  strategies::StrategyEntry e;
  e.meta.id = id;
  e.meta.title = id;
  e.meta.family = fam;
  e.meta.tiers = tiers;
  e.meta.red_one_liner = "red";
  e.meta.blue_one_liner = "blue";
  e.run = &dummy_run;
  return e;
}

}  // namespace

int main() {
  // ── 1. library anchor ──────────────────────────────────────────────────
  {
    expect(strategies::support_detail::library_abi_version() >= 1,
           "library_abi_version >= 1");
  }

  // ── 2. family_name / iequals (framework_meta) ──────────────────────────
  {
    expect(std::string(strategies::family_name(strategies::Family::Delivery)) ==
               "Delivery",
           "family_name(Delivery)");
    expect(std::string(strategies::family_name(strategies::Family::Structural)) ==
               "Structural",
           "family_name(Structural)");
    expect(strategies::iequals("Evasion", "evasion"), "iequals case-insensitive");
    expect(!strategies::iequals("T0", "T1"), "iequals mismatch");
  }

  // ── 3. pair_util on clean arena ────────────────────────────────────────
  {
    auto w = sim::make_arena();
    const auto game = strategies::find_game(w);
    expect(game != 0, "find_game non-zero on arena");
    expect(!strategies::any_vm_read(w, game), "clean arena has no foreign VM_READ");
    expect(strategies::count_foreign_vm_read_owners(w, game) == 0,
           "count_foreign_vm_read_owners == 0 clean");
  }

  // ── 4. plant foreign VM_READ and detect via sensors + pair_util ────────
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    const auto actor = w.spawn("strategies-lib-actor.exe");
    expect(actor != 0, "spawn actor");
    expect(w.open_process(actor, game, sim::AccessMask::VmRead, false),
           "open_process VM_READ");

    expect(strategies::any_vm_read(w, game), "any_vm_read after open");
    expect(strategies::sensors::has_foreign_vm_read(w),
           "sensors::has_foreign_vm_read");
    expect(strategies::sensors::count_foreign_vm_read(w) >= 1,
           "count_foreign_vm_read >= 1");

    const auto hits = strategies::sensors::list_foreign_vm_read(w);
    expect(!hits.empty(), "list_foreign_vm_read non-empty");
    expect(hits[0].owner_pid == actor, "foreign handle owner is actor");

    int entity_count = -1;
    expect(strategies::open_and_read_entity_count(w, actor, game, &entity_count),
           "open_and_read_entity_count succeeds");
    expect(entity_count >= 0, "entity_count >= 0");

    // Direct entity_count_via_handle after handle exists.
    const int via = strategies::entity_count_via_handle(w, actor, game);
    expect(via >= 0, "entity_count_via_handle >= 0");

    // A short backing store cannot be decoded as a partial four-byte count.
    w.proc(game)->memory.resize(2);
    expect(strategies::entity_count_via_handle(w, actor, game) == -1,
           "entity_count_via_handle rejects truncated count");
  }

  // ── 5. driver / device sensors ─────────────────────────────────────────
  {
    auto w = sim::make_arena();
    sim::Driver d;
    d.name = "vuln-lab.sys";
    d.provides_mem_rw = true;
    d.byovd_known_bad = true;
    d.is_ac = false;
    d.sha256 = "deadbeef";
    w.drivers.push_back(d);

    sim::Device dev;
    dev.name = "\\\\.\\VulnLab";
    dev.owner_driver = "vuln-lab.sys";
    dev.mem_rw_ioctl = true;
    w.devices.push_back(dev);

    expect(strategies::sensors::has_untrusted_memrw_driver(w),
           "has_untrusted_memrw_driver");
    expect(strategies::sensors::has_known_bad_byovd(w), "has_known_bad_byovd");
    expect(strategies::sensors::has_memrw_device(w), "has_memrw_device");
    expect(strategies::sensors::has_device_named(w, "\\\\.\\VulnLab", true),
           "has_device_named require memrw");
    expect(strategies::sensors::has_driver_named(w, "vuln-lab.sys"),
           "has_driver_named");
    expect(examples::support::has_device(w, "\\\\.\\VulnLab"),
           "examples::support::has_device");
    expect(examples::support::has_untrusted_memrw_driver(w),
           "examples::support::has_untrusted_memrw_driver");
  }

  // ── 6. full inventory + inventory_to_blue ──────────────────────────────
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    const auto actor = w.spawn("inv-actor.exe");
    w.open_process(actor, game, sim::AccessMask::VmRead, false);
    w.physmem_device_open = true;
    w.trust.personal_hv_active = true;
    w.trust.dma_device_present = true;
    w.trust.iommu_on = false;
    w.etw_enabled = false;
    w.silent_aim_active = true;
    w.vpn_proxy_active = true;

    const auto inv = strategies::sensors::inventory(w);
    expect(inv.total_scars >= 5, "inventory total_scars >= 5");
    expect(!inv.foreign_handles.empty(), "inventory foreign_handles");
    expect(inv.personal_hv, "inventory personal_hv");
    expect(inv.dma_device, "inventory dma_device");
    expect(inv.iommu_off, "inventory iommu_off");
    expect(inv.etw_blind, "inventory etw_blind");
    expect(!inv.summary.empty(), "inventory summary non-empty");

    const auto blue = strategies::sensors::inventory_to_blue(inv);
    expect(blue.signals >= 4, "inventory_to_blue signals >= 4");
    expect(blue.detected, "inventory_to_blue detected");
    expect(blue.risk > 0.0, "inventory_to_blue risk > 0");
    expect(!blue.reasons.empty(), "inventory_to_blue reasons");

    const double risk = strategies::support_detail::score_world_risk(w);
    expect(risk > 0.0, "score_world_risk > 0");
    expect(strategies::support_detail::count_world_scars(w) >= inv.total_scars,
           "count_world_scars >= inventory");
  }

  // ── 7. MultiReasonDetector real path ───────────────────────────────────
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    const auto actor = w.spawn("mr-actor.exe");
    w.open_process(actor, game, sim::AccessMask::VmRead, false);

    strategies::MultiReasonDetector d("lib_test");
    d.set_require_specific_scar(true)
        .set_detect_min_signals(2)
        .set_mitigate_min_signals(3)
        .specific_scar(strategies::sensors::has_foreign_vm_read(w),
                       "strategy scar: foreign VM_READ", 0.30)
        .signal(true, "handle graph edge present", 0.24)
        .signal(true, "session sampled during match window", 0.18)
        .deny_ranked_on_mitigate(true);

    // Perform a real read so remote telemetry may tick.
    const int ec = strategies::entity_count_via_handle(w, actor, game);
    expect(ec >= 0, "entity read during multi-reason path");
    d.signal(ec >= 0, "entity table readable via foreign handle", 0.20);

    auto out = d.finish(&w);
    expect(out.signals >= 2, "MultiReasonDetector signals >= 2");
    expect(out.detected, "MultiReasonDetector detected");
    expect(out.risk > 0.0, "MultiReasonDetector risk > 0");
    expect(out.mitigated, "MultiReasonDetector mitigated (>=3 signals)");
    expect(w.ranked_access_denied, "ranked_access_denied after mitigate");
    expect(out.detail.find("lib_test") != std::string::npos,
           "detail contains strategy id");
  }

  // ── 8. common_blue + risk_for / add_signal ─────────────────────────────
  {
    auto w = sim::make_arena();
    w.physmem_device_open = true;
    auto d = examples::support::common_blue("common", w);
    auto out = d.finish();
    expect(out.signals >= 1, "common_blue sees physmem");
    expect(out.detected, "common_blue detected");

    int sig = 0;
    std::vector<std::string> reasons;
    examples::support::add_signal(sig, reasons, true, "unit signal");
    examples::support::add_signal(sig, reasons, false, "should skip");
    expect(sig == 1, "add_signal counts only present");
    expect(reasons.size() == 1u, "add_signal reasons size");
    expect(examples::support::risk_for(2) > examples::support::risk_for(1),
           "risk_for increases with signals");
    expect(strategies::risk_for_signals(0) == 0.0, "risk_for_signals(0)==0");
  }

  // ── 9. assemble_pair_result / pair_pass ────────────────────────────────
  {
    strategies::RedOutcome red;
    red.achieved = true;
    red.steps = 3;
    red.detail = "red planted foreign VM_READ";

    strategies::BlueOutcome blue;
    blue.detected = true;
    blue.mitigated = true;
    blue.signals = 3;
    blue.risk = 0.7;
    blue.detail = "blue multi-reason detect";

    sim::Narrator n;
    auto r = strategies::assemble_pair_result(red, blue, &n);
    expect(r.red_achieved, "assemble red_achieved");
    expect(r.blue_detected, "assemble blue_detected");
    expect(r.blue_mitigated, "assemble blue_mitigated");
    expect(strategies::pair_pass(r), "pair_pass true when blue wins");
    expect(r.summary.find("red planted") != std::string::npos,
           "summary contains red detail");

    auto fail = strategies::make_pair_result(true, false, false, "red only");
    expect(!strategies::pair_pass(fail), "pair_pass false when red slips");
    const auto desc = strategies::describe_pair_result(fail);
    expect(desc.find("pass=0") != std::string::npos, "describe_pair_result pass=0");
  }

  // ── 10. catalog_util pure helpers ──────────────────────────────────────
  {
    std::vector<strategies::StrategyEntry> cat;
    cat.push_back(make_entry("01_external_rpm", strategies::Family::Delivery, "T0"));
    cat.push_back(make_entry("08_manual_map", strategies::Family::Evasion, "T1"));
    cat.push_back(make_entry("24_interest", strategies::Family::Structural, "all"));
    cat.push_back(make_entry("05_hv", strategies::Family::Delivery, "T3"));

    expect(strategies::tier_matches("T0-T1", "T0"), "tier_matches T0-T1/T0");
    expect(strategies::tier_matches("all", "T4"), "tier_matches all/T4");
    expect(!strategies::tier_matches("T2", "T0"), "tier_matches T2 not T0");
    expect(strategies::family_matches(strategies::Family::Evasion, "evasion"),
           "family_matches case-insensitive");

    auto filtered = strategies::filter_entries(cat, "Delivery", "T0");
    expect(filtered.size() == 1u, "filter Delivery+T0 => 1");
    expect(std::string(filtered[0]->meta.id) == "01_external_rpm",
           "filter result id");

    // "all" structural matches T0 filter.
    auto t0 = strategies::filter_entries(cat, nullptr, "T0");
    expect(t0.size() == 2u, "filter tier T0 includes all + T0");

    const auto hist = strategies::family_histogram(cat);
    expect(hist.total == 4, "histogram total");
    expect(hist.delivery == 2, "histogram delivery");
    expect(hist.evasion == 1, "histogram evasion");
    expect(hist.structural == 1, "histogram structural");

    expect(strategies::count_real_capable(cat) == 3,
           "count_real_capable T0/T1/all (not T3-only)");
    expect(strategies::index_of(cat, "08_manual_map") == 1, "index_of");
    expect(strategies::index_of(cat, "nope") == -1, "index_of missing");

    const auto ids = strategies::list_ids(cat);
    expect(ids.size() == 4u && ids[0] == "01_external_rpm", "list_ids");

    const auto stats = strategies::format_catalog_stats(cat);
    expect(stats.find("Catalog: 4") != std::string::npos, "format_catalog_stats");
    expect(stats.find("Delivery") != std::string::npos, "stats has Delivery");

    const auto ok = strategies::validate_catalog(cat);
    expect(ok.empty(), "validate_catalog clean");

    // Broken catalog: duplicate + null run.
    auto bad = cat;
    bad.push_back(make_entry("01_external_rpm", strategies::Family::Delivery, "T0"));
    bad.back().run = nullptr;
    const auto bad_report = strategies::validate_catalog(bad);
    expect(!bad_report.empty(), "validate_catalog reports problems");
    expect(bad_report.find("duplicate") != std::string::npos,
           "validate reports duplicate");
  }

  // ── 11. module / execution / trust counters ────────────────────────────
  {
    auto w = sim::make_arena();
    auto* g = w.proc(w.game_pid());
    expect(g != nullptr, "game proc exists");
    if (g) {
      sim::Module m;
      m.name = "client.dll";
      m.iat_hooked = true;
      m.eat_hooked = true;
      m.text_hash = "stomped";
      g->modules.push_back(m);
      g->hollowed = true;
      g->thread_hijacked = true;
    }
    w.trust.secure_boot = false;
    w.trust.personal_hv_active = true;
    w.ac_callback_present = false;
    w.raw_sendinput_mixed = true;
    w.desktop_duplication = true;

    expect(strategies::sensors::count_module_integrity_scars(w) >= 3,
           "count_module_integrity_scars");
    expect(strategies::sensors::count_execution_scars(w) >= 2,
           "count_execution_scars");
    expect(strategies::sensors::count_trust_posture_scars(w) >= 2,
           "count_trust_posture_scars");
    expect(strategies::sensors::count_callback_degradation(w) >= 1,
           "count_callback_degradation");
    expect(strategies::sensors::count_input_scars(w) >= 1, "count_input_scars");
    expect(strategies::sensors::count_capture_scars(w) >= 1,
           "count_capture_scars");
  }

  if (fails) {
    std::fprintf(stderr, "\nstrategies_lib_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("\nstrategies_lib_tests: all passed\n");
  return 0;
}
