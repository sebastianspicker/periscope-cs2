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
#if LR_HAS_REAL_PLATFORM
#include "real/real_fwd.hpp"
#endif

namespace strategies {

// ── run_real ───────────────────────────────────────────────────────
RealStrategyResult run_real(const char* strategy_id, bool verbose) {
#if LR_HAS_REAL_PLATFORM
  RealStrategyResult result;
  auto proc = real::cs2::find_cs2_process();
  if (!proc) { result.error_msg = "CS2 not found: " + proc.error_msg; if (verbose) std::printf("[real] %s\n", result.error_msg.c_str()); return result; }
  auto pid = proc->pid;
  if (verbose) std::printf("[real] Found CS2: pid=%u name=%s\n", pid, proc->name.c_str());
  auto handle = real::cs2::open_cs2_process(pid);
  if (!handle) { result.error_msg = "Failed to open CS2 handle: " + handle.error_msg; if (verbose) std::printf("[real] %s\n", result.error_msg.c_str()); return result; }
  result.handle_opened = true;
  if (verbose) std::printf("[real] Handle opened: 0x%llx\n", (unsigned long long)*handle);
  auto base = real::cs2::get_cs2_base_address(pid, *handle);
  if (!base) { result.error_msg = "Failed to get CS2 base: " + base.error_msg; real::cs2::detach_from_cs2(*handle); return result; }
  auto image_size = real::cs2::get_cs2_image_size(pid, *handle);
  if (!image_size) { result.error_msg = "Failed image size: " + image_size.error_msg; real::cs2::detach_from_cs2(*handle); return result; }
  if (verbose) std::printf("[real] CS2 base=0x%llx size=%zu\n", (unsigned long long)*base, *image_size);
  auto offsets = real::cs2::resolve_offsets(pid, *base, *image_size);
  if (!offsets) result.error_msg = "Offset resolution failed: " + offsets.error_msg;
  real::cs2::Cs2MemoryReader reader;
  auto status = reader.attach(ac::Tier::T0_UsermodeRpm, pid);
  bool reader_attached = (status == ac::Status::Ok);
  if (reader_attached && offsets) {
    auto entities = real::cs2::read_entity_list(reader, *offsets, pid);
    result.entities_read = entities.entity_count;
    result.red_achieved = entities.read_successful && entities.entity_count > 0;
    auto local = real::cs2::read_local_player(reader, *offsets);
    if (verbose && result.red_achieved) std::printf("[real] Read %d entities, local health=%d\n", entities.entity_count, local ? (int)local->health : -1);
    result.summary = entities.describe();
    result.blue_would_detect = result.handle_opened;
    reader.detach();
  }
  real::cs2::detach_from_cs2(*handle);
  return result;
#else
  (void)strategy_id; (void)verbose;
  RealStrategyResult result; result.error_msg = "Real platform unavailable."; return result;
#endif
}

// ── run_dual ───────────────────────────────────────────────────────
DualResult run_dual(const char* strategy_id, bool verbose) {
  DualResult dr;
#if LR_HAS_REAL_PLATFORM
  const auto& cfg = real::mode::config();
  if (cfg.may_use_real()) {
    if (verbose) std::printf("\n═══ DUAL MODE: REAL CS2 ═══\n");
    dr.real = run_real(strategy_id, verbose);
    if (dr.real.handle_opened || dr.real.red_achieved) dr.mode_used = "real";
  }
  if (cfg.may_use_sim()) {
    if (verbose && dr.real.handle_opened) std::printf("\n═══ DUAL MODE: SIM (comparison) ═══\n");
    const auto* entry = find(strategy_id);
    if (entry) {
      sim::Narrator n;
      auto world = sim::make_arena();
      dr.sim = entry->run(world, n);
      if (verbose) {
        std::printf("    red_achieved=%d blue_detected=%d blue_mitigated=%d\n    %s\n",
                    dr.sim.red_achieved ? 1 : 0, dr.sim.blue_detected ? 1 : 0,
                    dr.sim.blue_mitigated ? 1 : 0, dr.sim.summary.c_str());
      }
    } else {
      dr.sim.summary = "Unknown strategy";
    }
    dr.mode_used = (dr.real.handle_opened || dr.real.red_achieved) ? "hybrid" : "sim";
  }
#else
  const auto* entry = find(strategy_id);
  if (entry) { sim::Narrator n; auto w = sim::make_arena(); dr.sim = entry->run(w, n); }
  else { dr.sim.summary = "Unknown strategy"; }
  dr.mode_used = "sim";
#endif
  if (verbose) std::printf("%s\n", dr.describe().c_str());
  return dr;
}

// ── supports_real_mode ────────────────────────────────────────────
bool supports_real_mode(const StrategyMeta& meta) {
#if LR_HAS_REAL_PLATFORM
  const char* t = meta.tiers;
  return t && (std::strstr(t, "T0") || std::strstr(t, "T1") || std::strstr(t, "all"));
#else
  (void)meta; return false;
#endif
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
