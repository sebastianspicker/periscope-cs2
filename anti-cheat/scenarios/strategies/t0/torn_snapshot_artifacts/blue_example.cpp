#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::torn_snapshot_artifacts {

namespace {
// One plausibility invariant over an assembled entity record. A record that
// fails any invariant could not have come from one consistent game tick.
int implausible_reasons(const ac::EntitySnapshot& e, std::vector<std::string>* why) {
  int bad = 0;
  const bool team_valid = e.team == 1 || e.team == 2;  // T / CT
  if (!team_valid) {
    ++bad;
    if (why) why->emplace_back("record " + std::to_string(e.id) + ": invalid team value");
  }
  if (e.alive && e.origin.x == 0.f && e.origin.y == 0.f && e.origin.z == 0.f) {
    ++bad;
    if (why) why->emplace_back("record " + std::to_string(e.id) +
                               ": alive entity at the zero origin");
  }
  if (e.health > 0 && !e.alive) {
    ++bad;
    if (why) why->emplace_back("record " + std::to_string(e.id) +
                               ": positive health with dead lifestate");
  }
  return bad;
}
}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult result;
  // Signal shim: the shared add_signal takes const char*; wrap for std::string reasons.
  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present, reason.c_str());
  };
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    return result;
  }

  // Reason 1: validate every record red assembled against the invariants.
  int torn_records = 0;
  for (const auto& record : w.torn_read_records) {
    const int bad = implausible_reasons(record, &result.reasons);
    if (bad > 0) ++torn_records;
  }
  sig(torn_records > 0,
                      "implausible field combinations in " +
                          std::to_string(torn_records) + " record(s)");
  // Reason 2: the transaction pattern - many tiny single-field transactions
  // per record is the torn-read signature.
  sig(
                      w.torn_read_transactions >= 2 * static_cast<int>(w.torn_read_records.size()),
                      "multi-transaction read pattern transactions=" +
                          std::to_string(w.torn_read_transactions));
  // Reason 3 (strategy scar): red ran the torn multi-read pass at all.
  sig(w.torn_multi_read_active,
                      "torn multi-read pass is active");
  // Reason 4: remote-read telemetry from the field reads.
  sig(w.remote_read_ops > 0,
                      "remote read telemetry ops=" + std::to_string(w.remote_read_ops));

  const bool specific_scar = torn_records > 0;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: torn records from multi-transaction reads");
  }

  // Mitigation posture: require single-transaction snapshot reads with
  // sequence validation, so red's records must either be consistent or
  // visibly stale (and stale is detectable via generation counters).
  if (result.detected || specific_scar) {
    result.reasons.emplace_back(
        "blue requires single-transaction snapshot reads with sequence validation");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "torn_snapshot_artifacts blue signals=" +
                  std::to_string(result.signals) + " torn_records=" +
                  std::to_string(torn_records) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T0 torn_snapshot_artifacts] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T0 torn_snapshot_artifacts]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::torn_snapshot_artifacts
