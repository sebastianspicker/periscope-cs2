// RED example implementation for this strategy pair.
// Multi-step World scars for learners; pairs with blue_example detect/mitigate.

#include "red_example.hpp"

#include <cstdio>

namespace examples::thread_hide_dbg {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();

  std::printf("[red:thread_hide_dbg] verify a game process is present\n");
  if (game == 0 || w.proc(game) == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:thread_hide_dbg] spawn an anti-debug lab actor\n");
  const auto actor = w.spawn("thread-hide-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    r.detail = "actor process creation failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:thread_hide_dbg] hide thread from debugger visibility\n");
  w.thread_hide_from_debugger = true;
  if (!w.thread_hide_from_debugger) {
    r.detail = "could not plant thread_hide_from_debugger scar";
    return r;
  }
  ++r.steps;

  std::printf("[red:thread_hide_dbg] spoof PEB BeingDebugged / related flags\n");
  w.peb_being_debugged_spoofed = true;
  if (!w.peb_being_debugged_spoofed) {
    r.detail = "could not plant peb_being_debugged_spoofed scar";
    return r;
  }
  ++r.steps;

  std::printf("[red:thread_hide_dbg] optional soft attach: open game VM_READ handle\n");
  const bool opened = w.open_process(actor, game, sim::AccessMask::VmRead, false);
  if (opened) {
    ++r.steps;
  }

  r.achieved = w.thread_hide_from_debugger && w.peb_being_debugged_spoofed;
  r.detail = r.achieved
                 ? (opened ? "ThreadHide + PEB spoof planted with optional game handle"
                           : "ThreadHide + PEB spoof planted (no handle path)")
                 : "thread hide / PEB spoof incomplete";
  w.note(r.detail);
  std::printf("[red:thread_hide_dbg] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

}  // namespace examples::thread_hide_dbg
