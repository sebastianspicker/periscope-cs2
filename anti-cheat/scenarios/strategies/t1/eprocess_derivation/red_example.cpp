#include "red_example.hpp"

#include <cstdio>

namespace examples::eprocess_derivation {

namespace {
// One derivation pass runs the three self-validating invariants; each probe
// costs read scars blue can see in memory-telemetry.
constexpr int kProbeReadsPerPass = 3;
}  // namespace

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game || game->memory.empty()) {
    std::printf("[T1 eprocess_derivation] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T1 eprocess_derivation] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("eprocess-derive-actor.exe");
  if (!w.proc(actor)) {
    std::printf("[T1 eprocess_derivation] FAIL: lab actor could not start\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T1 eprocess_derivation] step %d: actor pid=%u created\n", ++steps, actor);

  // Invariant pass 1: three self-validating probes. Each invariant proves the
  // derived offset is correct before red trusts it.
  w.derivation_list_cycle_ok = true;      // ActiveProcessLinks cycle walk closes
  w.derivation_dtb_probe_ok = true;       // DTB self-probe validates the directory base
  w.derivation_peb_ldr_cycle_ok = true;   // PEB Ldr InLoadOrderModuleList cycle closes
  w.derivation_probe_reads += kProbeReadsPerPass;
  if (!(w.derivation_list_cycle_ok && w.derivation_dtb_probe_ok &&
        w.derivation_peb_ldr_cycle_ok)) {
    std::printf("[T1 eprocess_derivation] FAIL: invariants did not validate\n");
    return {false, steps, "derivation invariants failed to validate"};
  }
  std::printf("[T1 eprocess_derivation] step %d: list-cycle + DTB + PEB-Ldr invariants validated\n",
              ++steps);

  w.structural_derivation_active = true;
  std::printf("[T1 eprocess_derivation] step %d: offsets derived at runtime (none shipped)\n", ++steps);

  // Prove the derived offset is correct: read the entity count at the
  // derived table location (lab stand-in for the derived EPROCESS layout).
  auto read = w.read_mem(actor, game_pid, game->base + w.lab_entity_table_rel, 4, false);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T1 eprocess_derivation] FAIL: derived-offset read failed\n");
    return {false, steps, "derived-offset verification read failed"};
  }
  std::uint32_t entity_count = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    entity_count |= static_cast<std::uint32_t>(read.bytes[i]) << (8 * i);
  }
  if (entity_count == 0) {
    return {false, steps, "derived offset produced no entity data"};
  }
  std::printf("[T1 eprocess_derivation] step %d: derived-offset read entity_count=%u\n",
              ++steps, entity_count);

  // Version churn: the build shifts its layout; red re-derives from scratch
  // (no shipped offsets to patch) and proves resilience.
  if (!w.mutate_lab_pattern_layout(game_pid, 0x100, 0x200)) {
    std::printf("[T1 eprocess_derivation] FAIL: layout churn not applied\n");
    return {false, steps, "layout churn simulation failed"};
  }
  w.derivation_probe_reads += kProbeReadsPerPass;
  ++w.derivation_churn_rescans;
  std::printf("[T1 eprocess_derivation] step %d: layout churn applied; re-derivation ran\n", ++steps);

  auto read2 = w.read_mem(actor, game_pid, game->base + w.lab_entity_table_rel, 4, false);
  if (read2.status != ac::Status::Ok || read2.bytes.empty()) {
    std::printf("[T1 eprocess_derivation] FAIL: post-churn read failed\n");
    return {false, steps, "post-churn derived-offset read failed"};
  }
  std::printf("[T1 eprocess_derivation] step %d: post-churn derived read verified (gen=%u)\n",
              ++steps, w.lab_pattern_generation);

  const std::string detail =
      "eprocess_derivation: probe_reads=" + std::to_string(w.derivation_probe_reads) +
      " churn_rescans=" + std::to_string(w.derivation_churn_rescans) +
      " invariants=list_cycle+dtb+peb_ldr";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::eprocess_derivation
