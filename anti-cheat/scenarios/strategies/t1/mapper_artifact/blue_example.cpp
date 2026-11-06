// BLUE example implementation for this strategy pair.
// Multi-reason sensors on World scars; educational detect/mitigate path.

#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace examples::mapper_artifact {

namespace {

bool name_looks_like_mapper(const std::string& name) {
  // Case-insensitive-ish lab check on common mapper tooling names.
  std::string lower = name;
  for (auto& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return lower.find("kdmapper") != std::string::npos ||
         lower.find("mapper") != std::string::npos;
}

}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult r;

  const bool flag_hit = w.mapper_process_present;
  int name_hits = 0;
  for (const auto& p : w.list_processes(false)) {
    if (name_looks_like_mapper(p.name)) {
      ++name_hits;
    }
  }
  const bool pool_hit = w.pool_tag_anomaly;

  bool unsigned_mem_rw = false;
  for (const auto& d : w.drivers) {
    if (!d.is_ac && d.provides_mem_rw &&
        (d.signer == "unsigned" || d.sha256.find("unsigned") != std::string::npos)) {
      unsigned_mem_rw = true;
      break;
    }
  }

  if (flag_hit) {
    r.reasons.emplace_back("mapper_process_present world scar is set");
  }
  if (name_hits > 0) {
    r.reasons.emplace_back("process name contains mapper/kdmapper (" +
                           std::to_string(name_hits) + ")");
  }
  if (pool_hit) {
    r.reasons.emplace_back("pool_tag_anomaly co-artifact present");
  }
  if (unsigned_mem_rw) {
    r.reasons.emplace_back(
        "unsigned non-AC driver exposes memory read/write capability");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.28);
  r.detected = r.signals >= 2;
  r.mitigated = r.detected;
  r.detail = "flag=" + std::to_string(flag_hit ? 1 : 0) +
             " name_hits=" + std::to_string(name_hits) +
             " pool=" + std::to_string(pool_hit ? 1 : 0) +
             " risk=" + std::to_string(r.risk);

  std::printf(
      "[blue:mapper_artifact] detected=%d mitigated=%d flag=%d name_hits=%d\n",
      static_cast<int>(r.detected), static_cast<int>(r.mitigated),
      static_cast<int>(flag_hit), name_hits);
  return r;
}

}  // namespace examples::mapper_artifact
