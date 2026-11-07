// radar_snapshot_test.cpp: R1: protocol v1 serializer checks.
//
// Covers the properties the research ledger demands: stable field order,
// u64 ids serialized as strings, Steam IDs off by default, non-finite float
// normalization, string escaping, and team-view filtering.

#include "radar/snapshot.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {

using radar_domain::BombState;
using radar_domain::EntityEvent;
using radar_domain::EntityKind;
using radar_domain::GameSnapshot;
using radar_domain::PlayerState;
using radar_domain::SerializationOptions;
using radar_domain::TeamViewPolicy;
using radar_domain::WorldPosition;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

GameSnapshot sample_snapshot() {
  GameSnapshot snap;
  snap.seq = 1042;
  snap.t_capture_ms = 1762345678901ULL;
  snap.map = "de_mirage";
  snap.local.player_id = 76561198000000001ULL;
  snap.local.team = radar_domain::kTeamCT;
  snap.local.name = "local";
  snap.local.alive = true;
  snap.local.pos = {100.0f, 200.0f, 64.0f};
  snap.local.yaw = 91.4f;
  snap.local.health = 87;
  snap.local.armor = 100;

  PlayerState enemy;
  enemy.player_id = 76561198000000002ULL;
  enemy.steam_id = 11111111111111111ULL;
  enemy.team = radar_domain::kTeamT;
  enemy.name = "enemy \"quoted\"\n";
  enemy.alive = true;
  enemy.pos = {1213.5f, 812.25f, 64.0f};
  enemy.yaw = 12.0f;
  enemy.health = 100;
  enemy.armor = 50;
  enemy.weapon_id = 7;
  enemy.ammo_clip = 30;
  enemy.ammo_reserve = 90;

  PlayerState teammate;
  teammate.player_id = 76561198000000003ULL;
  teammate.team = radar_domain::kTeamCT;
  teammate.name = "mate";

  snap.players = {enemy, teammate};

  EntityEvent inferno;
  inferno.kind = EntityKind::Inferno;
  inferno.pos = {1.0f, 2.0f, 3.0f};
  inferno.life_remaining_s = 6.5f;
  inferno.hull = {{0, 0, 0}, {10, 0, 0}};
  snap.entities = {inferno};

  snap.bomb.planted = true;
  snap.bomb.ticking = true;
  snap.bomb.pos = {500.0f, 600.0f, 0.0f};
  snap.bomb.timer_length = 40.0f;
  return snap;
}

void test_stable_layout_and_string_ids() {
  const std::string json = radar_domain::serialize_snapshot_v1(sample_snapshot());
  check(contains(json, "\"v\":1,\"seq\":1042,\"t_capture_ms\":1762345678901"),
        "stable header order");
  check(contains(json, "\"map\":\"de_mirage\""), "map field");
  check(contains(json, "\"id\":\"76561198000000001\""),
        "u64 id serialized as string");
  check(contains(json, "\"ammo\":[30,90]"), "ammo pair");
  check(contains(json, "\"kind\":\"inferno\""), "typed entity");
  check(contains(json, "\"hull\":[[0,0,0],[10,0,0]]"), "inferno hull");
  check(contains(json, "\"planted\":true,\"ticking\":true"), "bomb block");
  check(json.back() == '}', "document closed");
}

void test_steam_ids_default_off_opt_in_on() {
  GameSnapshot snap = sample_snapshot();
  const std::string default_json = radar_domain::serialize_snapshot_v1(snap);
  check(!contains(default_json, "steam_id"), "steam id omitted by default");

  SerializationOptions opt_in;
  opt_in.include_steam_ids = true;
  const std::string with_ids = radar_domain::serialize_snapshot_v1(snap, opt_in);
  check(contains(with_ids, "\"steam_id\":\"11111111111111111\""),
        "steam id present when opted in");
}

void test_name_hiding() {
  GameSnapshot snap = sample_snapshot();
  SerializationOptions options;
  options.include_player_names = false;
  const std::string json = radar_domain::serialize_snapshot_v1(snap, options);
  check(contains(json, "\"name\":\"\""), "names blanked");
  check(!contains(json, "quoted"), "no name content leaks");
}

void test_escaping() {
  GameSnapshot snap = sample_snapshot();
  snap.players[0].name = "a\"b\\c\n\t\x01";
  const std::string json = radar_domain::serialize_snapshot_v1(snap);
  check(contains(json, "a\\\"b\\\\c\\n\\t\\u0001"), "escape sequence");
}

void test_non_finite_normalized() {
  GameSnapshot snap = sample_snapshot();
  const float quiet_nan = std::numeric_limits<float> ::quiet_NaN();
  const float inf = std::numeric_limits<float> ::infinity();
  snap.players[0].pos = {quiet_nan, inf, -inf};
  snap.players[0].yaw = quiet_nan;
  const std::string json = radar_domain::serialize_snapshot_v1(snap);
  check(!contains(json, "NaN") && !contains(json, "Infinity"),
        "no NaN/Infinity tokens");
  check(contains(json, "\"pos\":[0,0,0]"), "non-finite position zeroed");
  // The enemy (first player row) had its yaw poisoned; the local player's
  // 91.4 must not match, so this asserts the normalized zero specifically.
  check(contains(json, "\"yaw\":0,"), "non-finite yaw zeroed");
}

void test_team_view_policies() {
  GameSnapshot snap = sample_snapshot();
  const std::string all = radar_domain::serialize_snapshot_v1(snap);
  check(contains(all, "76561198000000002") &&
            contains(all, "76561198000000003"),
        "all view keeps both");

  SerializationOptions local_team;
  local_team.team_view = TeamViewPolicy::LocalTeamOnly;
  const std::string mates = radar_domain::serialize_snapshot_v1(snap, local_team);
  check(!contains(mates, "76561198000000002"), "local-team view drops enemy");
  check(contains(mates, "76561198000000003"), "local-team view keeps mate");

  SerializationOptions opponents;
  opponents.team_view = TeamViewPolicy::OpponentsOnly;
  const std::string foes = radar_domain::serialize_snapshot_v1(snap, opponents);
  check(contains(foes, "76561198000000002"), "opponents view keeps enemy");
  check(!contains(foes, "76561198000000003"), "opponents view drops mate");
}

void test_empty_snapshot_is_valid_json() {
  GameSnapshot snap;
  const std::string json = radar_domain::serialize_snapshot_v1(snap);
  check(contains(json, "\"players\":[]"), "empty players array");
  check(contains(json, "\"entities\":[]"), "empty entities array");
}

}  // namespace

int main() {
  test_stable_layout_and_string_ids();
  test_steam_ids_default_off_opt_in_on();
  test_name_hiding();
  test_escaping();
  test_non_finite_normalized();
  test_team_view_policies();
  test_empty_snapshot_is_valid_json();

  if (failures != 0) {
    std::printf("%d snapshot check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_snapshot_test: all checks passed\n");
  return 0;
}
