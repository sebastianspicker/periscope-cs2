#include "red_example.hpp"
#include <cstdio>

namespace examples::shellcode_inject_donor {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto donor = w.spawn("donor-shellcode.exe");
  auto* donor_proc = w.proc(donor);
  if (!donor_proc)
    return {false, steps, "donor spawn failed"};

  if (!w.open_process(donor, game_pid, sim::AccessMask::VmRead, false))
    return {false, steps, "OpenProcess failed"};

  donor_proc->has_foreign_thread = true;
  donor_proc->manual_mapped_region = true;

  w.shellcode_donor_active = true;
  w.shellcode_donor_pid = donor;
  w.shellcode_obfuscated = true;
  w.shellcode_xor_key_applied = true;

  const bool scar = w.shellcode_donor_active && w.shellcode_obfuscated && w.shellcode_xor_key_applied;
  if (!scar)
    return {false, steps, "shellcode donor scar failed"};

  std::printf("[T0 shellcode_inject_donor] step %d: obfuscated shellcode injected into donor pid=%u\n", ++steps, donor);
  w.note("shellcode_inject_donor: obfuscated shellcode injection into donor");
  return {true, steps, "shellcode_inject_donor: xor-obfuscated injection", donor, true};
}

}  // namespace examples::shellcode_inject_donor
