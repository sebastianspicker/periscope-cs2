#include "red_example.hpp"

#include <cstdio>

namespace examples::convars_temp_restore {

/// Red saves ConVar, modifies it, performs technique, then restores.
/// SCAR: window residual + continuous CRC sample + live technique co-scars.
Result apply(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "step 1: ensure educational ConVar surface exists");
  cs2::SimulatedConVar* sv = nullptr;
  for (auto& c : w.diagnostic_state.convars) {
    if (c.name == "sv_cheats") {
      sv = &c;
      break;
    }
  }
  if (sv == nullptr) {
    w.diagnostic_state.convars.push_back({"sv_cheats", "0", true, false, 0});
    sv = &w.diagnostic_state.convars.back();
  }

  const auto game = w.game_pid();
  auto* g = w.proc(game);
  if (g == nullptr) {
    Result r;
    r.achieved = false;
    r.detail = "game unavailable";
    return r;
  }

  n.say(sim::Side::Red, "step 2: spawn technique actor for mid-window residual");
  const auto actor = w.spawn("convar-temp-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    Result r;
    r.achieved = false;
    r.detail = "actor spawn failed";
    return r;
  }

  n.say(sim::Side::Red, "step 3: save original, modify sv_cheats for technique window");
  const std::string original = sv->value;
  sv->value = "1";
  sv->tampered = true;

  n.say(sim::Side::Red, "step 4: perform technique during dirty window (open+read)");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, false)) {
    Result r;
    r.achieved = false;
    r.detail = "open_process failed during technique window";
    return r;
  }
  (void)w.read_mem(actor, game, g->base, 4, true);

  // Continuous/trigger collectors sample the dirty window.
  w.note("convars_temp_restore: technique window with sv_cheats=1");
  w.note("convars_temp_restore: continuous_crc_sample_dirty");
  w.diag_convar_checked = true;

  n.say(sim::Side::Red, "step 5: restore original ConVar value before end-of-frame CRC");
  sv->value = original;
  sv->tampered = false;

  const bool residual =
      w.remote_read_ops > 0 && w.diag_convar_checked && !w.log.empty();
  Result r;
  r.achieved = residual;
  r.restored = true;
  r.detail = residual
                 ? "ConVar modified+restored; window residual + read co-scar retained"
                 : "restore succeeded but residual plant failed";
  n.say(sim::Side::Red, r.detail);
  std::printf("[red:convars_temp_restore] achieved=%d restored=1 reads=%llu\n",
              r.achieved ? 1 : 0,
              static_cast<unsigned long long>(w.remote_read_ops));
  return r;
}

}  // namespace examples::convars_temp_restore
