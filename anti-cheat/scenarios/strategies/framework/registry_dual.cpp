// registry_dual.cpp — Dual-mode (real + sim) strategy execution.
// Split from the original 610-line monolith registry.cpp.
// Requires real platform backends; gracefully degrades when absent.

#include "strategies/framework.hpp"
#include "sim/world.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>

#ifndef LR_HAS_REAL_PLATFORM
#define LR_HAS_REAL_PLATFORM 0
#endif

namespace strategies {

// ── run_real ───────────────────────────────────────────────────────
RealStrategyResult run_real(const char* strategy_id, bool verbose) {
  (void)strategy_id; (void)verbose;
  RealStrategyResult result; result.error_msg = "Real platform unavailable."; return result;
}

// ── run_dual ───────────────────────────────────────────────────────
DualResult run_dual(const char* strategy_id, bool verbose) {
  DualResult dr;
  const auto* entry = find(strategy_id);
  if (entry) { sim::Narrator n; auto w = sim::make_arena(); dr.sim = entry->run(w, n); }
  else { dr.sim.summary = "Unknown strategy"; }
  dr.mode_used = "sim";
  if (verbose) std::printf("%s\n", dr.describe().c_str());
  return dr;
}

// ── supports_real_mode ────────────────────────────────────────────
bool supports_real_mode(const StrategyMeta& meta) {
  (void)meta; return false;
}

// ── DualResult::describe ──────────────────────────────────────────
std::string DualResult::describe() const {
  std::string out;
  if (real.handle_opened || real.red_achieved) {
    out += "[REAL] "; out += real.red_achieved ? "Red achieved: YES (" + std::to_string(real.entities_read) + " entities)\n" : "Red achieved: NO (" + real.error_msg + ")\n";
    out += "[REAL] Handle opened: "; out += real.handle_opened ? "YES" : "NO"; out += "\n";
    out += "[REAL] Blue would detect: "; out += real.blue_would_detect ? "YES (handle scar)" : "NO"; out += "\n";
    out += "[REAL] "; out += real.summary.empty() ? real.error_msg : real.summary; out += "\n";
  }
  out += "[SIM]  Red achieved: "; out += sim.red_achieved ? "YES" : "NO";
  out += " | Blue detected: "; out += sim.blue_detected ? "YES" : "NO";
  out += " | Mitigated: "; out += sim.blue_mitigated ? "YES" : "NO";
  out += "\n[SIM]  "; out += sim.summary; out += "\n";
  out += "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\nLESSON: ";
  if (real.handle_opened && sim.blue_detected) out += "Both REAL and SIM show detection.\n";
  else if (real.handle_opened && !sim.blue_detected) out += "REAL detected but SIM missed it.\n";
  else if (!real.handle_opened && sim.blue_detected) out += "SIM detects but REAL can't attach.\n";
  else out += "Neither mode detected the technique.\n";
  return out;
}

}  // namespace strategies
