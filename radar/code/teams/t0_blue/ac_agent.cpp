// ac_agent.cpp — T0 blue agent: handle-graph + co-occurrence sensors on World.
// run_full_loop / detect path emits telemetry and risk.

#include "t0_blue/ac_agent.hpp"

#include <sstream>

namespace t0_blue {

// AcAgent::AcAgent: T0 blue agent: handle graph + co-occurrence + risk sink.
AcAgent::AcAgent(sim::World& world, ac::ITelemetrySink& sink)
    : world_(world),
      sink_(sink),
      handles_(sink),
      co_(sink) {
  fp_.allow_process_name("nvidia-overlay");
  fp_.allow_process_name("obs64");
  fp_.allow_process_name("discord");
  for (const auto& p : world_.list_processes(false)) {
    if (p.is_game) {
      game_pid_ = p.pid;
    }
    if (p.is_ac) {
      self_pid_ = p.pid;
    }
  }
  if (game_pid_ == 0) {
    game_pid_ = world_.game_pid();
  }
}

// AcAgent::merge_: Merge two findings into combined risk/detail.
void AcAgent::merge_(Detection& into, const Detection& part) {
  into.handle_hit = into.handle_hit || part.handle_hit;
  into.cooccurrence_hit = into.cooccurrence_hit || part.cooccurrence_hit;
  into.inject_hit = into.inject_hit || part.inject_hit;
  into.module_integrity_hit =
      into.module_integrity_hit || part.module_integrity_hit;
  into.overlay_hit = into.overlay_hit || part.overlay_hit;
  into.race_hit = into.race_hit || part.race_hit;
  into.reasons.insert(into.reasons.end(), part.reasons.begin(),
                      part.reasons.end());
}

// AcAgent::scan_handles: Blue: find foreign VmRead handles into game pid.
Detection AcAgent::scan_handles() {
  Detection d;
  auto sample = handles_.multi_sample(world_, game_pid_);
  for (const auto& e : sample.suspicious) {
    sim::Handle h{};
    h.owner_pid = e.source_pid;
    h.target_pid = e.target_pid;
    h.access = sim::AccessMask::VmRead;
    h.via_syscall_path = e.via_syscall;
    h.hidden_during_enum = e.hidden_during_enum;
    h.brief_reopen = e.brief_reopen;
    auto dec = fp_.evaluate_edge(h, world_);
    if (!dec.alert) {
      d.reasons.push_back("suppressed:" + e.source_name + ":" + dec.reason);
      continue;
    }
    d.handle_hit = true;
    d.reasons.push_back("VM_READ from " + e.source_name + " pid=" +
                        std::to_string(e.source_pid) + " (" + dec.reason + ")" +
                        (e.via_syscall ? " syscall" : " winapi") +
                        (e.hidden_during_enum ? " hidden_enum" : ""));
    ac::TelemetryEvent ev{
        .kind = ac::EventKind::HandleToGame,
        .related_tier = ac::Tier::T0_UsermodeRpm,
        .subject_pid = e.source_pid,
        .object_pid = game_pid_,
        .detail = e.source_name,
        .risk_delta = 3.0,
    };
    sink_.emit(ev);
    risk_.ingest(ev);
  }
  if (sample.race_suspected) {
    d.race_hit = true;
    d.reasons.push_back("handle_enum_race " + sample.detail);
  }
  d.risk = risk_.state().score;
  d.summary = sample.detail;
  return d;
}

// AcAgent::scan_cooccurrence: Blue: flag suspicious processes co-resident with game.
Detection AcAgent::scan_cooccurrence() {
  Detection d;
  auto r = co_.scan_world(world_, game_pid_, /*weak_enum=*/true);
  d.cooccurrence_hit = r.hit;
  for (const auto& h : r.hits) {
    d.reasons.push_back("co-run " + h.name + " (" + h.reason + ")");
    risk_.ingest(ac::TelemetryEvent{
        .kind = ac::EventKind::ProcessCoRun,
        .related_tier = ac::Tier::T0_UsermodeRpm,
        .subject_pid = h.pid,
        .object_pid = game_pid_,
        .detail = h.name,
        .risk_delta = h.risk_delta,
    });
  }
  d.risk = risk_.state().score;
  d.summary = r.detail;
  return d;
}

// AcAgent::scan_injection: Scan World for injection residuals.
Detection AcAgent::scan_injection() {
  Detection d;
  auto* g = world_.proc(game_pid_);
  if (!g) {
    return d;
  }
  if (g->has_foreign_thread || g->thread_hijacked) {
    d.inject_hit = true;
    d.reasons.push_back(g->thread_hijacked ? "thread_hijacked" : "foreign_thread");
    risk_.ingest({ac::EventKind::HandleToGame, ac::Tier::T0_UsermodeRpm, 0,
                  game_pid_, "inject_thread", 2.5});
  }
  if (g->manual_mapped_region) {
    d.inject_hit = true;
    d.reasons.push_back("manual_mapped_region");
  }
  for (const auto& m : g->modules) {
    if (!m.linked_in_peb || m.headers_erased) {
      d.inject_hit = true;
      d.reasons.push_back("module_stealth " + m.name);
    }
  }
  d.risk = risk_.state().score;
  return d;
}

// AcAgent::scan_module_integrity: Scan World for module integrity residuals.
Detection AcAgent::scan_module_integrity() {
  Detection d;
  auto* g = world_.proc(game_pid_);
  if (!g) {
    return d;
  }
  for (const auto& m : g->modules) {
    if (m.text_hash != "clean") {
      d.module_integrity_hit = true;
      d.reasons.push_back("text_hash " + m.name + "=" + m.text_hash);
    }
    if (m.iat_hooked || m.eat_hooked || m.present_hooked) {
      d.module_integrity_hit = true;
      d.reasons.push_back(std::string("hook ") + m.name +
                          (m.iat_hooked ? " iat" : "") +
                          (m.eat_hooked ? " eat" : "") +
                          (m.present_hooked ? " present" : ""));
    }
  }
  d.risk = risk_.state().score;
  return d;
}

// AcAgent::scan_overlays: Scan World for overlays residuals.
Detection AcAgent::scan_overlays() {
  Detection d;
  for (const auto& o : world_.overlays) {
    if (o.owner_pid == game_pid_ || o.owner_pid == self_pid_) {
      continue;
    }
    if (o.topmost || o.transparent || o.hijacks_swapchain) {
      d.overlay_hit = true;
      d.reasons.push_back("overlay \"" + o.title + "\" pid=" +
                          std::to_string(o.owner_pid));
      risk_.ingest({ac::EventKind::ProcessCoRun, ac::Tier::T0_UsermodeRpm,
                    o.owner_pid, game_pid_, o.title, 0.5});
    }
  }
  // Capture residuals
  if (world_.gdi_bitblt_capture || world_.printwindow_capture) {
    d.overlay_hit = true;
    d.reasons.push_back(world_.gdi_bitblt_capture ? "gdi_bitblt_capture"
                                                 : "printwindow_capture");
  }
  d.risk = risk_.state().score;
  return d;
}

// AcAgent::scan_handle_race: Scan World for handle race residuals.
Detection AcAgent::scan_handle_race() {
  Detection d;
  auto sample = handles_.multi_sample(world_, game_pid_);
  if (sample.race_suspected || sample.hidden_edges > 0) {
    d.race_hit = true;
    d.handle_hit = sample.truth_foreign_vm_read > 0;
    d.reasons.push_back("multi_sample " + sample.detail);
    risk_.ingest({ac::EventKind::HandleToGame, ac::Tier::T0_UsermodeRpm, 0,
                  game_pid_, "handle_race", 2.0});
  }
  d.risk = risk_.state().score;
  d.summary = sample.detail;
  return d;
}

// AcAgent::full_scan: Blue: run handle + co-occurrence (and related) sensors once.
Detection AcAgent::full_scan() {
  Detection d;
  merge_(d, scan_handles());
  merge_(d, scan_cooccurrence());
  merge_(d, scan_injection());
  merge_(d, scan_module_integrity());
  merge_(d, scan_overlays());
  // scan_handles already multi-samples; race reasons included.
  d.risk = risk_.state().score;
  std::ostringstream oss;
  oss << "handle=" << d.handle_hit << " co=" << d.cooccurrence_hit
      << " inject=" << d.inject_hit << " mod=" << d.module_integrity_hit
      << " overlay=" << d.overlay_hit << " race=" << d.race_hit
      << " risk=" << d.risk << " reasons=" << d.reasons.size();
  d.summary = oss.str();
  return d;
}

}  // namespace t0_blue
