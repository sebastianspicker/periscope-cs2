// scenario.cpp — CS-inspired bomb-defuse round FSM.
// Plant/defuse/drop/pickup, freeze economy, kit, multi win-path tick.

#include "fps/scenario.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace fps {
namespace {

float dist3(const ac::Vec3& a, const ac::Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

const char* to_string(Team t) {
  return t == Team::Attacker ? "attacker" : "defender";
}

const char* to_string(RoundPhase p) {
  switch (p) {
    case RoundPhase::Buy:
      return "buy";
    case RoundPhase::Live:
      return "live";
    case RoundPhase::BombPlanted:
      return "bomb_planted";
    case RoundPhase::RoundEnd:
      return "round_end";
  }
  return "unknown";
}

const char* to_string(RoundOutcome o) {
  switch (o) {
    case RoundOutcome::None:
      return "none";
    case RoundOutcome::AttackersWinExplode:
      return "attackers_win_explode";
    case RoundOutcome::AttackersWinEliminate:
      return "attackers_win_eliminate";
    case RoundOutcome::DefendersWinDefuse:
      return "defenders_win_defuse";
    case RoundOutcome::DefendersWinEliminate:
      return "defenders_win_eliminate";
    case RoundOutcome::DefendersWinTime:
      return "defenders_win_time";
  }
  return "unknown";
}

const char* to_string(WeaponClass w) {
  switch (w) {
    case WeaponClass::None:
      return "none";
    case WeaponClass::Pistol:
      return "pistol";
    case WeaponClass::Rifle:
      return "rifle";
    case WeaponClass::AWP:
      return "awp";
  }
  return "unknown";
}

Scenario::Scenario(Map map, RoundConfig cfg)
    : map_(std::move(map)), cfg_(cfg) {}

Player* Scenario::player(std::uint32_t id) {
  for (auto& p : players_) {
    if (p.id == id) {
      return &p;
    }
  }
  return nullptr;
}

const Player* Scenario::player(std::uint32_t id) const {
  for (const auto& p : players_) {
    if (p.id == id) {
      return &p;
    }
  }
  return nullptr;
}

void Scenario::start_default_round() {
  std::vector<Player> roster;
  const auto* tspawn = map_.find_area(AreaId::TSpawn);
  const auto* ctspawn = map_.find_area(AreaId::CTSpawn);
  const ac::Vec3 t0 = tspawn ? tspawn->center : ac::Vec3{0, 0, 0};
  const ac::Vec3 ct0 = ctspawn ? ctspawn->center : ac::Vec3{0, 0, 80};

  roster.push_back(
      Player{0, "T1", Team::Attacker, t0, true, true, 100.f, cfg_.start_money,
             false, WeaponClass::Pistol});
  roster.push_back(Player{0, "T2", Team::Attacker, {t0.x + 3.f, 0, t0.z}, true,
                          false, 100.f, cfg_.start_money, false,
                          WeaponClass::Pistol});
  roster.push_back(Player{0, "CT1", Team::Defender, ct0, true, false, 100.f,
                          cfg_.start_money, false, WeaponClass::Pistol});
  roster.push_back(Player{0, "CT2", Team::Defender, {ct0.x - 3.f, 0, ct0.z}, true,
                          false, 100.f, cfg_.start_money, false,
                          WeaponClass::Pistol});
  start_round(std::move(roster), /*skip_freeze=*/false);
}

void Scenario::start_round(std::vector<Player> roster, bool skip_freeze) {
  players_.clear();
  next_id_ = 1;
  bomb_ = {};
  outcome_ = RoundOutcome::None;
  plant_bonus_paid_ = false;
  for (auto p : roster) {
    p.id = next_id_++;
    if (!p.alive) {
      p.health = 0.f;
    }
    if (p.money <= 0) {
      p.money = cfg_.start_money;
    }
    players_.push_back(p);
  }
  // Exactly one bomb: if none carry, give to first living attacker.
  int carriers = 0;
  for (const auto& p : players_) {
    if (p.carries_bomb) {
      ++carriers;
    }
  }
  if (carriers == 0) {
    for (auto& p : players_) {
      if (p.team == Team::Attacker && p.alive) {
        p.carries_bomb = true;
        break;
      }
    }
  } else if (carriers > 1) {
    bool keep = false;
    for (auto& p : players_) {
      if (p.carries_bomb) {
        if (!keep) {
          keep = true;
        } else {
          p.carries_bomb = false;
        }
      }
    }
  }

  if (skip_freeze || cfg_.freeze_time <= 0.f) {
    phase_ = RoundPhase::Live;
    freeze_clock_ = 0.f;
    clock_ = cfg_.round_time;
  } else {
    phase_ = RoundPhase::Buy;
    freeze_clock_ = cfg_.freeze_time;
    clock_ = cfg_.round_time;
  }
}

ActionResult Scenario::end_freeze() {
  if (phase_ != RoundPhase::Buy) {
    return {false, "not_buy"};
  }
  freeze_clock_ = 0.f;
  phase_ = RoundPhase::Live;
  clock_ = cfg_.round_time;
  return {true, "live"};
}

ActionResult Scenario::move_player(std::uint32_t id, const ac::Vec3& to) {
  if (phase_ == RoundPhase::RoundEnd) {
    return {false, "round_over"};
  }
  // Free movement allowed in Buy (CS freeze still allows aim/buy in lab).
  auto* p = player(id);
  if (!p) {
    return {false, "no_player"};
  }
  if (!p->alive) {
    return {false, "dead"};
  }
  p->origin = to;
  if (bomb_.plant_in_progress && bomb_.planter_id == id) {
    if (!map_.site_at(p->origin)) {
      cancel_plant();
    }
  }
  if (bomb_.defuse_in_progress && bomb_.defuser_id == id) {
    if (!near_bomb_(*p)) {
      cancel_defuse();
    }
  }
  return {true, "moved"};
}

ActionResult Scenario::buy_defuse_kit(std::uint32_t defender_id) {
  if (phase_ == RoundPhase::RoundEnd || phase_ == RoundPhase::BombPlanted) {
    return {false, "cannot_buy_now"};
  }
  auto* p = player(defender_id);
  if (!p || !p->alive) {
    return {false, "dead_or_missing"};
  }
  if (p->team != Team::Defender) {
    return {false, "not_defender"};
  }
  if (p->has_defuse_kit) {
    return {false, "already_has_kit"};
  }
  if (p->money < cfg_.kit_cost) {
    return {false, "not_enough_money"};
  }
  p->money -= cfg_.kit_cost;
  p->has_defuse_kit = true;
  return {true, "bought_kit"};
}

ActionResult Scenario::buy_rifle(std::uint32_t player_id) {
  if (phase_ == RoundPhase::RoundEnd) {
    return {false, "cannot_buy_now"};
  }
  auto* p = player(player_id);
  if (!p || !p->alive) {
    return {false, "dead_or_missing"};
  }
  if (p->money < cfg_.rifle_cost) {
    return {false, "not_enough_money"};
  }
  p->money -= cfg_.rifle_cost;
  p->weapon = WeaponClass::Rifle;
  return {true, "bought_rifle"};
}

bool Scenario::near_bomb_(const Player& p) const {
  if (bomb_.planted) {
    return dist3(p.origin, bomb_.origin) <= cfg_.max_plant_range;
  }
  if (bomb_.ground.present) {
    return dist3(p.origin, bomb_.ground.origin) <= cfg_.max_pickup_range;
  }
  return false;
}

ActionResult Scenario::begin_plant(std::uint32_t attacker_id) {
  if (phase_ != RoundPhase::Live) {
    return {false, "not_live"};
  }
  if (bomb_.planted) {
    return {false, "already_planted"};
  }
  auto* p = player(attacker_id);
  if (!p || !p->alive) {
    return {false, "dead_or_missing"};
  }
  if (p->team != Team::Attacker) {
    return {false, "not_attacker"};
  }
  if (!p->carries_bomb) {
    return {false, "no_bomb"};
  }
  const auto* site = map_.site_at(p->origin);
  if (!site) {
    return {false, "not_at_site"};
  }
  bomb_.plant_in_progress = true;
  bomb_.planter_id = attacker_id;
  bomb_.site_name = site->name;
  bomb_.origin = site->plant_origin;
  return {true, "planting"};
}

ActionResult Scenario::cancel_plant() {
  if (!bomb_.plant_in_progress) {
    return {false, "not_planting"};
  }
  bomb_.plant_in_progress = false;
  bomb_.plant_progress = 0.f;
  bomb_.planter_id = 0;
  return {true, "plant_cancelled"};
}

ActionResult Scenario::begin_defuse(std::uint32_t defender_id) {
  if (phase_ != RoundPhase::BombPlanted) {
    return {false, "bomb_not_planted"};
  }
  if (bomb_.defused || bomb_.exploded) {
    return {false, "bomb_resolved"};
  }
  auto* p = player(defender_id);
  if (!p || !p->alive) {
    return {false, "dead_or_missing"};
  }
  if (p->team != Team::Defender) {
    return {false, "not_defender"};
  }
  if (!near_bomb_(*p)) {
    return {false, "too_far"};
  }
  bomb_.defuse_in_progress = true;
  bomb_.defuser_id = defender_id;
  return {true, "defusing"};
}

ActionResult Scenario::cancel_defuse() {
  if (!bomb_.defuse_in_progress) {
    return {false, "not_defusing"};
  }
  bomb_.defuse_in_progress = false;
  bomb_.defuse_progress = 0.f;
  bomb_.defuser_id = 0;
  return {true, "defuse_cancelled"};
}

ActionResult Scenario::plant_instant(std::uint32_t attacker_id,
                                     const std::string& site_name) {
  if (phase_ == RoundPhase::RoundEnd) {
    return {false, "round_over"};
  }
  // Demo/test helper: end freeze so plant_instant works from Buy, but still
  // enforce team/site/carrier via begin_plant (never an always-true stub).
  if (phase_ == RoundPhase::Buy) {
    end_freeze();
  }
  if (phase_ != RoundPhase::Live) {
    return {false, "not_live"};
  }
  auto* p = player(attacker_id);
  if (!p) {
    return {false, "no_player"};
  }
  if (!p->alive) {
    return {false, "dead_or_missing"};
  }
  if (p->team != Team::Attacker) {
    return {false, "not_attacker"};
  }
  if (!p->carries_bomb) {
    return {false, "no_bomb"};
  }
  const auto* site = map_.find_site(site_name);
  if (!site) {
    return {false, "bad_site"};
  }
  p->origin = site->plant_origin;
  auto r = begin_plant(attacker_id);
  if (!r.ok) {
    return r;
  }
  bomb_.planted = true;
  bomb_.plant_in_progress = false;
  bomb_.plant_progress = 0.f;
  bomb_.fuse_remaining = cfg_.fuse_time;
  bomb_.origin = site->plant_origin;
  bomb_.site_name = site->name;
  bomb_.ground = {};
  p->carries_bomb = false;
  phase_ = RoundPhase::BombPlanted;
  if (!plant_bonus_paid_) {
    for (auto& pl : players_) {
      if (pl.team == Team::Attacker && pl.alive) {
        pl.money += cfg_.plant_bonus;
      }
    }
    plant_bonus_paid_ = true;
  }
  return {true, "planted"};
}

ActionResult Scenario::defuse_instant(std::uint32_t defender_id) {
  if (phase_ == RoundPhase::RoundEnd) {
    return {false, "round_over"};
  }
  auto* p = player(defender_id);
  if (!p) {
    return {false, "no_player"};
  }
  if (!p->alive) {
    return {false, "dead_or_missing"};
  }
  if (p->team != Team::Defender) {
    return {false, "not_defender"};
  }
  if (!bomb_.planted || bomb_.defused || bomb_.exploded) {
    return {false, "bomb_not_planted"};
  }
  if (phase_ != RoundPhase::BombPlanted) {
    return {false, "bomb_not_planted"};
  }
  p->origin = bomb_.origin;
  auto r = begin_defuse(defender_id);
  if (!r.ok) {
    return r;
  }
  bomb_.defused = true;
  bomb_.defuse_in_progress = false;
  bomb_.defuse_progress = 0.f;
  bomb_.defuser_id = defender_id;
  finish_(RoundOutcome::DefendersWinDefuse);
  return {true, "defused"};
}

ActionResult Scenario::pickup_bomb(std::uint32_t attacker_id) {
  if (phase_ == RoundPhase::RoundEnd || bomb_.planted) {
    return {false, "cannot_pickup"};
  }
  if (!bomb_.ground.present) {
    return {false, "no_ground_bomb"};
  }
  auto* p = player(attacker_id);
  if (!p || !p->alive) {
    return {false, "dead_or_missing"};
  }
  if (p->team != Team::Attacker) {
    return {false, "not_attacker"};
  }
  if (dist3(p->origin, bomb_.ground.origin) > cfg_.max_pickup_range) {
    return {false, "too_far"};
  }
  // One bomb only: clear other carriers (should be none).
  for (auto& o : players_) {
    o.carries_bomb = false;
  }
  p->carries_bomb = true;
  bomb_.ground = {};
  return {true, "picked_up"};
}

float Scenario::effective_defuse_time(std::uint32_t defender_id) const {
  const auto* p = player(defender_id);
  if (p && p->has_defuse_kit) {
    return cfg_.defuse_time_kit;
  }
  return cfg_.defuse_time;
}

int Scenario::alive_count(Team t) const {
  int n = 0;
  for (const auto& p : players_) {
    if (p.team == t && p.alive) {
      ++n;
    }
  }
  return n;
}

std::uint32_t Scenario::bomb_carrier_id() const {
  for (const auto& p : players_) {
    if (p.carries_bomb) {
      return p.id;
    }
  }
  return 0;
}

std::string Scenario::describe() const {
  std::ostringstream oss;
  oss << "map=" << map_.name << " phase=" << to_string(phase_)
      << " clock=" << clock_ << " freeze=" << freeze_clock_
      << " outcome=" << to_string(outcome_)
      << " score=T" << score_.attacker_wins << "-CT" << score_.defender_wins
      << "\n";
  oss << "  bomb: planted=" << (bomb_.planted ? 1 : 0)
      << " site=" << (bomb_.site_name.empty() ? "-" : bomb_.site_name)
      << " fuse=" << bomb_.fuse_remaining
      << " ground=" << (bomb_.ground.present ? 1 : 0)
      << " carrier=" << bomb_carrier_id()
      << " defused=" << (bomb_.defused ? 1 : 0)
      << " exploded=" << (bomb_.exploded ? 1 : 0) << "\n";
  for (const auto& p : players_) {
    oss << "  [" << p.id << "] " << p.name << " " << to_string(p.team)
        << " alive=" << (p.alive ? 1 : 0) << " bomb=" << (p.carries_bomb ? 1 : 0)
        << " $=" << p.money << " kit=" << (p.has_defuse_kit ? 1 : 0)
        << " weap=" << to_string(p.weapon) << " hp=" << p.health
        << " pos=(" << p.origin.x << "," << p.origin.y << "," << p.origin.z
        << ")\n";
  }
  return oss.str();
}

}  // namespace fps
