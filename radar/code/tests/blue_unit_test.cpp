// Blue multi-sensor stack tests: drive shipped blue::BlueCoordinator on sim::World.
// No local reimplementation of sensors; every assertion hits evaluate()/evaluate_view().

#include "blue/blue_system.hpp"
#include "sim/world.hpp"
#include "ac/types.hpp"

#include <cstdio>
#include <cmath>
#include <string>

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

void plant_classic_rpm(sim::World& world) {
  const auto game_pid = world.game_pid();
  const auto cheat_pid = world.spawn("cheat-client.exe");
  world.open_process(cheat_pid, game_pid, sim::AccessMask::VmRead, false);
  const auto* game = world.proc(game_pid);
  for (int i = 0; i <= 100; ++i) {
    (void)world.read_mem(cheat_pid, game_pid, game->base, 4, true);
  }
}

bool view_fired(const blue::BlueAggregatedResult& r, ac::ObservationView v) {
  for (const auto& pv : r.per_view) {
    if (pv.view == v && pv.anomaly_detected) {
      return true;
    }
  }
  return false;
}

const blue::BlueViewResult* find_view(const blue::BlueAggregatedResult& r,
                                      ac::ObservationView v) {
  for (const auto& pv : r.per_view) {
    if (pv.view == v) {
      return &pv;
    }
  }
  return nullptr;
}

}  // namespace

int main() {
  std::printf("=== Blue unit tests (shipped API) ===\n");

  // ── 1. Classic external RPM scars → multi-view detection ───────────────
  {
    std::printf("\n[1] RPM scar multi-view detect\n");
    sim::World world;
    const auto game_pid = world.spawn("cs2.exe", true);
    world.plant_lab_entities(game_pid);
    plant_classic_rpm(world);

    blue::BlueCoordinator blue(world);
    const auto result = blue.evaluate();

    std::printf("  %s\n", result.summary.c_str());
    for (const auto& reason : result.detection_reasons) {
      std::printf("  reason: %s\n", reason.c_str());
    }

    expect(result.total_views_active == 6, "all six views active by default");
    expect(result.views_with_anomalies >= 2, "RPM: views_with_anomalies >= 2");
    expect(result.red_detected, "RPM: red_detected true");
    expect(view_fired(result, ac::ObservationView::HandleTable),
           "RPM: HandleTable fires");
    expect(view_fired(result, ac::ObservationView::MemoryPattern),
           "RPM: MemoryPattern fires");
    expect(!result.detection_reasons.empty(), "RPM: non-empty detection_reasons");
    expect(!result.per_view.empty(), "RPM: per_view populated");
    expect(result.overall_confidence > 0.0, "RPM: overall_confidence > 0");
    expect(result.views_with_anomalies_list.size() ==
               static_cast<std::size_t>(result.views_with_anomalies),
           "RPM: anomaly list size matches count");

    // Per-view details must be World-derived, not empty stubs.
    const auto* ht = find_view(result, ac::ObservationView::HandleTable);
    expect(ht != nullptr && ht->anomaly_detected && !ht->detail.empty() &&
               ht->detail != "ok",
           "RPM: HandleTable has real detail");
    const auto* mp = find_view(result, ac::ObservationView::MemoryPattern);
    expect(mp != nullptr && mp->anomaly_detected && !mp->detail.empty(),
           "RPM: MemoryPattern has real detail");
  }

  // ── 2. Clean baseline World → no actionable multi-view detection ───────
  {
    std::printf("\n[2] Clean World baseline\n");
    sim::World world = sim::make_arena("cs2.exe");
    // Game + AC only; no foreign handles, no RPM, no planted cheat scars.
    blue::BlueCoordinator blue(world);
    const auto result = blue.evaluate();

    std::printf("  %s\n", result.summary.c_str());
    expect(!result.red_detected, "clean: red_detected false");
    expect(!result.strong_detection, "clean: strong_detection false");
    expect(result.views_with_anomalies < 2, "clean: views_with_anomalies < 2");
    expect(result.total_views_active == 6, "clean: all views still active");
    // No fabricated strong detection confidence.
    expect(result.overall_confidence < 0.5, "clean: overall_confidence not strong");
  }

  // ── 3. View enable/disable observably changes aggregation ──────────────
  {
    std::printf("\n[3] Config: disable HandleTable under RPM plant\n");
    sim::World world;
    const auto game_pid = world.spawn("cs2.exe", true);
    world.plant_lab_entities(game_pid);
    plant_classic_rpm(world);

    blue::BlueCoordinator blue(world);
    const auto full = blue.evaluate();
    expect(full.red_detected, "config setup: full evaluate detects");
    const int full_anomalies = full.views_with_anomalies;
    const auto full_reason_count = full.detection_reasons.size();

    blue.set_view_enabled(ac::ObservationView::HandleTable, false);
    expect(!blue.is_view_enabled(ac::ObservationView::HandleTable),
           "HandleTable disabled");
    const auto partial = blue.evaluate();

    std::printf("  full anomalies=%d reasons=%zu | partial anomalies=%d reasons=%zu\n",
                full_anomalies, full_reason_count, partial.views_with_anomalies,
                partial.detection_reasons.size());

    expect(partial.total_views_active == 5, "disabled view not counted active");
    expect(!view_fired(partial, ac::ObservationView::HandleTable),
           "disabled HandleTable does not appear in per_view fires");
    expect(partial.views_with_anomalies == full_anomalies - 1 ||
               partial.views_with_anomalies < full_anomalies,
           "anomaly count drops after disable");
    expect(partial.detection_reasons.size() < full_reason_count,
           "detection_reasons shrink after disable");

    // Re-enable restores path.
    blue.set_view_enabled(ac::ObservationView::HandleTable, true);
    const auto restored = blue.evaluate();
    expect(restored.total_views_active == 6, "re-enable restores active count");
    expect(view_fired(restored, ac::ObservationView::HandleTable),
           "re-enable HandleTable fires again");
  }

  // ── 4. Sensitivity observably changes confidence / thresholds ──────────
  {
    std::printf("\n[4] Config: sensitivity effect on weak module scar\n");
    sim::World world;
    const auto game_pid = world.spawn("cs2.exe", true);
    world.plant_lab_entities(game_pid);
    auto* game = world.proc(game_pid);
    // Weak signal alone: high module count (weight 0.20) — subthreshold at sens=1.
    for (int i = 0; i < 90; ++i) {
      game->modules.push_back(sim::Module{"extra_" + std::to_string(i), 0, 0});
    }

    blue::BlueCoordinator blue(world);
    blue.set_sensitivity(ac::ObservationView::ModuleList, 1.0);
    auto low = blue.evaluate_view(ac::ObservationView::ModuleList);
    std::printf("  sens=1.0 anomaly=%d conf=%.3f detail=%s\n",
                static_cast<int>(low.anomaly_detected), low.confidence,
                low.detail.c_str());

    blue.set_sensitivity(ac::ObservationView::ModuleList, 3.0);
    auto high = blue.evaluate_view(ac::ObservationView::ModuleList);
    std::printf("  sens=3.0 anomaly=%d conf=%.3f detail=%s\n",
                static_cast<int>(high.anomaly_detected), high.confidence,
                high.detail.c_str());

    expect(blue.sensitivity_of(ac::ObservationView::ModuleList) == 3.0,
           "sensitivity_of reflects set value");
    expect(high.confidence > low.confidence,
           "higher sensitivity increases confidence");
    // Strong scar should still fire at default sensitivity.
    game->manual_mapped_region = true;
    blue.set_sensitivity(ac::ObservationView::ModuleList, 1.0);
    auto strong = blue.evaluate_view(ac::ObservationView::ModuleList);
    expect(strong.anomaly_detected, "manual_map fires ModuleList at sens=1");
    expect(!strong.reasons.empty(), "ModuleList reasons non-empty");
  }

  // ── 5. Additional scar classes: each declared view can fire for real ───
  {
    std::printf("\n[5] Per-view scar coverage\n");
    sim::World world;
    const auto game_pid = world.spawn("cs2.exe", true);
    world.plant_lab_entities(game_pid);
    auto* game = world.proc(game_pid);

    // HandleTable
    const auto cheat = world.spawn("reader.exe");
    world.open_process(cheat, game_pid, sim::AccessMask::VmRead, true);

    // ModuleList
    game->manual_mapped_region = true;
    game->modules.push_back(
        sim::Module{"inject.dll", 0x20000000, 0x1000, /*linked*/ false,
                    /*headers_erased*/ true, "dirty", true, false, false});

    // MemoryPattern
    world.remote_read_ops = 150;
    world.remote_read_bytes = 2 * 1024 * 1024;

    // InProcess
    game->has_foreign_thread = true;
    world.steam_present_hooked = true;

    // Behavioral
    world.silent_aim_active = true;
    world.aim_samples.push_back({});
    world.aim_samples.back().camera_yaw = 0;
    world.aim_samples.back().server_aim_yaw = 45;
    world.aim_samples.back().challenge_passed = false;
    world.aim_samples.push_back(world.aim_samples.back());
    world.aim_samples.push_back(world.aim_samples.back());
    world.triggerbot_active = true;
    world.trigger_on_target_fires = 10;
    world.trigger_mean_latency_ms = 5.f;

    // PostExecution
    world.pattern_rescan_count = 8;
    world.forensic_cleanup_active = true;
    world.forensic_prefetch_cleared = true;
    world.forensic_cleanup_steps_completed = 3;

    blue::BlueCoordinator blue(world);
    const auto result = blue.evaluate();
    std::printf("  %s\n", result.summary.c_str());
    for (const auto& reason : result.detection_reasons) {
      std::printf("  reason: %s\n", reason.c_str());
    }

    expect(view_fired(result, ac::ObservationView::HandleTable),
           "scar: HandleTable");
    expect(view_fired(result, ac::ObservationView::ModuleList), "scar: ModuleList");
    expect(view_fired(result, ac::ObservationView::MemoryPattern),
           "scar: MemoryPattern");
    expect(view_fired(result, ac::ObservationView::InProcess), "scar: InProcess");
    expect(view_fired(result, ac::ObservationView::Behavioral), "scar: Behavioral");
    expect(view_fired(result, ac::ObservationView::PostExecution),
           "scar: PostExecution");
    expect(result.views_with_anomalies >= 3, "multi-scar: >=3 views");
    expect(result.strong_detection, "multi-scar: strong_detection");
    expect(result.red_detected, "multi-scar: red_detected");
    expect(result.detection_reasons.size() >= 3, "multi-scar: >=3 reasons");
  }

  // ── 6. evaluate_view matches evaluate per-view for same World ──────────
  {
    std::printf("\n[6] evaluate_view consistency\n");
    sim::World world;
    const auto game_pid = world.spawn("cs2.exe", true);
    world.plant_lab_entities(game_pid);
    plant_classic_rpm(world);

    blue::BlueCoordinator blue(world);
    const auto agg = blue.evaluate();
    for (const auto& pv : agg.per_view) {
      const auto single = blue.evaluate_view(pv.view);
      expect(single.anomaly_detected == pv.anomaly_detected,
             "evaluate_view anomaly matches aggregate per_view");
      expect(std::fabs(single.confidence - pv.confidence) < 1e-9,
             "evaluate_view confidence matches aggregate per_view");
    }
  }

  std::printf("\n=== Blue unit tests: %s (%d failures) ===\n",
              fails == 0 ? "PASS" : "FAIL", fails);
  return fails == 0 ? 0 : 1;
}
