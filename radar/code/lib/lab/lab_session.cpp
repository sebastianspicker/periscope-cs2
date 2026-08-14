#include "lab/lab_session.hpp"

#include "cs2/signatures.hpp"

#include <cstring>
#include <sstream>
#include <utility>

namespace lab {

LabSession::LabSession(LabSessionConfig config) : config_(std::move(config)) {}

void LabSession::build_esp_from_fixture(const FixtureProcess& fx) {
  draw_.clear();
  const auto ents = fx.read_entities();
  const auto vp = ViewProjection::orthographic(-100.f, 100.f, -100.f, 100.f,
                                                0.1f, 1000.f);
  int idx = 0;
  for (const auto& e : ents) {
    auto sp = DrawList::world_to_screen({e.x, e.y, e.z}, vp, 1920.f, 1080.f);
    if (!sp.on_screen) {
      // Still mark near-screen for lab determinism.
      sp.x = 200.f + static_cast<float>(idx) * 40.f;
      sp.y = 200.f + static_cast<float>(e.team) * 20.f;
      sp.on_screen = true;
    }
    draw_.entity_marker(sp.x, sp.y, e.team, e.alive != 0,
                        "E" + std::to_string(idx));
    if (e.alive) {
      draw_.box_2d(sp.x - 10.f, sp.y - 30.f, 20.f, 40.f,
                   e.team == 2 ? 0xFF4040FFu : 0xFFFFA040u);
    }
    ++idx;
  }
}

LabSessionReport LabSession::run_fixture_pipeline() {
  LabSessionReport report;
  auto& fx = global_fixture();
  // Ensure entities exist even if a prior test cleared the image.
  if (fx.entity_count() == 0) fx.plant_synthetic_entities();

  if (config_.plant_cs2_markers) {
    plant_cs2_pattern_markers(fx.image_mut(), fx.base_address(),
                              cs2::SignatureDatabase::get());
    report.markers_planted = true;
    report.steps.push_back("plant_cs2_pattern_markers");
  }

  if (backend_.attach(fx.id()) != ac::Status::Ok) {
    report.detail = "attach failed";
    last_ = report;
    return last_;
  }
  report.attached = true;
  report.steps.push_back("LabMemoryBackend::attach");

  // Drive sequential entity table reads (scan detector signal).
  ac::ReadRequest count_req{fx.base_address(), sizeof(std::uint32_t)};
  auto count_rr = backend_.read(count_req);
  if (count_rr.status == ac::Status::Ok &&
      count_rr.bytes.size() >= sizeof(std::uint32_t)) {
    std::uint32_t count = 0;
    std::memcpy(&count, count_rr.bytes.data(), sizeof(count));
    report.entities = static_cast<int>(count);
    for (std::uint32_t i = 0; i < count && i < 64; ++i) {
      backend_.read(ac::ReadRequest{
          fx.base_address() + 0x10 + i * sizeof(FixtureEntity),
          sizeof(FixtureEntity)});
    }
  }
  report.steps.push_back("entity_table_rpm");

  AobScanner scanner;
  scanner.set_memory_region(fx.image().data(), fx.image().size(),
                            fx.base_address());
  last_aob_ = scanner.scan_entity_radar_patterns();
  report.aob_found = last_aob_.found_count;
  report.aob_attempted = last_aob_.attempted_scans;
  report.steps.push_back("AobScanner::scan_entity_radar_patterns found=" +
                         std::to_string(report.aob_found));

  if (config_.resolve_apis_via_peb) {
    apis_.set_build_salt(0xC0FFEEu);
    report.apis_resolved = apis_.resolve_all(config_.api_path);
    report.api_stealth = apis_.analyze_iat_leaks().stealth_score;
    report.steps.push_back("ApiTable::resolve_all n=" +
                           std::to_string(report.apis_resolved));
  }

  if (config_.build_esp_drawlist) {
    build_esp_from_fixture(fx);
    const auto st = draw_.stats();
    report.draw_commands = st.total_commands;
    report.esp_signature = st.looks_like_esp;
    report.steps.push_back("DrawList ESP cmds=" +
                           std::to_string(report.draw_commands));
  }

  // Code-sample match against seeded cheat DB (self-scan of synthetic marker).
  auto& db = CheatSignatureDb::instance();
  if (db.known_cheat_count() > 0) {
    const auto& sig = db.signatures().front();
    auto hit = db.match(sig.code_sample, 0.90);
    if (hit) {
      report.steps.push_back("CheatSignatureDb match=" + hit->name);
    }
  }

  auto scan = ScanDetector::analyze_backend(backend_.stats(),
                                            report.markers_planted);
  report.scan_suspected = scan.pattern_scan_suspected;
  report.scan_risk = scan.risk_score;
  report.steps.push_back("ScanDetector::analyze_backend risk=" +
                         std::to_string(report.scan_risk));

  backend_.detach();
  report.steps.push_back("detach");

  report.success = report.attached && report.entities >= 2 &&
                   report.aob_found > 0 && report.apis_resolved >= 20;
  std::ostringstream oss;
  oss << "fixture_pipeline success=" << (report.success ? 1 : 0)
      << " entities=" << report.entities << " aob=" << report.aob_found
      << "/" << report.aob_attempted << " apis=" << report.apis_resolved
      << " scan_risk=" << report.scan_risk;
  report.detail = oss.str();
  last_ = report;
  return last_;
}

LabSessionReport LabSession::run_world_pipeline(sim::World& world,
                                                std::uint32_t cheat_pid) {
  LabSessionReport report;
  const auto game_pid = world.game_pid();
  auto* game = world.proc(game_pid);
  if (!game || !game->is_game) {
    report.detail = "world game process unavailable";
    last_ = report;
    return last_;
  }

  if (config_.plant_cs2_markers) {
    plant_cs2_pattern_markers(game->memory, game->base,
                              cs2::SignatureDatabase::get());
    world.lab_pattern_marker_present = true;
    report.markers_planted = true;
    report.steps.push_back("plant markers into world game memory");
  }

  // Open handle + bulk read to generate remote_read telemetry.
  if (!world.open_process(cheat_pid, game_pid, sim::AccessMask::VmRead, false)) {
    // Spawn a reader if cheat pid invalid.
    const auto reader = world.spawn("lab-session-reader.exe");
    if (!world.open_process(reader, game_pid, sim::AccessMask::VmRead, false)) {
      report.detail = "open_process failed";
      last_ = report;
      return last_;
    }
    cheat_pid = reader;
  }
  report.attached = true;
  report.steps.push_back("open_process VM_READ");

  const auto image = world.read_mem(cheat_pid, game_pid, game->base,
                                    game->memory.size(), true);
  if (image.status != ac::Status::Ok) {
    report.detail = "world read_mem failed";
    last_ = report;
    return last_;
  }
  report.steps.push_back("read_mem image_bytes=" +
                         std::to_string(image.bytes.size()));

  AobScanner scanner;
  scanner.set_memory_region(image.bytes.data(), image.bytes.size(), game->base);
  last_aob_ = scanner.scan_entity_radar_patterns();
  report.aob_found = last_aob_.found_count;
  report.aob_attempted = last_aob_.attempted_scans;
  report.steps.push_back("AOB found=" + std::to_string(report.aob_found));

  // Extra sequential reads for detector burst threshold.
  for (int i = 0; i < 4; ++i) {
    world.read_mem(cheat_pid, game_pid, game->base + 0x10 + i * 16, 16, true);
  }

  apis_.set_build_salt(static_cast<std::uint64_t>(game_pid) ^ 0x1AB5E55u);
  report.apis_resolved = apis_.resolve_all(config_.api_path);
  apis_.apply_to_world(world);
  report.api_stealth = apis_.analyze_iat_leaks().stealth_score;
  report.steps.push_back("ApiTable apply_to_world");

  DisguiseEngine::apply_to_world(world, cheat_pid, config_.disguise);
  auto dvr = DisguiseEngine::verify_report(world, cheat_pid, config_.disguise);
  report.disguise_verified = dvr.verified;
  report.steps.push_back(dvr.detail);

  // Entity count from world memory.
  if (game->memory.size() >= 4) {
    std::uint32_t count = 0;
    std::memcpy(&count, game->memory.data(), sizeof(count));
    report.entities = static_cast<int>(count);
  }

  if (config_.build_esp_drawlist) {
    // Build ESP from world entity table when present.
    auto& fx = global_fixture();
    build_esp_from_fixture(fx);
    const auto st = draw_.stats();
    report.draw_commands = st.total_commands;
    report.esp_signature = st.looks_like_esp;
  }

  ScanDetector detector(world);
  auto scan = detector.analyze_with_aob(last_aob_);
  report.scan_suspected = scan.pattern_scan_suspected;
  report.scan_risk = scan.risk_score;
  report.steps.push_back(scan.detail);

  report.success = report.attached && report.aob_found > 0 &&
                   report.apis_resolved >= 20 && report.disguise_verified &&
                   report.scan_suspected;
  std::ostringstream oss;
  oss << "world_pipeline success=" << (report.success ? 1 : 0)
      << " aob=" << report.aob_found << " apis=" << report.apis_resolved
      << " disguise=" << (report.disguise_verified ? 1 : 0)
      << " scan_risk=" << report.scan_risk;
  report.detail = oss.str();
  last_ = report;
  return last_;
}

}  // namespace lab
