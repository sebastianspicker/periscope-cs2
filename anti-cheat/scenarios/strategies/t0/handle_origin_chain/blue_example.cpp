#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>
#include <set>

namespace examples::handle_origin_chain {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = 1;
    return result;
  }

  const auto sig = [&result](bool present, std::string reason) {
    strategies::add_signal(result.signals, result.reasons, present,
                           reason.c_str());
  };

  // Reason 1: the table contains duplicate-acquired handles at all.
  int duplicate_handles = 0;
  for (const auto& entry : w.handle_table.all_entries()) {
    if (entry.target_pid == game_pid &&
        entry.acquisition == ac::HandleAcquisitionModel::HandleDuplicate) {
      ++duplicate_handles;
    }
  }
  sig(duplicate_handles > 0,
      "duplicate-acquired VM_READ handles=" + std::to_string(duplicate_handles));

  // Reason 2: duplicate edges alone are not enough. Walk the whole handle set
  // for the target and count distinct owners: with a relay chain, at least
  // three processes (donor, relay, consumer) hold an edge, so the immediate
  // owner is not the origin. A single duplicate with no upstream stays at one.
  std::set<std::uint32_t> owners;
  for (const auto& entry : w.handle_table.open_handles_to(game_pid, true)) {
    if (entry.acquisition == ac::HandleAcquisitionModel::HandleDuplicate ||
        entry.acquisition == ac::HandleAcquisitionModel::DirectOpenProcess) {
      owners.insert(entry.owner_pid);
    }
  }
  const bool misattributed = owners.size() >= 3 && duplicate_handles >= 2;
  w.handle_origin_trace_hops = owners.empty()
                                   ? 0
                                   : static_cast<int>(owners.size()) - 1;
  w.handle_origin_misattributed = misattributed;
  sig(misattributed,
      "distinct handle owners=" + std::to_string(owners.size()) +
          ": origin is upstream of the immediate owner");

  // Reason 3: chain depth recorded by the duplicate graph.
  sig(w.handle_chain_depth >= 2,
      "duplicate chain depth=" + std::to_string(w.handle_chain_depth));

  // Reason 4: read telemetry still exists from the consumer.
  sig(w.remote_read_ops > 0,
      "remote read telemetry ops=" + std::to_string(w.remote_read_ops));

  const bool specific_scar = misattributed && duplicate_handles > 0;
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: handle origin hidden behind a relay hop");
  }

  // Mitigation: resolve the chain to the root and require the root process
  // (not the relay) to be signed/reputable before granting ranked access.
  if (result.detected || specific_scar) {
    ++w.handle_origin_trace_hops;
    result.reasons.emplace_back(
        "mitigation: chain resolved to root, root reputation required (hops=" +
        std::to_string(w.handle_origin_trace_hops) + ")");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && result.risk >= 0.65;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "handle_origin_chain blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf(
      "[T0 handle_origin_chain] BLUE signals=%d detected=%s mitigated=%s\n",
      result.signals, result.detected ? "true" : "false",
      result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T0 handle_origin_chain]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::handle_origin_chain
