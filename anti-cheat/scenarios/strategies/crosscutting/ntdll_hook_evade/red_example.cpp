#include "red_example.hpp"

namespace examples::ntdll_hook_evade {

RedResult apply(sim::World& w) {
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  if (game == 0 || target == nullptr) {
    return {false, 0, 0, false, "precondition failed: game process missing"};
  }

  int steps = 1;
  constexpr int kHooksFound = 3;
  const auto unhooker = w.spawn("unhooker.exe");
  if (unhooker == 0 || w.proc(unhooker) == nullptr) {
    return {false, steps, 0, false, "unhooker process creation failed"};
  }
  ++steps;

  // The lab models comparing in-memory ntdll.text with the clean disk image.
  w.ntdll_hooks_detected = true;
  w.hooks_evaded_count = kHooksFound;
  ++steps;

  w.used_clean_ntdll_copy = true;
  ++steps;

  if (!w.open_process(unhooker, game, sim::AccessMask::VmRead, true)) {
    return {false, steps, kHooksFound, true, "syscall VM_READ handle open failed"};
  }
  ++steps;

  if (w.read_mem(unhooker, game, target->base, 4, true).status != ac::Status::Ok) {
    return {false, steps, kHooksFound, true, "entity data read failed"};
  }
  ++steps;

  bool syscall_handle = false;
  for (const auto& handle : w.handles_to(game, true)) {
    if (handle.owner_pid == unhooker &&
        sim::has(handle.access, sim::AccessMask::VmRead) &&
        handle.via_syscall_path) {
      syscall_handle = true;
      break;
    }
  }

  const bool achieved = w.ntdll_hooks_detected && syscall_handle;
  const std::string detail =
      "found 3 ntdll hooks, mapped a clean disk copy, and read entity data via syscall";
  w.note(detail);
  return {achieved, steps, kHooksFound, w.used_clean_ntdll_copy, detail};
}

}  // namespace examples::ntdll_hook_evade
