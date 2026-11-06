#include "red_example.hpp"
#include <sstream>
namespace examples::dll_thread_protect {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("protected-modules.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  // Depth beyond 45 suite booleans.
  w.dll_protection_active = true;
  w.thread_protection_active = true;
  w.protector_find_target = true;
  w.protected_thread_count = 4;
  // May also set suite flags, but blue keys on *detail* scars.
  w.anti_debug_active = true;
  r.achieved = w.dll_protection_active && w.thread_protection_active &&
               w.protected_thread_count >= 2 && w.protector_find_target;
  std::ostringstream oss;
  oss << "dll_thread_protect red dll_prot=1 thread_prot=1 "
      << "find_target=1 protected_threads=" << w.protected_thread_count
      << " (deeper than protector_suite)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::dll_thread_protect
