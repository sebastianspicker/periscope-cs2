#include "red_example.hpp"

#include <cstdio>

namespace examples::auditor_introspection_spoof {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T2 auditor_introspection_spoof] FAIL: game process unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  // The auditor (CS2's diagnostic module scan) must be active for the
  // interplay to exist; pair.cpp seeds this posture like sibling pairs.
  if (!w.diagnostic_system_active || !w.diagnostic_state.is_active) {
    std::printf("[T2 auditor_introspection_spoof] FAIL: auditor not active\n");
    return {false, steps, "precondition failed: auditor introspection not active"};
  }
  std::printf("[T2 auditor_introspection_spoof] step %d: game + auditor validated\n", ++steps);

  // Step: land the cheat image inside the game as a manual-mapped region so
  // the auditor's scan has something overlapping to lie about.
  game->modules.push_back({"auditor_spoof.dat", game->base + 0x600000, 0x2000,
                           false, true, "clean", false, false, false});
  game->manual_mapped_region = true;
  if (game->modules.empty() || game->modules.back().text_hash != "clean") {
    return {false, steps, "cheat image module was not registered"};
  }
  std::printf("[T2 auditor_introspection_spoof] step %d: cheat image mapped inside game\n", ++steps);

  // Step: rewrite the CRC the auditor's module-scan reports for the cheat
  // region - the scan runs, but the reported digest is pre-spoofed to the
  // pristine value the auditor expects.
  auto& report = w.diagnostic_state.loaded_modules.emplace_back();
  report.name = "auditor_spoof.dat";
  report.crc32 = 0xC0FFEE42u;  // lab 'expected clean' digest
  report.game_module = true;
  report.expected = true;
  w.auditor_crc_report_spoofed = true;
  w.auditor_crc_rewrites = 2;  // initial scan + one re-scan both rewritten
  if (!w.auditor_crc_report_spoofed || w.auditor_crc_rewrites == 0) {
    return {false, steps, "CRC report rewrite was not recorded"};
  }
  std::printf("[T2 auditor_introspection_spoof] step %d: module-scan CRC report rewritten (n=%d)\n",
              ++steps, w.auditor_crc_rewrites);

  // Step: null the diagnostic dumps that overlap the cheat image so the dump
  // path sees zeros (or pristine bytes) instead of patched code.
  w.diagnostic_dump_nulled_overlap = true;
  w.diagnostic_dump_nulls = 1;
  w.diag_memory_dump_performed = true;  // the dump still ran - it was sanitized
  std::printf("[T2 auditor_introspection_spoof] step %d: overlapping diagnostic dumps nulled\n", ++steps);

  // Step: when the auditor re-reads the prologue through its string-copy
  // helper, feed the saved original bytes back so the copy returns the
  // unpatched prologue.
  w.prologue_string_copy_feedback = true;
  std::printf("[T2 auditor_introspection_spoof] step %d: original prologue fed back via string copy\n", ++steps);

  // Scar verification: the game module still hashes clean from the auditor's
  // point of view while the manual-mapped region and rewrite scars persist.
  if (!(game->manual_mapped_region && w.auditor_crc_report_spoofed &&
        w.diagnostic_dump_nulled_overlap && w.prologue_string_copy_feedback &&
        game->modules.back().text_hash == "clean")) {
    std::printf("[T2 auditor_introspection_spoof] FAIL: spoof surface not retained\n");
    return {false, steps, "scar verification failed: spoof surface incomplete"};
  }
  std::printf("[T2 auditor_introspection_spoof] step %d: all three spoof scars verified\n", ++steps);

  const std::string detail =
      "auditor_introspection_spoof: crc_rewrites=" +
      std::to_string(w.auditor_crc_rewrites) + " dump_nulls=" +
      std::to_string(w.diagnostic_dump_nulls) + " prologue_feedback=1";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::auditor_introspection_spoof
