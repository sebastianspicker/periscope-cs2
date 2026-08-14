#include "red_example.hpp"

#include <cstdio>

ac::Status vac_handle_red_apply(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "step 1: validate game process for VAC handle demo");
  const auto game = w.game_pid();
  if (game == 0 || w.proc(game) == nullptr) {
    n.say(sim::Side::Red, "Game process missing.");
    return ac::Status::Unavailable;
  }

  n.say(sim::Side::Red, "step 2: spawn non-Steam external radar process");
  const auto reader_pid = w.spawn("external_radar.exe");
  if (reader_pid == 0 || w.proc(reader_pid) == nullptr) {
    n.say(sim::Side::Red, "Reader spawn failed.");
    return ac::Status::Unavailable;
  }

  n.say(sim::Side::Red, "step 3: open foreign VM_READ handle to the game");
  if (!w.open_process(reader_pid, game, sim::AccessMask::VmRead, false)) {
    n.say(sim::Side::Red, "Handle open failed in the simulated process table.");
    return ac::Status::Denied;
  }

  n.say(sim::Side::Red, "step 4: live entity peek so the handle is active");
  const auto* g = w.proc(game);
  if (g == nullptr ||
      w.read_mem(reader_pid, game, g->base, 4, true).status != ac::Status::Ok) {
    n.say(sim::Side::Red, "Entity peek failed.");
    return ac::Status::Denied;
  }

  n.say(sim::Side::Red, "step 5: verify handle edge is visible to system-handle enumeration");
  bool visible = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (h.owner_pid == reader_pid && sim::has(h.access, sim::AccessMask::VmRead)) {
      visible = true;
      break;
    }
  }
  if (!visible || w.remote_read_ops == 0) {
    n.say(sim::Side::Red, "Post-condition failed: handle/read residual missing.");
    return ac::Status::Denied;
  }

  n.say(sim::Side::Red, "The non-Steam VM_READ edge is now visible to handle enumeration.");
  std::printf("[red:vac_handle_enum] achieved: foreign VM_READ owner=%u reads=%llu\n",
              reader_pid, static_cast<unsigned long long>(w.remote_read_ops));
  w.note("vac_handle_enum: non-Steam foreign VM_READ + live remote read");
  return ac::Status::Ok;
}
