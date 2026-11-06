#include "red_example.hpp"
#include <cstdio>

namespace examples::peb_spoof_hide {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "game unavailable"};

  const auto spoofer = w.spawn("peb-spoofer.exe");
  if (!w.proc(spoofer))
    return {false, steps, "spoofer spawn failed"};

  w.peb_spoof_active = true;
  w.peb_being_debugged_cleared = true;
  w.peb_nt_global_flag_cleared = true;
  w.peb_being_debugged_spoofed = true;

  const bool scar = w.peb_spoof_active && w.peb_being_debugged_cleared && w.peb_nt_global_flag_cleared;
  if (!scar)
    return {false, steps, "PEB spoof scar failed"};

  std::printf("[T0 peb_spoof_hide] step %d: BeingDebugged and NtGlobalFlag cleared\n", ++steps);
  w.note("peb_spoof_hide: PEB BeingDebugged/NtGlobalFlag cleared");
  return {true, steps, "peb_spoof_hide: PEB debug flags cleared", true, true};
}

}  // namespace examples::peb_spoof_hide
