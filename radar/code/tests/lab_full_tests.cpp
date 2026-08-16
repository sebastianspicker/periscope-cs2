// lab_full_tests.cpp — drive every shipped lab/ entry point on the real path.
// No stubs, no hard-coded expected addresses, no re-implementation of scanners.

#include "lab/aob_scanner.hpp"
#include "lab/api_table.hpp"
#include "lab/cheat_sig_db.hpp"
#include "lab/disguise_engine.hpp"
#include "lab/draw_list.hpp"
#include "lab/fixture_process.hpp"
#include "lab/lab_memory.hpp"
#include "lab/lab_session.hpp"
#include "lab/scan_detector.hpp"
#include "sim/world.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

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
  // ── 1. FixtureProcess entity layout + modules ───────────────────────────
  {
    lab::FixtureProcess fx(9001);
    expect(lab::register_fixture(&fx), "register secondary fixture");
    expect(fx.entity_count() >= 2, "fixture entity_count >= 2");
    auto ents = fx.read_entities();
    expect(ents.size() == fx.entity_count(), "fixture read_entities size");
    expect(ents[0].alive != 0 && ents[1].team != 0, "fixture entity fields");
    expect(!fx.modules().empty(), "fixture default modules planted");
    auto vm = fx.read_bytes(fx.base_address() + lab::FixtureProcess::kViewMatrixRel,
                            sizeof(float) * 16);
    const bool matrix_readable =
        vm.status == ac::Status::Ok && vm.bytes.size() == sizeof(float) * 16;
    expect(matrix_readable, "fixture view matrix readable");
    float m15 = 0;
    if (matrix_readable) {
      std::array<std::uint8_t, sizeof(float)> raw{};
      std::copy_n(vm.bytes.begin() + 15 * sizeof(float), raw.size(), raw.begin());
      m15 = std::bit_cast<float>(raw);
    }
    expect(m15 == 1.0f, "fixture identity view matrix m[15]==1");

    const auto original_size = fx.image_size();
    const std::uint8_t one = 1;
    fx.write_bytes(std::numeric_limits<std::uint64_t>::max(), {&one, 1});
    expect(fx.image_size() == original_size,
           "fixture rejects overflowing write address without resize");
    expect(fx.read_bytes(fx.base_address() + original_size - 1, 2).status ==
               ac::Status::InvalidArgument,
           "fixture rejects truncated read extent");
    auto misaligned = fx.read_bytes(fx.base_address() + 1, sizeof(std::uint32_t));
    expect(misaligned.status == ac::Status::Ok &&
               misaligned.bytes.size() == sizeof(std::uint32_t),
           "fixture permits bounded misaligned byte read");

    lab::LabMemoryBackend be;
    expect(be.attach(9001) == ac::Status::Ok, "lab_memory attach secondary id");
    be.detach();
    lab::clear_fixture_registry();
    // global_fixture re-registers itself on first use after clear.
    (void)lab::global_fixture();
  }

  // ── 2. LabMemoryBackend attach/read/write/scatter/stats ────────────────
  {
    auto& fx = lab::global_fixture();
    lab::LabMemoryBackend be;
    expect(be.attach(fx.id()) == ac::Status::Ok, "lab_memory attach global");
    expect(be.is_attached() && be.tier() == ac::Tier::T0_UsermodeRpm,
           "lab_memory attached tier");
    auto rr = be.read({fx.base_address(), sizeof(std::uint32_t)});
    const bool count_readable =
        rr.status == ac::Status::Ok && rr.bytes.size() >= sizeof(std::uint32_t);
    expect(count_readable, "lab_memory read count");
    std::uint32_t count = 0;
    if (count_readable) {
      std::array<std::uint8_t, sizeof(std::uint32_t)> raw{};
      std::copy_n(rr.bytes.begin(), raw.size(), raw.begin());
      count = std::bit_cast<std::uint32_t>(raw);
    }
    expect(count >= 2, "lab_memory entity count field");

    // Sequential entity reads to trip sequential_burst / bulk flags.
    for (std::uint32_t i = 0; i < count; ++i) {
      auto er = be.read({fx.base_address() + 0x10 + i * 16, 16});
      expect(er.status == ac::Status::Ok, "lab_memory entity row read");
    }
    expect(be.stats().read_ops >= 1 + count, "lab_memory stats read_ops");
    expect(be.stats().read_bytes >= sizeof(std::uint32_t) + count * 16,
           "lab_memory stats read_bytes");

    const std::uint8_t marker[] = {'L', 'A', 'B', '1'};
    expect(be.write(fx.base_address() + 0x80, marker) == ac::Status::Ok,
           "lab_memory write");
    auto wr = be.read({fx.base_address() + 0x80, 4});
    expect(wr.status == ac::Status::Ok && wr.bytes.size() == 4 &&
               wr.bytes[0] == 'L' && wr.bytes[3] == '1',
           "lab_memory write/read roundtrip");

    auto sc = be.scatter_read({{fx.base_address(), 4},
                               {fx.base_address() + 0x10, 16}});
    expect(sc.status == ac::Status::Ok && sc.parts.size() == 2,
           "lab_memory scatter_read");
    expect(be.stats().scatter_ops >= 1, "lab_memory scatter_ops");

    auto partial = be.scatter_read({{fx.base_address(), 4},
                                    {fx.base_address() + fx.image_size() - 1, 2}});
    expect(partial.status == ac::Status::InvalidArgument && partial.parts.size() == 2 &&
               partial.parts[1].bytes.empty(),
           "lab_memory preserves partial scatter failure without short bytes");

    be.detach();
    expect(!be.is_attached(), "lab_memory detach");
    expect(be.read({fx.base_address(), 4}).status != ac::Status::Ok,
           "lab_memory read after detach fails");
    expect(be.attach(0xDEADBEEF) != ac::Status::Ok,
           "lab_memory reject unknown id");
  }

  // ── 3. AobScanner find planted markers ─────────────────────────────────
  {
    auto& fx = lab::global_fixture();
    lab::plant_cs2_pattern_markers(fx.image_mut(), fx.base_address(),
                                   cs2::SignatureDatabase::get());
    lab::AobScanner scanner;
    scanner.set_memory_region(fx.image().data(), fx.image().size(),
                              fx.base_address());
    auto report = scanner.scan_entity_radar_patterns();
    expect(report.attempted_scans > 0, "aob attempted > 0");
    expect(report.found_count > 0, "aob found planted markers");
    expect(report.found_count ==
               static_cast<int>(std::count_if(
                   report.operations.begin(), report.operations.end(),
                   [](const lab::ScanOperation& o) { return o.found; })),
           "aob found_count matches operations");
    expect(scanner.last_report().found_count == report.found_count,
           "aob last_report");

    // Raw pattern: plant a unique sequence and scan it.
    const std::uint8_t raw[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    const auto plant_off = fx.image().size();
    fx.image_mut().insert(fx.image_mut().end(), raw, raw + sizeof(raw));
    scanner.set_memory_region(fx.image().data(), fx.image().size(),
                              fx.base_address());
    auto op = scanner.scan_raw("lab_unique", "DE AD BE EF CA FE BA BE");
    expect(op.found && op.resolved_addr == fx.base_address() + plant_off,
           "aob scan_raw unique pattern");

    // Rip-relative: E8 call with disp32 = 0x10 at instruction end.
    // instr at base+0: E8 10 00 00 00 → target = instr+5+0x10
    std::vector<std::uint8_t> call_img = {0xE8, 0x10, 0x00, 0x00, 0x00};
    scanner.set_memory_region(call_img.data(), call_img.size(), 0x20000000ull);
    auto rip = scanner.resolve_rip_relative(0x20000000ull, 1, 5);
    expect(rip.valid && rip.target_addr == 0x20000000ull + 5 + 0x10,
           "aob rip-relative decode");
  }

  // ── 4. ApiTable PEB vs IAT stealth ─────────────────────────────────────
  {
    lab::ApiTable peb;
    peb.set_build_salt(0xABCDu);
    const int n = peb.resolve_all(lab::ApiResolvePath::PebEatWalk);
    expect(n >= 20, "api_table resolve_all PEB count");
    expect(peb.resolve(lab::ApiName::NtOpenProcess) != nullptr,
           "api_table NtOpenProcess");
    auto leak = peb.analyze_iat_leaks();
    expect(leak.total_resolved == n, "api_table leak total");
    expect(leak.runtime_resolved == n && leak.in_iat == 0,
           "api_table all PEB no IAT");
    expect(leak.stealth_score == 1.0, "api_table full stealth");
    expect(leak.hidden.size() >= 1, "api_table dangerous hidden");
    expect(leak.leaked.empty(), "api_table no IAT leaks");

    lab::ApiTable iat;
    iat.resolve_dangerous(lab::ApiResolvePath::ImportAddressTable);
    auto bad = iat.analyze_iat_leaks();
    expect(bad.in_iat >= 1 && !bad.leaked.empty(), "api_table IAT leaks dangerous");
    expect(bad.stealth_score < 1.0, "api_table IAT stealth degraded");

    auto w = sim::make_arena();
    peb.apply_to_world(w);
    expect(w.dynamic_import_resolution && w.dynamic_import_count >= 20,
           "api_table apply_to_world scars");
  }

  // ── 5. CheatSignatureDb seed + match ───────────────────────────────────
  {
    auto& db = lab::CheatSignatureDb::instance();
    expect(db.known_cheat_count() >= 4, "cheat_sig seeded known cheats");
    const auto& first = db.signatures().front();
    expect(first.known_cheat && !first.code_sample.empty(),
           "cheat_sig first sample");
    auto hit = db.match(first.code_sample, 0.90);
    expect(hit.has_value() && hit->name == first.name,
           "cheat_sig exact match on seeded sample");

    // Mutate one byte → still high similarity if threshold loose.
    auto noisy = first.code_sample;
    if (!noisy.empty()) noisy[0] ^= 0xFF;
    auto ranked = db.match_all(noisy, 0.50);
    expect(!ranked.empty() && ranked.front().similarity < 1.0,
           "cheat_sig fuzzy match_all");

    // Benign sample should not match as known cheat at high threshold.
    std::vector<std::uint8_t> benign = {0x90, 0x90, 0x90, 0xC3};
    expect(!db.match(benign, 0.95).has_value(),
           "cheat_sig benign no high-threshold hit");
  }

  // ── 6. DisguiseEngine multi-signal ─────────────────────────────────────
  {
    auto w = sim::make_arena();
    const auto cheat = w.spawn("cheat.exe");
    lab::DisguiseEngine::apply_to_world(w, cheat, ac::DisguiseProfile::RivaTuner);
    auto rep = lab::DisguiseEngine::verify_report(w, cheat,
                                                  ac::DisguiseProfile::RivaTuner);
    expect(rep.verified && rep.signals_matched >= 3,
           "disguise multi-signal verified");
    expect(rep.process_name_ok && rep.window_class_ok, "disguise name+class");
    expect(w.hw_monitor_disguise_active, "disguise hw_monitor flag");
    expect(lab::DisguiseEngine::verify_disguise(
               w, cheat, ac::DisguiseProfile::RivaTuner),
           "disguise verify_disguise bool");

    // Wrong profile must fail multi-signal.
    auto wrong = lab::DisguiseEngine::verify_report(
        w, cheat, ac::DisguiseProfile::ObsStudio);
    expect(!wrong.verified, "disguise wrong profile fails");

    // All profiles produce non-empty attributes.
    for (auto p : {ac::DisguiseProfile::SteamOverlay,
                   ac::DisguiseProfile::DiscordOverlay,
                   ac::DisguiseProfile::ObsStudio,
                   ac::DisguiseProfile::NvidiaShadowplay,
                   ac::DisguiseProfile::GenericMonitor}) {
      auto a = lab::DisguiseEngine::attributes_for(p);
      expect(!a.process_name.empty() && !a.window_class.empty(),
             std::string("disguise attrs ") +
                 std::string(lab::DisguiseEngine::profile_name(p)));
    }
  }

  // ── 7. DrawList ESP helpers + world_to_screen ──────────────────────────
  {
    lab::DrawList dl;
    dl.entity_marker(100, 200, 2, true, "T");
    dl.entity_marker(300, 400, 3, true, "CT");
    dl.box_2d(90, 170, 20, 40, 0xFF4040FFu);
    dl.line(0, 0, 10, 10, 0xFFFFFFFFu);
    auto st = dl.stats();
    expect(st.total_commands >= 4, "draw_list commands");
    expect(st.looks_like_esp, "draw_list looks_like_esp");
    expect(st.unique_colors >= 2, "draw_list multi color");

    // Point inside the orthographic volume (z between near/far).
    auto vp = lab::ViewProjection::orthographic(-50, 50, -50, 50, 0.1f, 100.f);
    auto sp = lab::DrawList::world_to_screen({0, 0, 50.f}, vp, 800, 600);
    expect(sp.on_screen, "draw_list world_to_screen mid-frustum on screen");
    // Identity matrix: origin maps to screen center.
    auto id = lab::ViewProjection::identity();
    auto sp0 = lab::DrawList::world_to_screen({0, 0, 0}, id, 800, 600);
    expect(sp0.on_screen && sp0.x > 300.f && sp0.x < 500.f,
           "draw_list world_to_screen identity origin near center");
  }

  // ── 8. ScanDetector world + backend paths ──────────────────────────────
  {
    auto w = sim::make_arena();
    w.lab_pattern_marker_present = true;
    const auto reader = w.spawn("scanner.exe");
    const auto game = w.game_pid();
    expect(w.open_process(reader, game, sim::AccessMask::VmRead, false),
           "scan open_process");
    auto* g = w.proc(game);
    expect(g != nullptr, "scan game proc");
    // Burst remote reads.
    for (int i = 0; i < 5; ++i) {
      auto rr = w.read_mem(reader, game, g->base + i * 0x100, 0x200, true);
      expect(rr.status == ac::Status::Ok, "scan remote read");
    }
    lab::ScanDetector det(w);
    auto rep = det.analyze();
    expect(rep.bulk_read_detected, "scan_detector bulk");
    expect(rep.pattern_scan_suspected, "scan_detector suspected");
    expect(rep.risk_score > 0.0, "scan_detector risk");
    expect(rep.signal_count >= 2, "scan_detector multi-signal");

    lab::AobScanner sc;
    // Empty memory → zero found; still exercises analyze_with_aob path.
    std::vector<std::uint8_t> empty(64, 0);
    sc.set_memory_region(empty.data(), empty.size(), g->base);
    // Plant one CS2 marker into empty and scan.
    lab::plant_cs2_pattern_markers(empty, g->base,
                                   cs2::SignatureDatabase::get());
    sc.set_memory_region(empty.data(), empty.size(), g->base);
    auto aob = sc.scan_entity_radar_patterns();
    auto rep2 = det.analyze_with_aob(aob);
    expect(rep2.risk_score >= rep.risk_score || rep2.aob_report_correlated ||
               aob.found_count == 0,
           "scan_detector analyze_with_aob runs");

    lab::LabMemoryStats st{};
    st.read_ops = 8;
    st.read_bytes = 4096;
    st.bulk_read = true;
    st.sequential_burst = true;
    auto be = lab::ScanDetector::analyze_backend(st, true);
    expect(be.pattern_scan_suspected && be.risk_score > 0.5,
           "scan_detector backend path");
  }

  // ── 9. LabSession end-to-end fixture + world pipelines ─────────────────
  {
    lab::LabSession session;
    auto fx_rep = session.run_fixture_pipeline();
    expect(fx_rep.attached, "session fixture attached");
    expect(fx_rep.entities >= 2, "session fixture entities");
    expect(fx_rep.aob_found > 0, "session fixture aob found");
    expect(fx_rep.apis_resolved >= 20, "session fixture apis");
    expect(fx_rep.success, "session fixture success");
    expect(!fx_rep.steps.empty(), "session fixture steps");
    std::printf("  fixture: %s\n", fx_rep.detail.c_str());

    auto w = sim::make_arena();
    const auto cheat = w.spawn("lab-radar.exe");
    lab::LabSession world_session;
    auto w_rep = world_session.run_world_pipeline(w, cheat);
    expect(w_rep.attached, "session world attached");
    expect(w_rep.aob_found > 0, "session world aob");
    expect(w_rep.apis_resolved >= 20, "session world apis");
    expect(w_rep.disguise_verified, "session world disguise");
    expect(w_rep.scan_suspected, "session world scan suspected");
    expect(w_rep.success, "session world success");
    std::printf("  world: %s\n", w_rep.detail.c_str());
  }

  if (fails) {
    std::fprintf(stderr, "lab_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("lab_full_tests: all passed\n");
  return 0;
}
