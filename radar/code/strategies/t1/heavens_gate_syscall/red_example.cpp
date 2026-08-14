#include "red_example.hpp"

namespace examples::heavens_gate_syscall {

RedResult apply(sim::World& w) {
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  if (game == 0 || target == nullptr) {
    return {false, 0, "precondition failed: game process missing"};
  }

  int steps = 1;
  const auto stub = w.spawn("wow64-stub.exe");
  if (stub == 0 || w.proc(stub) == nullptr) {
    return {false, steps, "WOW64 stub process creation failed"};
  }
  ++steps;

  // The 32-bit stub uses the simulated Heaven's Gate transition before opening.
  w.heavens_gate_transition = true;
  w.heavens_gate_stub_pid = stub;
  ++steps;

  if (!w.open_process(stub, game, sim::AccessMask::VmRead, false)) {
    return {false, steps, "Heaven's Gate VM_READ handle open failed"};
  }
  ++steps;

  if (w.read_mem(stub, game, target->base, 4, true).status != ac::Status::Ok) {
    return {false, steps, "entity data read failed"};
  }
  ++steps;

  bool gate_handle = false;
  for (const auto& handle : w.handles_to(game, true)) {
    if (handle.owner_pid == stub && sim::has(handle.access, sim::AccessMask::VmRead) &&
        !handle.via_syscall_path && !handle.via_proxy) {
      gate_handle = true;
      break;
    }
  }

  const bool achieved = gate_handle && w.heavens_gate_transition &&
                        w.heavens_gate_stub_pid == stub;
  const std::string detail =
      "WOW64 stub used Heaven's Gate to open a raw 64-bit VM_READ handle";
  w.note(detail);
  return {achieved, steps, detail};
}

}  // namespace examples::heavens_gate_syscall
