#include "blue_example.hpp"
#include "server/ban_correlator.hpp"
#include "ac/risk_score.hpp"

namespace examples::delayed_ban {

BlueResult detect(sim::World& w) {
  BlueResult r;
  r.ticks_seen = w.lab_match_tick;
  r.multi_tick = w.lab_match_tick >= 2;
  ac::RiskAggregator risk;
  bool foreign = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      const auto* p = w.proc(h.owner_pid);
      if (p && !p->is_game && !p->is_ac) foreign = true;
    }
  }
  if (foreign) {
    ac::TelemetryEvent ev;
    ev.kind = ac::EventKind::HandleToGame;
    ev.detail = "weak_handle";
    ev.risk_delta = 2.0;
    risk.ingest(ev);
  }
  if (w.binary_build_id != "shared" && !w.binary_build_id.empty()) {
    ac::TelemetryEvent ev;
    ev.kind = ac::EventKind::Generic;
    ev.detail = "unique_build";
    ev.risk_delta = 1.5;
    risk.ingest(ev);
  }
  server::BanCorrelator corr;
  auto d = corr.evaluate(risk.state(), w.lab_confidence);
  r.detected = r.multi_tick && (w.lab_delayed_ban_ready || d.multi_signal ||
                                d.action != server::BanAction::None || foreign);
  if (r.detected) {
    r.mitigated = true;
    w.overwatch_queued = true;
    if (w.lab_delayed_ban_ready) {
      w.ranked_access_denied = true;
    }
  }
  r.detail = "delayed_ban blue ticks=" + std::to_string(r.ticks_seen) +
             " multi_tick=" + std::to_string(r.multi_tick ? 1 : 0) +
             " ban_ready=" + std::to_string(w.lab_delayed_ban_ready ? 1 : 0) +
             " action=" + std::to_string(static_cast<int>(d.action));
  w.note(r.detail);
  return r;
}

}  // namespace examples::delayed_ban
