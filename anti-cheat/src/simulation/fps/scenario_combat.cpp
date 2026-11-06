#include "fps/scenario.hpp"

namespace fps {

float Scenario::weapon_damage_(WeaponClass w) const {
  switch (w) {
    case WeaponClass::Pistol:
      return cfg_.pistol_damage;
    case WeaponClass::Rifle:
      return cfg_.rifle_damage;
    case WeaponClass::AWP:
      return cfg_.awp_damage;
    default:
      return cfg_.pistol_damage;
  }
}

ActionResult Scenario::damage(std::uint32_t victim_id, std::uint32_t attacker_id,
                              float raw_damage) {
  if (phase_ == RoundPhase::RoundEnd || phase_ == RoundPhase::Buy) {
    return {false, "cannot_damage_now"};
  }
  auto* v = player(victim_id);
  auto* a = player(attacker_id);
  if (!v || !v->alive) {
    return {false, "victim_dead"};
  }
  if (!a || !a->alive) {
    return {false, "attacker_dead"};
  }
  if (a->team == v->team) {
    return {false, "friendly_fire_off"};
  }
  const float dmg =
      raw_damage > 0.f ? raw_damage : weapon_damage_(a->weapon);
  v->health -= dmg;
  if (v->health <= 0.f) {
    return kill(victim_id, attacker_id);
  }
  return {true, "damaged"};
}

ActionResult Scenario::kill(std::uint32_t victim_id, std::uint32_t /*killer*/) {
  if (phase_ == RoundPhase::RoundEnd) {
    return {false, "round_over"};
  }
  auto* v = player(victim_id);
  if (!v || !v->alive) {
    return {false, "already_dead"};
  }
  v->alive = false;
  v->health = 0.f;
  // CS-like: carrier death drops bomb on ground for another T to pick up.
  if (v->carries_bomb) {
    v->carries_bomb = false;
    bomb_.ground.present = true;
    bomb_.ground.origin = v->origin;
  }
  if (bomb_.plant_in_progress && bomb_.planter_id == victim_id) {
    cancel_plant();
  }
  if (bomb_.defuse_in_progress && bomb_.defuser_id == victim_id) {
    cancel_defuse();
  }
  resolve_elimination_();
  return {true, "killed"};
}

void Scenario::tick(float dt) {
  if (dt < 0.f) {
    dt = 0.f;
  }
  if (phase_ == RoundPhase::RoundEnd) {
    return;
  }

  // Freeze / buy → Live
  if (phase_ == RoundPhase::Buy) {
    freeze_clock_ -= dt;
    if (freeze_clock_ <= 0.f) {
      freeze_clock_ = 0.f;
      phase_ = RoundPhase::Live;
      clock_ = cfg_.round_time;
    }
    return;
  }

  // Round clock only in Live (no plant). After plant, fuse owns the clock.
  if (phase_ == RoundPhase::Live) {
    clock_ -= dt;
    if (clock_ <= 0.f) {
      clock_ = 0.f;
      resolve_time_();
      return;
    }
  }

  // Plant progress
  if (bomb_.plant_in_progress && phase_ == RoundPhase::Live) {
    auto* p = player(bomb_.planter_id);
    if (!p || !p->alive || !p->carries_bomb || !map_.site_at(p->origin)) {
      cancel_plant();
    } else {
      bomb_.plant_progress += dt;
      if (bomb_.plant_progress >= cfg_.plant_time) {
        bomb_.planted = true;
        bomb_.plant_in_progress = false;
        bomb_.plant_progress = 0.f;
        bomb_.fuse_remaining = cfg_.fuse_time;
        const auto* site = map_.site_at(p->origin);
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
      }
    }
  }

  // Defuse progress (kit shortens hold)
  if (bomb_.defuse_in_progress && phase_ == RoundPhase::BombPlanted) {
    auto* p = player(bomb_.defuser_id);
    if (!p || !p->alive || !near_bomb_(*p)) {
      cancel_defuse();
    } else {
      bomb_.defuse_progress += dt;
      const float need = effective_defuse_time(bomb_.defuser_id);
      if (bomb_.defuse_progress >= need) {
        bomb_.defused = true;
        bomb_.defuse_in_progress = false;
        finish_(RoundOutcome::DefendersWinDefuse);
        return;
      }
    }
  }

  // Fuse
  if (phase_ == RoundPhase::BombPlanted && bomb_.planted && !bomb_.defused) {
    bomb_.fuse_remaining -= dt;
    if (bomb_.fuse_remaining <= 0.f) {
      bomb_.fuse_remaining = 0.f;
      bomb_.exploded = true;
      finish_(RoundOutcome::AttackersWinExplode);
      return;
    }
  }

  resolve_elimination_();
}

void Scenario::resolve_elimination_() {
  if (phase_ == RoundPhase::RoundEnd || phase_ == RoundPhase::Buy) {
    return;
  }
  const int atk = alive_count(Team::Attacker);
  const int def = alive_count(Team::Defender);
  // Planted-fuse exception: all attackers dead with bomb planted does NOT
  // grant defender eliminate — fuse still decides explode vs defuse.
  if (atk == 0 && bomb_.planted && !bomb_.defused && !bomb_.exploded &&
      phase_ == RoundPhase::BombPlanted) {
    return;
  }
  if (def == 0 && atk > 0) {
    finish_(RoundOutcome::AttackersWinEliminate);
    return;
  }
  if (atk == 0 && def > 0) {
    finish_(RoundOutcome::DefendersWinEliminate);
    return;
  }
  // Both teams wiped without a planted bomb: no legitimate CS win path —
  // treat as defender time/stalemate only if still Live with no plant.
  if (atk == 0 && def == 0 && !bomb_.planted) {
    finish_(RoundOutcome::DefendersWinEliminate);
  }
}

void Scenario::resolve_time_() {
  if (phase_ == RoundPhase::Live && !bomb_.planted) {
    finish_(RoundOutcome::DefendersWinTime);
  }
}

void Scenario::apply_round_money_(RoundOutcome o) {
  const bool atk_win =
      o == RoundOutcome::AttackersWinExplode ||
      o == RoundOutcome::AttackersWinEliminate;
  for (auto& p : players_) {
    if (atk_win) {
      if (p.team == Team::Attacker) {
        p.money += cfg_.win_reward;
      } else {
        p.money += cfg_.loss_reward;
      }
    } else if (o != RoundOutcome::None) {
      if (p.team == Team::Defender) {
        p.money += cfg_.win_reward;
      } else {
        p.money += cfg_.loss_reward;
      }
    }
  }
}

void Scenario::finish_(RoundOutcome o) {
  phase_ = RoundPhase::RoundEnd;
  outcome_ = o;
  bomb_.plant_in_progress = false;
  bomb_.defuse_in_progress = false;
  apply_round_money_(o);
  if (o == RoundOutcome::AttackersWinExplode ||
      o == RoundOutcome::AttackersWinEliminate) {
    ++score_.attacker_wins;
  } else if (o != RoundOutcome::None) {
    ++score_.defender_wins;
  }
}

}  // namespace fps
