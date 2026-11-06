#include "red_example.hpp"

#include <cstdio>

namespace examples::fp_allowlist_evasion {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 fp_allowlist_evasion] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 fp_allowlist_evasion] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("fp_allowlist_evasion-actor.exe");
  auto* actor_process = w.proc(actor);
  if (!actor_process) {
    std::printf("[T0 fp_allowlist_evasion] FAIL: actor creation was not observable\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T0 fp_allowlist_evasion] step %d: actor pid=%u created\n", ++steps, actor);

  if (!w.open_process(actor, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 fp_allowlist_evasion] FAIL: VM_READ handle denied\n");
    return {false, steps, "OpenProcess VM_READ failed"};
  }
  bool has_handle = false;
  bool has_brief_handle = false;
  for (const auto& handle : w.handles_to(game_pid)) {
    if (handle.owner_pid == actor && sim::has(handle.access, sim::AccessMask::VmRead)) {
      has_handle = true;
      has_brief_handle = handle.brief_reopen;
    }
  }
  if (!has_handle) {
    return {false, steps, "VM_READ handle verification failed"};
  }
  std::printf("[T0 fp_allowlist_evasion] step %d: VM_READ edge verified\n", ++steps);

  auto read = w.read_mem(actor, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 fp_allowlist_evasion] FAIL: protected read failed\n");
    return {false, steps, "VM_READ verification read failed"};
  }
  std::printf("[T0 fp_allowlist_evasion] step %d: read telemetry recorded\n", ++steps);

  actor_process->looks_reputable = true;
  actor_process->reader_active = true;
  const bool scar_verified = actor_process->looks_reputable;
  if (!scar_verified) {
    std::printf("[T0 fp_allowlist_evasion] FAIL: reputable-looking reader co-residence was not retained\n");
    return {false, steps, "scar verification failed: reputable-looking reader co-residence"};
  }
  std::printf("[T0 fp_allowlist_evasion] step %d: reputable-looking reader co-residence planted and verified\n", ++steps);
  const std::string detail = "fp_allowlist_evasion: VM_READ plus reputable-looking reader co-residence";
  w.note(detail);
  return {true, steps, detail, false, true};
}

RedResult apply_true_radar(sim::World& w) { return apply(w); }

RedResult apply_legit_only(sim::World& w) {
  const auto legit = w.spawn("nvidia-overlay");
  auto* process = w.proc(legit);
  if (!process) return {false, 0, "legitimate process spawn failed", false, false};
  process->looks_reputable = true;
  process->reader_active = false;
  const std::string detail = "fp_allowlist_evasion: reputable process without VM_READ";
  w.note(detail);
  return {true, 1, detail, true, false};
}

}  // namespace examples::fp_allowlist_evasion
