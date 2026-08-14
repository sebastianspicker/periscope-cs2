#include "red_example.hpp"

namespace examples::dynamic_ssn_resolve {

RedResult apply(sim::World& w) {
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  if (game == 0 || target == nullptr) {
    return {false, 0, 0, "precondition failed: game process missing"};
  }

  int steps = 1;
  constexpr int kResolvedSsnCount = 2;
  const auto resolver = w.spawn("ssn-resolver.exe");
  if (resolver == 0 || w.proc(resolver) == nullptr) {
    return {false, steps, 0, "SSN resolver process creation failed"};
  }
  ++steps;

  // The lab models export/stub parsing from the target build's ntdll.dll on disk.
  w.dynamic_ssn_resolved = true;
  w.resolved_ssn_windows_build = 22631;
  ++steps;

  if (!w.open_process(resolver, game, sim::AccessMask::VmRead, true)) {
    return {false, steps, kResolvedSsnCount, "resolved syscall VM_READ handle open failed"};
  }
  ++steps;

  if (w.read_mem(resolver, game, target->base, 4, true).status != ac::Status::Ok) {
    return {false, steps, kResolvedSsnCount, "entity data read failed"};
  }
  ++steps;

  bool syscall_handle = false;
  for (const auto& handle : w.handles_to(game, true)) {
    if (handle.owner_pid == resolver &&
        sim::has(handle.access, sim::AccessMask::VmRead) &&
        handle.via_syscall_path) {
      syscall_handle = true;
      break;
    }
  }

  const bool achieved = w.dynamic_ssn_resolved && syscall_handle;
  const std::string detail =
      "resolved NtOpenProcess and NtReadVirtualMemory SSNs from ntdll build 22631; "
      "hardcoded tables break on new Windows builds";
  w.note(detail);
  return {achieved, steps, kResolvedSsnCount, detail};
}

}  // namespace examples::dynamic_ssn_resolve
