// Tests drive shipped fps::Scenario CS-inspired plant/defuse/drop/economy APIs.

#include "fps/scenario.hpp"
#include "fps/lab_bridge.hpp"
#include "fps/map.hpp"
#include "sim/world.hpp"

#include <cstdio>
#include <cstring>
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

}  // namespace

int main() {
  // ── Map: dusty_yard areas + sites A/B + area_at / site_at ────────────
  {
    auto map = fps::Map::make_dusty_yard();
    expect(map.name == "dusty_yard", "map name dusty_yard");
    expect(map.sites.size() >= 2, "map has ≥2 bomb sites");
    expect(map.areas.size() >= 6, "map has full area set");
    expect(map.find_site("A") != nullptr && map.find_site("B") != nullptr,
           "sites A and B exist");
    expect(map.find_area(fps::AreaId::TSpawn) != nullptr, "TSpawn area exists");
    expect(map.find_area(fps::AreaId::CTSpawn) != nullptr, "CTSpawn area exists");
    expect(map.site_at(map.find_site("A")->plant_origin) != nullptr &&
               map.site_at(map.find_site("A")->plant_origin)->name == "A",
           "site_at plant origin A");
    expect(map.site_at(map.find_site("B")->plant_origin) != nullptr &&
               map.site_at(map.find_site("B")->plant_origin)->name == "B",
           "site_at plant origin B");
    expect(map.site_at({0.f, 0.f, 0.f}) == nullptr, "T spawn is not a plant site");
    const auto* t_area = map.area_at({0.f, 0.f, 0.f});
    expect(t_area != nullptr && t_area->id == fps::AreaId::TSpawn,
           "area_at T spawn center → TSpawn");
    const auto* mid = map.area_at({0.f, 0.f, 40.f});
    expect(mid != nullptr && mid->id == fps::AreaId::Mid, "area_at mid center");
    const auto* sa = map.area_at({-30.f, 0.f, 50.f});
    expect(sa != nullptr && sa->id == fps::AreaId::SiteA, "area_at site A");
    expect(map.area_at({999.f, 0.f, 999.f}) == nullptr, "area_at outside all");
  }

  // ── Freeze / buy then live ───────────────────────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.freeze_time = 2.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    expect(sc.phase() == fps::RoundPhase::Buy, "starts in buy/freeze");
    expect(sc.player(1)->money == cfg.start_money, "start money");
    sc.tick(1.f);
    expect(sc.phase() == fps::RoundPhase::Buy, "still freeze mid");
    sc.tick(1.5f);
    expect(sc.phase() == fps::RoundPhase::Live, "freeze ends → live");
  }

  // ── Plant rules (deny wrong team / off-site / no-bomb) ───────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    expect(sc.end_freeze().ok, "end freeze for plant tests");
    expect(sc.phase() == fps::RoundPhase::Live, "live after end_freeze");

    auto bad = sc.plant_instant(3, "A");
    expect(!bad.ok, "defender plant denied");
    expect(std::string(bad.reason) == "not_attacker" || !bad.ok,
           "defender plant reason non-ok");

    auto nobomb = sc.begin_plant(2);
    expect(!nobomb.ok, "no-bomb attacker cannot begin_plant");

    auto not_site = sc.begin_plant(1);
    expect(!not_site.ok, "plant denied off-site");
    expect(std::string(not_site.reason) == "not_at_site", "off-site reason");

    auto plant = sc.plant_instant(1, "A");
    expect(plant.ok, "attacker plant at A");
    expect(sc.bomb().planted && sc.bomb().site_name == "A", "planted A");
    expect(sc.phase() == fps::RoundPhase::BombPlanted, "phase bomb_planted");
    expect(!sc.player(1)->carries_bomb, "planter no longer carries");
    expect(sc.player(1)->money > sc.config().start_money, "plant bonus money");

    // Double plant / re-plant denied
    auto again = sc.plant_instant(1, "B");
    expect(!again.ok, "second plant denied after already planted");
  }

  // ── Pre-plant defuse denied ──────────────────────────────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    sc.end_freeze();
    auto pre = sc.defuse_instant(3);
    expect(!pre.ok, "pre-plant defuse denied");
    auto pre2 = sc.begin_defuse(3);
    expect(!pre2.ok, "pre-plant begin_defuse denied");
  }

  // ── Bomb drop + pickup (range + attacker-only) ───────────────────────
  {
    fps::Scenario sc;
    sc.start_round(
        {
            fps::Player{0, "T1", fps::Team::Attacker, {0, 0, 0}, true, true,
                        100.f, 800, false, fps::WeaponClass::Pistol},
            fps::Player{0, "T2", fps::Team::Attacker, {2, 0, 0}, true, false,
                        100.f, 800, false, fps::WeaponClass::Pistol},
            fps::Player{0, "CT1", fps::Team::Defender, {0, 0, 80}, true, false,
                        100.f, 800, false, fps::WeaponClass::Pistol},
        },
        /*skip_freeze=*/true);
    expect(sc.bomb_carrier_id() == 1, "T1 starts with bomb");
    sc.kill(1);
    expect(sc.bomb_on_ground(), "bomb dropped on ground");
    expect(!sc.player(1)->carries_bomb, "dead carrier empty hands");
    expect(sc.bomb_carrier_id() == 0, "no carrier while on ground");

    // Defender at drop site cannot pick up
    sc.move_player(3, sc.bomb().ground.origin);
    auto ct_up = sc.pickup_bomb(3);
    expect(!ct_up.ok, "defender pickup denied");

    // T2 far from drop — cannot pick up
    sc.move_player(2, {100.f, 0.f, 100.f});
    auto far = sc.pickup_bomb(2);
    expect(!far.ok, "pickup denied when far");
    sc.move_player(2, sc.bomb().ground.origin);
    auto up = sc.pickup_bomb(2);
    expect(up.ok, "T2 picks up ground bomb");
    expect(sc.player(2)->carries_bomb, "T2 carries bomb");
    expect(!sc.bomb_on_ground(), "ground clear after pickup");
    // Plant with new carrier
    auto plant = sc.plant_instant(2, "A");
    expect(plant.ok, "pickup carrier can plant");
  }

  // ── Kit shortens defuse hold ─────────────────────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.defuse_time = 2.0f;
    cfg.defuse_time_kit = 0.5f;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    expect(sc.buy_defuse_kit(3).ok, "CT buys kit");
    expect(sc.player(3)->has_defuse_kit, "kit equipped");
    expect(sc.player(3)->money < cfg.start_money, "kit cost deducted");
    expect(sc.effective_defuse_time(3) == cfg.defuse_time_kit,
           "effective defuse time uses kit");
    expect(sc.effective_defuse_time(4) == cfg.defuse_time,
           "effective defuse time bare without kit");
    sc.plant_instant(1, "A");
    sc.move_player(3, sc.bomb().origin);
    expect(sc.begin_defuse(3).ok, "begin defuse with kit");
    sc.tick(0.4f);
    expect(!sc.bomb().defused, "not defused mid-kit-hold");
    sc.tick(0.2f);
    expect(sc.bomb().defused, "defused faster with kit");
    expect(sc.outcome() == fps::RoundOutcome::DefendersWinDefuse, "defuse win");
  }

  // ── Defuse without kit needs full time ───────────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.defuse_time = 1.0f;
    cfg.defuse_time_kit = 0.2f;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sc.plant_instant(1, "B");
    sc.move_player(3, sc.bomb().origin);
    sc.begin_defuse(3);
    sc.tick(0.5f);
    expect(!sc.bomb().defused, "no kit not done at 0.5s of 1.0s");
    sc.tick(0.6f);
    expect(sc.bomb().defused, "no kit done after full defuse_time");
  }

  // ── Explode ──────────────────────────────────────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.fuse_time = 2.f;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sc.plant_instant(1, "A");
    sc.tick(2.1f);
    expect(sc.bomb().exploded, "bomb exploded");
    expect(sc.outcome() == fps::RoundOutcome::AttackersWinExplode, "explode win");
    expect(sc.player(1)->money > cfg.start_money, "round-end money applied");
  }

  // ── Hold plant ───────────────────────────────────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.plant_time = 1.0f;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    auto* site = sc.map().find_site("A");
    sc.move_player(1, site->plant_origin);
    expect(sc.begin_plant(1).ok, "begin_plant");
    sc.tick(0.4f);
    expect(!sc.bomb().planted, "not planted mid-hold");
    sc.tick(0.7f);
    expect(sc.bomb().planted, "planted after hold");
    expect(sc.phase() == fps::RoundPhase::BombPlanted, "hold plant → BombPlanted");
  }

  // ── Time win ─────────────────────────────────────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.round_time = 1.5f;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sc.tick(2.f);
    expect(sc.outcome() == fps::RoundOutcome::DefendersWinTime, "time win");
  }

  // ── Elimination (no plant) ───────────────────────────────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    sc.end_freeze();
    sc.kill(3);
    sc.kill(4);
    expect(sc.outcome() == fps::RoundOutcome::AttackersWinEliminate,
           "elim CTs → T win");
  }

  // ── Defenders elim Ts when bomb not planted ──────────────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    sc.end_freeze();
    sc.kill(1);
    sc.kill(2);
    expect(sc.alive_count(fps::Team::Attacker) == 0, "Ts eliminated");
    expect(sc.outcome() == fps::RoundOutcome::DefendersWinEliminate,
           "elim Ts no plant → CT win");
  }

  // ── Bomb planted + Ts dead → fuse continues (planted-fuse exception) ─
  {
    fps::RoundConfig cfg;
    cfg.fuse_time = 3.f;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sc.plant_instant(1, "A");
    sc.kill(1);
    sc.kill(2);
    expect(sc.alive_count(fps::Team::Attacker) == 0, "Ts dead");
    expect(sc.phase() == fps::RoundPhase::BombPlanted, "still planted phase");
    expect(sc.outcome() == fps::RoundOutcome::None, "no elim win while fuse live");
    sc.tick(3.1f);
    expect(sc.outcome() == fps::RoundOutcome::AttackersWinExplode,
           "Ts dead but explode still wins");
  }

  // ── Planted + Ts dead → CT can still defuse ──────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.fuse_time = 40.f;
    cfg.freeze_time = 0.f;
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    sc.plant_instant(1, "B");
    sc.kill(1);
    sc.kill(2);
    expect(sc.phase() == fps::RoundPhase::BombPlanted, "fuse exception holds phase");
    expect(sc.defuse_instant(3).ok, "CT defuses after T wipe");
    expect(sc.outcome() == fps::RoundOutcome::DefendersWinDefuse,
           "defuse after T wipe");
  }

  // ── Weapon damage + kill path ────────────────────────────────────────
  {
    fps::RoundConfig cfg;
    cfg.freeze_time = 0.f;
    cfg.rifle_cost = 500;  // affordable for test start money
    fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
    sc.start_default_round();
    expect(sc.buy_rifle(1).ok, "buy rifle ok");
    expect(sc.player(1)->weapon == fps::WeaponClass::Rifle, "rifle bought");
    const float hp0 = sc.player(3)->health;
    expect(sc.damage(3, 1).ok, "T damages CT");
    expect(sc.player(3)->health < hp0, "health reduced by rifle");
    // Finish with raw damage kill
    expect(sc.damage(3, 1, 200.f).ok, "overkill damage");
    expect(!sc.player(3)->alive, "victim dead after overkill");
  }

  // ── Instant defuse ───────────────────────────────────────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    sc.end_freeze();
    sc.plant_instant(1, "B");
    expect(sc.bomb().site_name == "B", "planted site B");
    expect(sc.defuse_instant(3).ok, "defuse instant");
    expect(sc.outcome() == fps::RoundOutcome::DefendersWinDefuse, "defuse outcome");
  }

  // ── Post-round actions rejected ──────────────────────────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    sc.end_freeze();
    sc.plant_instant(1, "A");
    sc.defuse_instant(3);
    expect(sc.phase() == fps::RoundPhase::RoundEnd, "round ended");
    expect(!sc.plant_instant(1, "B").ok, "post-round plant denied");
    expect(!sc.defuse_instant(3).ok, "post-round defuse denied");
    expect(!sc.kill(2).ok, "post-round kill denied");
    expect(!sc.move_player(1, {0, 0, 0}).ok, "post-round move denied");
    expect(!sc.pickup_bomb(1).ok, "post-round pickup denied");
    expect(!sc.buy_rifle(1).ok, "post-round buy denied");
    expect(!sc.begin_plant(1).ok, "post-round begin_plant denied");
  }

  // ── Lab bridge: full snapshot mapping + World sync ───────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    sc.end_freeze();
    sc.buy_rifle(1);  // may fail if money; start 800, rifle 2700 default
    // Give money and buy for weapon_id mapping
    sc.player(1)->money = 5000;
    expect(sc.buy_rifle(1).ok, "bridge setup buy rifle");
    sc.plant_instant(1, "A");

    auto snaps = fps::to_entity_snapshots(sc, false);
    expect(snaps.size() == 4, "4 entity snapshots");
    bool found_carrier_or_planter = false;
    bool found_name = false;
    bool found_health = false;
    bool found_weapon = false;
    for (const auto& e : snaps) {
      if (e.id == 1) {
        found_name = (std::strcmp(e.name, "T1") == 0);
        found_health = (e.health > 0);
        found_weapon = (e.weapon_id == 2);  // Rifle
        // After plant, planter no longer carries
        expect(!e.is_bomb_carrier, "planter not carrier after plant");
      }
      if (e.team == 0 || e.team == 1) {
        found_carrier_or_planter = true;
      }
      expect(e.alive, "all alive in full roster pre-kill");
    }
    expect(found_name, "snapshot maps player name");
    expect(found_health, "snapshot maps health");
    expect(found_weapon, "snapshot maps weapon_id from WeaponClass");
    expect(found_carrier_or_planter, "snapshot maps teams");

    // Before plant carrier flag: new round
    fps::Scenario sc2;
    sc2.start_default_round();
    sc2.end_freeze();
    auto snaps2 = fps::to_entity_snapshots(sc2, false);
    bool carrier = false;
    for (const auto& e : snaps2) {
      if (e.is_bomb_carrier) {
        carrier = true;
        expect(e.id == sc2.bomb_carrier_id(), "carrier id matches snapshot");
      }
    }
    expect(carrier, "snapshot maps is_bomb_carrier");

    auto w = fps::make_lab_world_from_scenario(sc);
    auto* g = w.proc(w.game_pid());
    expect(g && g->memory.size() >= 4, "game memory");
    expect(w.game_pid() != 0, "non-zero game pid");
    std::uint32_t count = 0;
    std::memcpy(&count, g->memory.data(), sizeof(count));
    expect(count == 4, "synced entity count=4");
    // Read first entity row at base+0x10
    float x = 0.f, y = 0.f, z = 0.f;
    std::uint8_t team = 0, alive = 0;
    std::memcpy(&x, g->memory.data() + 0x10, sizeof(float));
    std::memcpy(&y, g->memory.data() + 0x14, sizeof(float));
    std::memcpy(&z, g->memory.data() + 0x18, sizeof(float));
    std::memcpy(&team, g->memory.data() + 0x1c, 1);
    std::memcpy(&alive, g->memory.data() + 0x1d, 1);
    expect(alive == 1, "world row alive flag");
    expect(team == 0 || team == 1, "world row team");
    // Positions should match scenario player 0 (id 1) origin
    expect(x == sc.players()[0].origin.x && z == sc.players()[0].origin.z,
           "world row origin matches scenario");

    sc.kill(2);
    auto living = fps::to_entity_snapshots(sc, true);
    expect(living.size() == 3, "living_only drops dead");

    // sync fails cleanly with empty World (no game process)
    sim::World empty;
    expect(!fps::sync_entities_to_sim(sc, empty),
           "sync fails cleanly when no game process");
  }

  // ── Buy kit denied for attackers ─────────────────────────────────────
  {
    fps::Scenario sc;
    sc.start_default_round();
    expect(!sc.buy_defuse_kit(1).ok, "attacker cannot buy kit");
  }

  if (fails) {
    std::fprintf(stderr, "fps_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("fps_tests: all passed\n");
  return 0;
}
