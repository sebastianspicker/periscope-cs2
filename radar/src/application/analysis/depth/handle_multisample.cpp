// handle_multisample.cpp — depth analytics: multi-sample / fusion scoring for blue.
// Educational scorers over World residuals.

#include "depth/handle_multisample.hpp"

#include <sstream>
#include <unordered_set>

namespace depth {
namespace {

// is_foreign_reader: free function for this educational unit.
bool is_foreign_reader(const sim::World& w, std::uint32_t owner) {
  const auto* p = w.proc(owner);
  return p && !p->is_game && !p->is_ac;
}

}  // namespace

// MultiSampleHandleDetector::sample: Sample World handles into multi-sample window.
HandleSample MultiSampleHandleDetector::sample(const sim::World& w,
                                               int tick) const {
  HandleSample s;
  s.tick = tick;
  const auto game = w.game_pid();
  if (game == 0) {
    return s;
  }

  std::unordered_set<std::uint32_t> vis;
  std::unordered_set<std::uint32_t> truth;

  for (const auto& h : w.handles_to(game, /*include_hidden=*/false)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    if (!is_foreign_reader(w, h.owner_pid)) {
      continue;
    }
    ++s.foreign_vm_read_visible;
    vis.insert(h.owner_pid);
    if (h.brief_reopen) {
      ++s.brief_reopen_edges;
    }
  }

  for (const auto& h : w.handles_to(game, /*include_hidden=*/true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    if (!is_foreign_reader(w, h.owner_pid)) {
      continue;
    }
    ++s.foreign_vm_read_truth;
    truth.insert(h.owner_pid);
    if (h.hidden_during_enum) {
      ++s.hidden_edges;
    }
    // Truth path: count brief_reopen even when hidden during visible enum.
    // Visible pass already counted open-on-sample edges; hidden-only brief
    // races must still increment so multi-sample composition sees them.
    if (h.brief_reopen && h.hidden_during_enum) {
      ++s.brief_reopen_edges;
    }
  }

  s.visible_owner_pids.assign(vis.begin(), vis.end());
  s.truth_owner_pids.assign(truth.begin(), truth.end());
  return s;
}

// MultiSampleHandleDetector::push: Push one handle-sample into the detector window.
void MultiSampleHandleDetector::push(const HandleSample& s) {
  samples_.push_back(s);
}

// MultiSampleHandleDetector::evaluate: Syscall-aware monitor: correlate handles with syscall/soft signals.
MultiSampleHandleResult MultiSampleHandleDetector::evaluate(
    const sim::World& w) const {
  MultiSampleHandleResult r;
  r.samples_taken = static_cast<int>(samples_.size());

  int min_vis = 1000000;
  for (const auto& s : samples_) {
    if (s.foreign_vm_read_visible > r.max_visible) {
      r.max_visible = s.foreign_vm_read_visible;
    }
    if (s.foreign_vm_read_truth > r.max_truth) {
      r.max_truth = s.foreign_vm_read_truth;
    }
    if (s.foreign_vm_read_visible < min_vis) {
      min_vis = s.foreign_vm_read_visible;
    }
    if (s.foreign_vm_read_truth > 0 || s.hidden_edges > 0) {
      r.continuous_hit = true;
    }
  }
  if (samples_.empty()) {
    min_vis = 0;
  }

  // Race: AC samples disagree on visibility while truth held a handle.
  r.race_detected =
      r.samples_taken >= 2 && r.continuous_hit && (r.max_visible != min_vis ||
                                                   r.max_visible < r.max_truth);

  // Lineage composition: reader parent is short-lived stub / non-explorer chain.
  std::unordered_set<std::uint32_t> owners;
  for (const auto& s : samples_) {
    for (auto pid : s.truth_owner_pids) {
      owners.insert(pid);
    }
  }
  for (auto pid : owners) {
    const auto* p = w.proc(pid);
    if (!p) {
      continue;
    }
    LineageHit lh;
    lh.reader_pid = pid;
    lh.parent_pid = p->parent_pid;
    if (p->parent_pid != 0) {
      const auto* parent = w.proc(p->parent_pid);
      if (parent) {
        lh.parent_name = parent->name;
        // Suspicious: parent is stub loader, or parent is not a known shell.
        const bool known_shell =
            parent->name.find("explorer") != std::string::npos ||
            parent->name.find("cmd") != std::string::npos ||
            parent->name.find("pwsh") != std::string::npos;
        lh.parent_suspicious =
            parent->name.find("stub") != std::string::npos ||
            parent->name.find("loader") != std::string::npos ||
            parent->name.find("stage") != std::string::npos || !known_shell;
      } else {
        lh.parent_suspicious = true;
        lh.parent_name = "missing";
      }
    }
    if (lh.parent_suspicious) {
      r.lineage_hit = true;
    }
    r.lineage.push_back(lh);

    ReputationHit rh;
    rh.pid = pid;
    rh.name = p->name;
    rh.looks_reputable = p->looks_reputable;
    rh.allowlisted = p->looks_reputable;  // solo allowlist would suppress
    r.reputation.push_back(rh);
    // Evasion: looks reputable BUT multi-sample race or lineage still dirty.
    if (p->looks_reputable && (r.race_detected || r.lineage_hit)) {
      r.reputation_evasion = true;
    }
  }

  // Composition score: continuous + race + lineage + reputation_evasion
  if (r.continuous_hit) {
    r.score += 2.0;
  }
  if (r.race_detected) {
    r.score += 2.5;
  }
  if (r.lineage_hit) {
    r.score += 1.5;
  }
  if (r.reputation_evasion) {
    r.score += 2.0;
  }
  // Brief reopen across samples
  for (const auto& s : samples_) {
    if (s.brief_reopen_edges > 0) {
      r.score += 1.0;
      break;
    }
  }

  // Composed hit requires multi-sample continuous truth OR race, not single bool.
  r.composed_hit =
      r.samples_taken >= 2 &&
      (r.race_detected || (r.continuous_hit && r.lineage_hit) ||
       (r.continuous_hit && r.reputation_evasion) || r.score >= 3.0);

  std::ostringstream oss;
  oss << "samples=" << r.samples_taken << " max_vis=" << r.max_visible
      << " max_truth=" << r.max_truth << " race=" << (r.race_detected ? 1 : 0)
      << " continuous=" << (r.continuous_hit ? 1 : 0)
      << " lineage=" << (r.lineage_hit ? 1 : 0)
      << " rep_evasion=" << (r.reputation_evasion ? 1 : 0)
      << " score=" << r.score
      << " composed=" << (r.composed_hit ? 1 : 0);
  r.detail = oss.str();
  return r;
}

HandleRaceScript run_handle_race(sim::World& w, int samples, bool use_syscall,
                                 bool claim_reputable,
                                 std::uint32_t parent_pid) {
  HandleRaceScript out;
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  if (!g) {
    out.detail = "no game";
    return out;
  }
  if (samples < 2) {
    samples = 2;  // multi-sample composition requires ≥2
  }

  MultiSampleHandleDetector det;

  // Optional suspicious parent (loader stub) — lineage composition signal.
  std::uint32_t parent = parent_pid;
  if (parent == 0) {
    parent = w.spawn("stage-loader.exe", false, false, 0);
  }
  const auto red = w.spawn("radar-ms.exe", false, false, parent);
  out.actor_pid = red;
  if (auto* p = w.proc(red)) {
    p->looks_reputable = claim_reputable;
  }

  // Tick 0: open handle, take sample while visible (AC enum would see it).
  out.opened = w.open_process(red, game, sim::AccessMask::VmRead, use_syscall);
  if (!out.opened) {
    out.detail = "open failed";
    return out;
  }
  det.push(det.sample(w, 0));

  // Tick 1: hide during enum (race against AC multi-sample window).
  for (auto& h : w.handles) {
    if (h.owner_pid == red && h.target_pid == game) {
      h.hidden_during_enum = true;
      h.brief_reopen = true;
    }
  }
  out.hid = true;
  det.push(det.sample(w, 1));

  // Still can read while "hidden" from enum (truth continuous VM_READ).
  auto rr = w.read_mem(red, game, g->base, 4, true);
  out.read_ok = rr.status == ac::Status::Ok;

  // Subsequent ticks: brief reopen windows interleaved with hide (race script).
  for (int t = 2; t < samples; ++t) {
    const bool show = (t % 2) == 0;
    for (auto& h : w.handles) {
      if (h.owner_pid == red && h.target_pid == game) {
        h.hidden_during_enum = !show;
        if (show) {
          h.brief_reopen = true;
        }
      }
    }
    if (show) {
      out.reopened = true;
    }
    det.push(det.sample(w, t));
  }
  // If only 2 samples requested, still mark reopen for narrative completeness.
  if (!out.reopened) {
    for (auto& h : w.handles) {
      if (h.owner_pid == red && h.target_pid == game) {
        h.hidden_during_enum = false;
        h.brief_reopen = true;
      }
    }
    out.reopened = true;
    if (static_cast<int>(det.samples().size()) < samples) {
      det.push(det.sample(w, static_cast<int>(det.samples().size())));
    }
  }

  // Ensure at least 2 samples always (composition invariant).
  while (static_cast<int>(det.samples().size()) < 2) {
    det.push(det.sample(w, static_cast<int>(det.samples().size())));
  }

  auto eval = det.evaluate(w);
  w.note("handle_race " + eval.detail);
  out.detail = eval.detail;
  return out;
}

}  // namespace depth
