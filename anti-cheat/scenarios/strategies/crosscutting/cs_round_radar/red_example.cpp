// RED: fps::Scenario (planted CS round) → lab_bridge → external RPM product.

#include "red_example.hpp"

#include "fps/lab_bridge.hpp"

#include <cstring>
#include <sstream>

namespace examples::cs_round_radar {
namespace {

struct EntRaw {
  float x, y, z;
  std::uint8_t team, alive, pad[2];
};

void pull_entities_from_world(sim::World& w, std::uint32_t actor, RedResult& r) {
  r.entities.clear();
  auto* g = w.proc(w.game_pid());
  if (!g || g->memory.size() < 4) {
    return;
  }
  auto count_rr =
      w.read_mem(actor, w.game_pid(), g->base, sizeof(std::uint32_t), true);
  if (count_rr.status != ac::Status::Ok ||
      count_rr.bytes.size() < sizeof(std::uint32_t)) {
    return;
  }
  std::uint32_t count = 0;
  std::memcpy(&count, count_rr.bytes.data(), sizeof(count));
  if (count > 32) {
    count = 32;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto addr = g->base + 0x10 + static_cast<std::uint64_t>(i) * sizeof(EntRaw);
    auto er = w.read_mem(actor, w.game_pid(), addr, sizeof(EntRaw), true);
    if (er.status != ac::Status::Ok || er.bytes.size() < sizeof(EntRaw)) {
      break;
    }
    EntRaw raw{};
    std::memcpy(&raw, er.bytes.data(), sizeof(raw));
    ac::EntitySnapshot snap;
    snap.id = i + 1;
    snap.origin = {raw.x, raw.y, raw.z};
    snap.team = raw.team;
    snap.alive = raw.alive != 0;
    r.entities.push_back(snap);
    if (snap.alive && snap.team == 0) {
      ++r.attackers_alive;
    }
    if (snap.alive && snap.team == 1) {
      ++r.defenders_alive;
    }
  }
  r.entity_count = static_cast<int>(r.entities.size());
}

}  // namespace

RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w) {
  RedResult r;
  // Ensure Live + plant so positions span sites (CS-shaped layout).
  if (sc.phase() == fps::RoundPhase::Buy) {
    (void)sc.end_freeze();
  }
  if (!sc.bomb().planted) {
    (void)sc.plant_instant(1, "A");
  }
  // Bridge scenario players into World game memory (replaces static ACPT table).
  w = fps::make_lab_world_from_scenario(sc, "dusty-fps.exe");
  r.bomb_planted = sc.bomb().planted;
  r.site = sc.bomb().site_name;

  const auto actor = w.spawn("cs-round-radar.exe");
  r.actor_pid = actor;
  if (!w.open_process(actor, w.game_pid(), sim::AccessMask::VmRead, false)) {
    r.detail = "cs_round_radar red open_process failed";
    w.note(r.detail);
    return r;
  }
  // Full-origin client product want (structural pressure for fog blue).
  w.server_sends_full_enemy_origin = true;
  w.client_entity_fidelity = 1.0f;

  pull_entities_from_world(w, actor, r);
  r.achieved = r.entity_count >= 2 && r.attackers_alive >= 1 &&
               r.defenders_alive >= 1 && r.bomb_planted;
  std::ostringstream oss;
  oss << "cs_round_radar red site=" << r.site
      << " ents=" << r.entity_count << " T_alive=" << r.attackers_alive
      << " CT_alive=" << r.defenders_alive
      << " planted=" << (r.bomb_planted ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

RedResult apply(sim::World& w) {
  fps::RoundConfig cfg;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();
  // T2 stays far at T spawn so CT-side fog culls unobservable Ts.
  return apply_from_scenario(sc, w);
}

}  // namespace examples::cs_round_radar
