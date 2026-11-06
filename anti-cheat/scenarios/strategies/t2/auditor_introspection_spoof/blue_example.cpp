#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::auditor_introspection_spoof {

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

  // Reason 1 (CRC-of-CRC): the auditor re-hashes its own CRC report. A
  // rewritten report leaves two entries for the same region or a report
  // count that disagrees with the scan count - the report fails
  // self-verification before any byte is trusted.
  if (w.auditor_crc_report_spoofed && w.auditor_crc_rewrites > 0) {
    w.auditor_crc_of_crc_checked = true;
    sig(true,
        "CRC-of-CRC mismatch: module-scan report was rewritten " +
            std::to_string(w.auditor_crc_rewrites) + " time(s)");
  }
  // Reason 2 (second-path prologue read): read the prologue through a
  // different access path than the string-copy helper red feeds.
  if (w.prologue_string_copy_feedback || w.diagnostic_dump_nulled_overlap) {
    w.auditor_second_path_prologue_read = true;
    sig(true, "second-path prologue read disagrees with the string-copy feedback");
  }
  // Reason 3 (diagnostic cross-check): dumps that overlap a live module must
  // not be uniformly nulled; compare dump bytes against the module map.
  sig(w.diagnostic_dump_nulled_overlap,
                      "diagnostic dumps overlapping a mapped region were nulled (nulls=" +
                          std::to_string(w.diagnostic_dump_nulls) + ")");
  // Reason 4 (execution-artifact corroboration from the shared scanner).
  if (game->manual_mapped_region || game->has_foreign_thread) {
    sig(true, "game process carries an execution artifact under a clean report");
  }

  const bool specific_scar = w.auditor_crc_report_spoofed &&
                             (w.diagnostic_dump_nulled_overlap ||
                              w.prologue_string_copy_feedback);
  if (specific_scar) {
    result.reasons.emplace_back(
        "strategy scar: auditor introspection spoof (CRC report + dump/prologue tamper)");
  }

  // Mitigation: run both verification paths persistently and record
  // evidence; a CRC report that fails CRC-of-CRC is treated as spoofed.
  if (result.detected || specific_scar) {
    w.auditor_crc_of_crc_checked = true;
    w.auditor_second_path_prologue_read = true;
    ++w.evidence_count;
    result.reasons.emplace_back("blue persists CRC-of-CRC + second-path reads (evidence=" +
                                std::to_string(w.evidence_count) + ")");
  }

  result.risk = support::risk_for(result.signals);
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && w.auditor_crc_of_crc_checked &&
                     w.auditor_second_path_prologue_read && result.risk >= 0.70;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "auditor_introspection_spoof blue signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  std::printf("[T2 auditor_introspection_spoof] BLUE signals=%d detected=%s mitigated=%s\n",
              result.signals, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T2 auditor_introspection_spoof]   %s\n", reason.c_str());
  }
  w.note(result.detail);
  return result;
}

}  // namespace examples::auditor_introspection_spoof
