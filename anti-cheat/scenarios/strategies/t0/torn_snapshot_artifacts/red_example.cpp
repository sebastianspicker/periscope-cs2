#include "red_example.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace examples::torn_snapshot_artifacts {

namespace {
// Lab entity row layout (matches the planted table): x,y,z floats, then
// team byte, alive byte, 2 pad bytes.
constexpr std::size_t kRowSize = 16;
constexpr std::size_t kRowsOffset = 0x10;

std::vector<std::uint8_t> row_bytes(float x, float y, float z, std::uint8_t team,
                                    std::uint8_t alive) {
  std::vector<std::uint8_t> bytes(kRowSize, 0);
  std::memcpy(bytes.data() + 0, &x, 4);
  std::memcpy(bytes.data() + 4, &y, 4);
  std::memcpy(bytes.data() + 8, &z, 4);
  bytes[12] = team;
  bytes[13] = alive;
  return bytes;
}

void game_updates_row(sim::World& w, std::uint32_t game_pid, std::size_t row_index,
                      const std::vector<std::uint8_t>& bytes) {
  // The lab stands in for the game's own state advance between red's
  // transactions (the game writes its own memory, not red).
  const auto addr = static_cast<std::uint64_t>(w.lab_entity_table_rel) +
                    kRowsOffset + row_index * kRowSize;
  w.write_mem(game_pid, w.proc(game_pid)->base + addr, bytes.data(), bytes.size());
}
}  // namespace

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game || game->memory.empty()) {
    std::printf("[T0 torn_snapshot_artifacts] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 torn_snapshot_artifacts] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("torn-read-actor.exe");
  if (!w.proc(actor) ||
      !w.open_process(actor, game_pid, sim::AccessMask::VmRead)) {
    std::printf("[T0 torn_snapshot_artifacts] FAIL: actor or VM_READ handle failed\n");
    return {false, steps, "actor spawn / VM_READ handle failed"};
  }
  std::printf("[T0 torn_snapshot_artifacts] step %d: actor pid=%u with VM_READ handle\n",
              ++steps, actor);

  // Control: one single-transaction read decodes a consistent snapshot.
  auto control = w.read_entity_snapshots(game_pid);
  if (control.empty()) {
    return {false, steps, "control single-transaction read failed"};
  }
  w.single_transaction_read_done = true;
  std::printf("[T0 torn_snapshot_artifacts] step %d: control snapshot consistent (%zu records)\n",
              ++steps, control.size());

  // Torn pass: read fields in separate transactions while the game advances.
  const auto row0_addr = game->base + w.lab_entity_table_rel + kRowsOffset;
  const auto row1_addr = row0_addr + kRowSize;

  // Record 1: alive/team from generation 0, ...
  auto alive_team = w.read_mem(actor, game_pid, row0_addr + 12, 2, true);
  if (alive_team.status != ac::Status::Ok || alive_team.bytes.size() != 2) {
    return {false, steps, "field read (alive/team) failed"};
  }
  ac::EntitySnapshot record1;
  record1.health = 100;  // health read in the same early transaction
  record1.alive = alive_team.bytes[1] != 0;
  ++w.torn_read_transactions;

  // ... the game updates row 0 (origin zeroed, team 1->2, alive 0), ...
  game_updates_row(w, game_pid, 0, row_bytes(0.f, 0.f, 0.f, 2, 0));

  // ... then origin/team are read from the new generation.
  auto origin = w.read_mem(actor, game_pid, row0_addr, 12, true);
  if (origin.status != ac::Status::Ok || origin.bytes.size() != 12) {
    return {false, steps, "field read (origin) failed"};
  }
  std::memcpy(&record1.origin.x, origin.bytes.data() + 0, 4);
  std::memcpy(&record1.origin.y, origin.bytes.data() + 4, 4);
  std::memcpy(&record1.origin.z, origin.bytes.data() + 8, 4);
  auto team1 = w.read_mem(actor, game_pid, row0_addr + 12, 1, true);
  record1.team = team1.status == ac::Status::Ok && !team1.bytes.empty() ? team1.bytes[0] : 0;
  w.torn_read_transactions += 2;
  record1.id = 1;
  // Torn: alive from the old generation, origin/team from the new one -
  // an "alive" entity standing at the zero origin.
  w.torn_read_records.push_back(record1);
  std::printf("[T0 torn_snapshot_artifacts] step %d: torn record 1 assembled (alive=%d at zero origin)\n",
              ++steps, record1.alive ? 1 : 0);

  // Record 2: the row is mid-teardown (the game clears the team byte first),
  // so the team read lands on the transient zero while the count still lists
  // the entity; the alive byte is read after the teardown completes.
  game_updates_row(w, game_pid, 0, row_bytes(0.f, 0.f, 0.f, 0, 0));
  auto team_mid = w.read_mem(actor, game_pid, row0_addr + 12, 1, true);
  ac::EntitySnapshot record2;
  record2.id = 2;
  record2.team = team_mid.status == ac::Status::Ok && !team_mid.bytes.empty()
                     ? team_mid.bytes[0]
                     : 0;
  auto alive2 = w.read_mem(actor, game_pid, row0_addr + 13, 1, true);
  record2.alive = alive2.status == ac::Status::Ok && !alive2.bytes.empty()
                      ? alive2.bytes[0] != 0
                      : false;
  // Cross-contamination: pair row 0's torn fields with row 1's origin.
  auto origin1 = w.read_mem(actor, game_pid, row1_addr, 12, true);
  if (origin1.status == ac::Status::Ok && origin1.bytes.size() == 12) {
    std::memcpy(&record2.origin.x, origin1.bytes.data() + 0, 4);
    std::memcpy(&record2.origin.y, origin1.bytes.data() + 4, 4);
    std::memcpy(&record2.origin.z, origin1.bytes.data() + 8, 4);
  }
  w.torn_read_transactions += 3;
  w.torn_read_records.push_back(record2);
  std::printf("[T0 torn_snapshot_artifacts] step %d: torn record 2 assembled (invalid team=%u)\n",
              ++steps, record2.team);

  w.torn_multi_read_active = true;
  if (w.torn_read_records.size() < 2 || w.torn_read_transactions < 5) {
    return {false, steps, "torn artifact production failed"};
  }
  std::printf("[T0 torn_snapshot_artifacts] step %d: torn artifacts retained (%zu records, %d transactions)\n",
              ++steps, w.torn_read_records.size(), w.torn_read_transactions);

  const std::string detail =
      "torn_snapshot_artifacts: transactions=" + std::to_string(w.torn_read_transactions) +
      " records=" + std::to_string(w.torn_read_records.size()) +
      " control=single_transaction_ok";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::torn_snapshot_artifacts
