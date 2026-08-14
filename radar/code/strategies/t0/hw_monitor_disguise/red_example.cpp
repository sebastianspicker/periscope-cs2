#include "red_example.hpp"
#include <cstdio>

namespace examples::hw_monitor_disguise {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto monitor = w.spawn("RTSS.exe");
  auto* mon_proc = w.proc(monitor);
  if (!mon_proc)
    return {false, steps, "monitor spawn failed"};

  mon_proc->looks_reputable = true;

  w.hw_monitor_disguise_active = true;
  w.hw_monitor_decoy_osd_active = true;
  w.hw_monitor_rtss_hijack = true;

  sim::OverlayWindow ov;
  ov.owner_pid = monitor;
  ov.title = "RTSS OSD";
  ov.topmost = true;
  ov.transparent = true;
  ov.stream_proof = true;
  w.add_overlay(ov);

  const bool scar = w.hw_monitor_disguise_active && w.hw_monitor_decoy_osd_active && w.hw_monitor_rtss_hijack;
  if (!scar)
    return {false, steps, "hw monitor disguise scar failed"};

  std::printf("[T0 hw_monitor_disguise] step %d: RTSS/Afterburner decoy OSD active\n", ++steps);
  w.note("hw_monitor_disguise: RTSS/Afterburner decoy OSD");
  return {true, steps, "hw_monitor_disguise: decoy OSD via RTSS", true, true};
}

}  // namespace examples::hw_monitor_disguise
