#include "red_example.hpp"

#include <cstdio>

bool self_obfuscate_process_red_apply(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "step 1: validate game process for identity-disguise plant");
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (game == nullptr || game_pid == 0) return false;

  n.say(sim::Side::Red, "step 2: spawn actor disguised as legitimate RTSS monitor");
  const auto actor_pid = w.spawn("rtss.exe");
  auto* actor = w.proc(actor_pid);
  if (actor == nullptr) return false;

  n.say(sim::Side::Red, "step 3: plant multi-field identity façade (name/window/cmdline/WS)");
  actor->window_class = "RTSSHooks";
  actor->command_line = "rtss.exe /silent";
  actor->working_set_kb = 18432;
  actor->timing_jittered = true;
  actor->peb_identity_matched = true;
  actor->modules.push_back(
      {"RTSSHooks64.dll", actor->base + 0x20000, 0x3000, true, false, "clean"});
  actor->reader_active = true;

  n.say(sim::Side::Red, "step 4: open VM_READ on game under the disguised identity");
  if (!w.open_process(actor_pid, game_pid, sim::AccessMask::VmRead, false)) {
    return false;
  }

  n.say(sim::Side::Red, "step 5: perform protected entity peek (second scar family)");
  auto read = w.read_mem(actor_pid, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    return false;
  }

  n.say(sim::Side::Red, "step 6: verify disguise + handle/read residual retained");
  bool has_handle = false;
  for (const auto& h : w.handles_to(game_pid, true)) {
    if (h.owner_pid == actor_pid && sim::has(h.access, sim::AccessMask::VmRead)) {
      has_handle = true;
      break;
    }
  }
  const bool scar = has_handle && actor->reader_active && actor->timing_jittered &&
                    actor->window_class == "RTSSHooks" && w.remote_read_ops > 0;
  if (!scar) return false;

  std::printf("[red:self_obfuscate_process] achieved: rtss façade + VM_READ + read ops\n");
  w.note("self_obfuscate_process: RTSS identity façade with live game reader");
  return true;
}
