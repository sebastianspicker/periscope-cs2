// Dummy FPS bomb plant/defuse demo for the anti-cheat lab.
// Headless scenario progression — no 3D renderer required.

#include "fps/scenario.hpp"
#include "fps/lab_bridge.hpp"
#include "server/info_advantage.hpp"
#include "server/interest_mgmt.hpp"
#include "sim/world.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

void banner(const char* title) {
  std::printf("\n======== %s ========\n", title);
}

void show(const fps::Scenario& sc) {
  std::fputs(sc.describe().c_str(), stdout);
}

int run_drop_pickup() {
  banner("SCENARIO: bomb carrier dies → drop → teammate pickup → plant");
  fps::Scenario sc;
  sc.start_default_round();
  sc.end_freeze();
  show(sc);
  std::printf("carrier=%u\n", sc.bomb_carrier_id());
  sc.kill(1);
  std::printf("after kill carrier: ground=%d\n", sc.bomb_on_ground() ? 1 : 0);
  sc.move_player(2, sc.bomb().ground.origin);
  auto up = sc.pickup_bomb(2);
  std::printf("pickup: ok=%d reason=%s carrier=%u\n", up.ok, up.reason,
              sc.bomb_carrier_id());
  auto plant = sc.plant_instant(2, "A");
  std::printf("plant: ok=%d phase=%s\n", plant.ok, fps::to_string(sc.phase()));
  show(sc);
  return plant.ok && sc.bomb().planted ? 0 : 2;
}

int run_economy() {
  banner("SCENARIO: freeze buy kit+rifle → plant → money update");
  fps::Scenario sc;
  sc.start_default_round();
  std::printf("phase=%s freeze=%.1f money_T1=%d\n", fps::to_string(sc.phase()),
              sc.freeze_clock(), sc.player(1)->money);
  sc.buy_rifle(1);
  sc.buy_defuse_kit(3);
  sc.end_freeze();
  sc.plant_instant(1, "A");
  std::printf("after plant money_T1=%d kit_CT=%d\n", sc.player(1)->money,
              sc.player(3)->has_defuse_kit ? 1 : 0);
  sc.defuse_instant(3);
  std::printf("after defuse money_CT1=%d outcome=%s\n", sc.player(3)->money,
              fps::to_string(sc.outcome()));
  show(sc);
  return sc.outcome() == fps::RoundOutcome::DefendersWinDefuse ? 0 : 2;
}

int run_plant_defuse() {
  banner("SCENARIO: plant at A → CT defuses");
  fps::Scenario sc;
  sc.start_default_round();
  sc.end_freeze();
  std::printf("Map sites: ");
  for (const auto& s : sc.map().sites) {
    std::printf("%s(%.0f,%.0f,%.0f) ", s.name.c_str(), s.plant_origin.x,
                s.plant_origin.y, s.plant_origin.z);
  }
  std::printf("\n");
  show(sc);

  // T1 (id 1) walks to site A and plants.
  auto* t1 = sc.player(1);
  if (!t1) {
    std::fprintf(stderr, "missing T1\n");
    return 1;
  }
  const auto* site_a = sc.map().find_site("A");
  sc.move_player(1, site_a->plant_origin);
  auto plant = sc.plant_instant(1, "A");
  std::printf("\nplant_instant: ok=%d reason=%s\n", plant.ok, plant.reason);
  show(sc);

  // CT1 walks to bomb and defuses.
  sc.move_player(3, sc.bomb().origin);
  auto def = sc.defuse_instant(3);
  std::printf("\ndefuse_instant: ok=%d reason=%s\n", def.ok, def.reason);
  show(sc);

  std::printf("\nOUTCOME: %s\n", fps::to_string(sc.outcome()));
  return sc.outcome() == fps::RoundOutcome::DefendersWinDefuse ? 0 : 2;
}

int run_plant_explode() {
  banner("SCENARIO: plant at B → fuse expires (attackers win)");
  fps::RoundConfig cfg;
  cfg.fuse_time = 5.f;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();

  sc.plant_instant(1, "B");
  std::printf("Planted at B, fuse=%.1f\n", sc.bomb().fuse_remaining);
  // Tick past fuse with no defuse.
  for (int i = 0; i < 6; ++i) {
    sc.tick(1.f);
    std::printf("  tick fuse=%.1f phase=%s\n", sc.bomb().fuse_remaining,
              // fps::to_string: fps::to_string: educational sim residual path.
                fps::to_string(sc.phase()));
  }
  show(sc);
  std::printf("OUTCOME: %s\n", fps::to_string(sc.outcome()));
  return sc.outcome() == fps::RoundOutcome::AttackersWinExplode ? 0 : 2;
}

int run_lab_bridge() {
  banner("LAB BRIDGE: FPS entities → sim::World + entity snapshots");
  fps::Scenario sc;
  sc.start_default_round();
  sc.end_freeze();
  sc.plant_instant(1, "A");

  auto snaps = fps::to_entity_snapshots(sc, /*living_only=*/false);
  std::printf("entity snapshots: %zu\n", snaps.size());
  for (const auto& e : snaps) {
    std::printf("  id=%u team=%u alive=%d pos=(%.1f,%.1f,%.1f)\n", e.id, e.team,
                e.alive ? 1 : 0, e.origin.x, e.origin.y, e.origin.z);
  }

  auto w = fps::make_lab_world_from_scenario(sc);
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  std::uint32_t count = 0;
  if (g && g->memory.size() >= 4) {
    std::memcpy(&count, g->memory.data(), sizeof(count));
  }
  std::printf("sim game pid=%u entity_count_in_mem=%u\n", game, count);
  std::printf("(T0 RPM readers can OpenProcess+ReadProcessMemory this stand-in)\n");
  return count >= 2 ? 0 : 2;
}

/// Multi-step residual after FPS bridge: RPM entity count + InterestManager
/// fog cull + InfoAdvantageScorer pre-aim hits.
int run_ac_residual() {
  banner("AC RESIDUAL: FPS bridge → RPM + fog cull + info-advantage");

  fps::Scenario sc;
  sc.start_default_round();
  sc.end_freeze();
  // T1 plants A (moves toward site); T2 stays far at T spawn so fog culls.
  const auto* site_a = sc.map().find_site("A");
  if (!site_a) {
    std::fprintf(stderr, "missing site A\n");
    return 1;
  }
  sc.move_player(1, site_a->plant_origin);
  auto plant = sc.plant_instant(1, "A");
  std::printf("plant_instant: ok=%d reason=%s\n", plant.ok, plant.reason);
  if (!plant.ok) {
    return 2;
  }

  auto w = fps::make_lab_world_from_scenario(sc);
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  if (!g) {
    std::fprintf(stderr, "no game process in lab world\n");
    return 2;
  }

  // Red path: OpenProcess + RPM-style read of entity count @ game base.
  const auto reader = w.spawn("rpm-reader.exe");
  if (!w.open_process(reader, game, sim::AccessMask::VmRead, /*syscall=*/false)) {
    std::fprintf(stderr, "open_process failed\n");
    return 2;
  }
  auto rr = w.read_mem(reader, game, g->base, sizeof(std::uint32_t),
                       /*require_handle=*/true);
  std::uint32_t count = 0;
  if (rr.status == ac::Status::Ok && rr.bytes.size() >= sizeof(count)) {
    std::memcpy(&count, rr.bytes.data(), sizeof(count));
  }
  std::printf("RPM entity_count=%u (status=%d handles=%zu)\n", count,
              static_cast<int>(rr.status), w.handles_to(game).size());

  // Blue: InterestManager cull far enemies on truth entities from scenario.
  std::vector<server::WorldEntity> truth;
  for (const auto& p : sc.players()) {
    truth.push_back(server::WorldEntity{
        p.id, p.origin, static_cast<std::uint8_t>(p.team), p.alive});
  }
  // CT observer at CT spawn — T-spawn enemies are ~80 units away.
  const auto* ctspawn = sc.map().find_area(fps::AreaId::CTSpawn);
  server::Observer obs{};
  obs.origin = ctspawn ? ctspawn->center : ac::Vec3{0.f, 0.f, 80.f};
  server::InterestManager im;
  server::InterestFilterStats fog{};
  auto vis = im.filter_for_client_stats(obs, /*observer_team=*/1, truth,
                                        /*enemy_radius=*/50.f, fog);
  std::printf("InterestManager: truth_alive=%d replicated=%d culled=%d "
              "teammates=%d multi=%d detail=%s\n",
              fog.truth_alive, fog.replicated, fog.enemies_culled, fog.teammates,
              fog.multi_reason ? 1 : 0, fog.detail.c_str());
  std::printf("  visible ids:");
  for (const auto& e : vis) {
    std::printf(" %u", e.id);
  }
  std::printf("\n");

  // Blue: InfoAdvantageScorer on 3+ pre-aim frames (no vision/audio).
  server::InfoAdvantageScorer ia;
  for (int i = 0; i < 4; ++i) {
    server::DemoFrame f;
    f.t = static_cast<double>(i) * 0.05;
    f.aim_on_hidden_target = true;
    f.has_vision_on_target = false;
    f.has_audio_on_target = false;
    ia.on_frame(f);
  }
  auto iar = ia.result();
  std::printf("InfoAdvantage: hits=%d score=%.1f max_consec=%d multi=%d\n",
              iar.hits, iar.score, iar.max_consecutive, iar.multi_reason ? 1 : 0);
  for (const auto& r : iar.reasons) {
    std::printf("  reason: %s\n", r.c_str());
  }

  const bool ok = count >= 2 && fog.enemies_culled > 0 && iar.hits > 0;
  std::printf("OUTCOME multi-reason: count>=2=%d fog_culled=%d ia_hits>0=%d → %s\n",
              count >= 2 ? 1 : 0, fog.enemies_culled > 0 ? 1 : 0,
              iar.hits > 0 ? 1 : 0, ok ? "PASS" : "FAIL");
  return ok ? 0 : 2;
}

void usage(const char* argv0) {
  std::printf(
      "Usage: %s [plant_defuse|plant_explode|drop_pickup|economy|lab_bridge|ac_residual|all]\n"
      "  plant_defuse  — attackers plant A, defenders defuse (default)\n"
      "  plant_explode — plant B, fuse runs out\n"
      "  drop_pickup   — carrier death → ground bomb → pickup → plant\n"
      "  economy       — freeze buy kit/rifle + plant bonus + round money\n"
      "  lab_bridge    — sync entities into sim::World for AC lab\n"
      "  ac_residual   — plant + RPM count + InterestManager fog + IA pre-aim\n"
      "  all           — run all scenarios\n",
      argv0);
}

}  // namespace

int main(int argc, char** argv) {
  std::printf("dummy FPS — dusty_yard attacker/defender bomb scenario\n");
  std::printf("(educational stand-in for anti-cheat-legit-radar lab)\n");

  std::string mode = "plant_defuse";
  if (argc >= 2) {
    mode = argv[1];
  }
  if (mode == "-h" || mode == "--help") {
    usage(argv[0]);
    return 0;
  }

  int rc = 0;
  if (mode == "plant_defuse" || mode == "all") {
    rc |= run_plant_defuse();
  }
  if (mode == "plant_explode" || mode == "all") {
    rc |= run_plant_explode();
  }
  if (mode == "drop_pickup" || mode == "all") {
    rc |= run_drop_pickup();
  }
  if (mode == "economy" || mode == "all") {
    rc |= run_economy();
  }
  if (mode == "lab_bridge" || mode == "all") {
    rc |= run_lab_bridge();
  }
  if (mode == "ac_residual" || mode == "all") {
    rc |= run_ac_residual();
  }
  if (mode != "plant_defuse" && mode != "plant_explode" && mode != "drop_pickup" &&
      mode != "economy" && mode != "lab_bridge" && mode != "ac_residual" &&
      mode != "all") {
    usage(argv[0]);
    return 1;
  }

  std::printf("\n%s\n", rc == 0 ? "demo OK" : "demo FAILED");
  return rc;
}
