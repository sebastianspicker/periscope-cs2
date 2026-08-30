#include "red_example.hpp"

namespace examples::enhanced_stack_spoof {

RedResult apply(sim::World& w) {
  const auto game = w.game_pid();
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, 0, "precondition failed: game process missing"};
  }

  int steps = 1;
  const auto reader = w.spawn("legit-reader.exe");
  if (reader == 0 || w.proc(reader) == nullptr) {
    return {false, steps, "reader process creation failed"};
  }
  ++steps;

  if (!w.open_process(reader, game, sim::AccessMask::VmRead, true)) {
    return {false, steps, "syscall VM_READ handle open failed"};
  }
  ++steps;

  // This simulation records a four-frame, trusted-module-looking return chain.
  w.enhanced_stack_spoof = true;
  w.spoofed_call_depth = 4;
  w.stack_spoof_on_read = true;
  ++steps;

  const auto* target = w.proc(game);
  if (target == nullptr ||
      w.read_mem(reader, game, target->base, 4, true).status != ac::Status::Ok) {
    return {false, steps, "entity data read failed"};
  }
  ++steps;

  const bool handle_open = !w.handles_to(game, true).empty();
  const bool achieved = handle_open && w.spoofed_call_depth >= 2;
  const std::string detail =
      "enhanced stack spoof forged 4 return frames: ntdll -> kernel32 -> game.dll -> reader";
  w.note(detail);
  return {achieved, steps, detail};
}

}  // namespace examples::enhanced_stack_spoof
