#pragma once

// Counter-Strike-inspired bomb-defuse FPS scenario (headless, Simulated).
// Plant/defuse/drop/economy rules for the anti-cheat lab — not Valve netcode.

#include "fps/map.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fps {

enum class Team : std::uint8_t {
  Attacker = 0,  // plant side (T)
  Defender = 1,  // defuse side (CT)
};

enum class RoundPhase : std::uint8_t {
  Buy = 0,  // freeze / buy time
  Live,
  BombPlanted,
  RoundEnd,
};

enum class RoundOutcome : std::uint8_t {
  None = 0,
  AttackersWinExplode,
  AttackersWinEliminate,
  DefendersWinDefuse,
  DefendersWinEliminate,
  DefendersWinTime,  // round clock out with no plant
};

// Lab weapon class (minimal CS flavor for buy/damage).
enum class WeaponClass : std::uint8_t {
  None = 0,
  Pistol,
  Rifle,
  AWP,
};

// Lab type `Player` used by this educational unit.
struct Player {
  std::uint32_t id = 0;
  std::string name;
  Team team = Team::Attacker;
  ac::Vec3 origin{};
  bool alive = true;
  bool carries_bomb = false;
  float health = 100.f;
  int money = 800;                 // start money (CS-like freeze slice)
  bool has_defuse_kit = false;     // shortens defuse when true
  WeaponClass weapon = WeaponClass::Pistol;
};

// Bomb on ground when carrier dies before plant (CS-like).
struct GroundBomb {
  bool present = false;
  ac::Vec3 origin{};
};

// Lab type `BombState` used by this educational unit.
struct BombState {
  bool planted = false;
  bool defused = false;
  bool exploded = false;
  std::string site_name;  // "A" / "B" when planted
  ac::Vec3 origin{};
  std::uint32_t planter_id = 0;
  std::uint32_t defuser_id = 0;
  float fuse_remaining = 0.f;     // seconds until explode
  float plant_progress = 0.f;     // 0..plant_time
  float defuse_progress = 0.f;    // 0..effective defuse_time
  bool plant_in_progress = false;
  bool defuse_in_progress = false;
  GroundBomb ground{};            // dropped bomb before plant
};

// Lab type `RoundConfig` used by this educational unit.
struct RoundConfig {
  float freeze_time = 5.f;      // buy phase duration
  float round_time = 115.f;     // live seconds before time win (no plant)
  float plant_time = 3.f;       // hold-to-plant
  float defuse_time = 10.f;     // hold-to-defuse without kit (CS-ish)
  float defuse_time_kit = 5.f;  // with defuse kit
  float fuse_time = 40.f;       // C4 fuse after plant
  float max_plant_range = 4.f;  // must be within site / bomb radius
  float max_pickup_range = 4.f;
  int start_money = 800;
  int win_reward = 3250;
  int loss_reward = 1400;
  int plant_bonus = 300;  // attackers on successful plant (even if round continues)
  int kit_cost = 400;
  int rifle_cost = 2700;
  int pistol_cost = 0;
  float pistol_damage = 25.f;
  float rifle_damage = 35.f;
  float awp_damage = 100.f;
};

// Aggregate outcome fields for `ActionResult` (lab narrative / tests).
struct ActionResult {
  bool ok = false;
  const char* reason = "";
};

// Match score across rounds (optional multi-round).
struct MatchScore {
  int attacker_wins = 0;
  int defender_wins = 0;
};

/// Full match-round state machine (CS bomb-defuse inspired).
class Scenario {
 public:
  explicit Scenario(Map map = Map::make_dusty_yard(), RoundConfig cfg = {});

  const Map& map() const { return map_; }
  const RoundConfig& config() const { return cfg_; }
  RoundPhase phase() const { return phase_; }
  RoundOutcome outcome() const { return outcome_; }
  float clock() const { return clock_; }
  float freeze_clock() const { return freeze_clock_; }
  const BombState& bomb() const { return bomb_; }
  const std::vector<Player>& players() const { return players_; }
  const MatchScore& score() const { return score_; }
  Player* player(std::uint32_t id);
  const Player* player(std::uint32_t id) const;

  /// Spawn default 2v2 on dusty_yard; starts in Buy (freeze) then tick to Live.
  void start_default_round();

  /// Reset and place custom players. Starts in Buy freeze unless skip_freeze.
  void start_round(std::vector<Player> roster, bool skip_freeze = false);

  /// Skip remaining freeze and enter Live (tests/demos).
  ActionResult end_freeze();

  /// Move player if alive and not RoundEnd; cancels plant/defuse if out of range.
  ActionResult move_player(std::uint32_t id, const ac::Vec3& to);

  /// Buy defuse kit (defenders, Buy or Live, enough money).
  ActionResult buy_defuse_kit(std::uint32_t defender_id);
  /// Buy rifle (either team).
  ActionResult buy_rifle(std::uint32_t player_id);

  /// Deal damage with killer weapon class; may kill.
  ActionResult damage(std::uint32_t victim_id, std::uint32_t attacker_id,
                      float raw_damage = -1.f);

  /// Begin/continue plant: attacker + carries bomb + at site + alive + Live.
  ActionResult begin_plant(std::uint32_t attacker_id);
  ActionResult cancel_plant();

  /// Begin/continue defuse: defender + bomb planted + near bomb + alive.
  ActionResult begin_defuse(std::uint32_t defender_id);
  ActionResult cancel_defuse();

  /// Instant complete plant if rules allow (still enforces team/site/bomb).
  ActionResult plant_instant(std::uint32_t attacker_id, const std::string& site);

  /// Instant complete defuse if rules allow (kit only affects hold-time path).
  ActionResult defuse_instant(std::uint32_t defender_id);

  /// Eliminate a player; bomb carrier drops bomb on ground.
  ActionResult kill(std::uint32_t victim_id, std::uint32_t killer_id = 0);

  /// Attacker picks up ground bomb when in range.
  ActionResult pickup_bomb(std::uint32_t attacker_id);

  /// Advance simulation by dt: freeze→live, plant/defuse, fuse, clock, outcomes.
  void tick(float dt);

  int alive_count(Team t) const;
  std::uint32_t bomb_carrier_id() const;
  bool bomb_on_ground() const { return bomb_.ground.present; }

  /// Effective defuse hold time for a defender (kit shortens).
  float effective_defuse_time(std::uint32_t defender_id) const;

  std::string describe() const;

 private:
  void resolve_elimination_();
  void resolve_time_();
  void finish_(RoundOutcome o);
  void apply_round_money_(RoundOutcome o);
  float weapon_damage_(WeaponClass w) const;
  bool near_bomb_(const Player& p) const;

  Map map_;
  RoundConfig cfg_;
  RoundPhase phase_ = RoundPhase::Buy;
  RoundOutcome outcome_ = RoundOutcome::None;
  float clock_ = 0.f;
  float freeze_clock_ = 0.f;
  BombState bomb_{};
  std::vector<Player> players_;
  std::uint32_t next_id_ = 1;
  MatchScore score_{};
  bool plant_bonus_paid_ = false;
};

const char* to_string(Team t);
const char* to_string(RoundPhase p);
const char* to_string(RoundOutcome o);
const char* to_string(WeaponClass w);

}  // namespace fps
