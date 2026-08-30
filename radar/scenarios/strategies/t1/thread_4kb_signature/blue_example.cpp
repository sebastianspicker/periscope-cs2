#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

namespace strategy::t1_thread_4kb_signature {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult r;
  int signals = 0;

  std::printf("[blue:thread_4kb_signature] sensor 1: memory protection of thread start\n");
  if (w.thread_memory_protection == "PAGE_EXECUTE_READWRITE") {
    r.rwx_detected = true;
    r.detail += "rwx_protection ";
    ++signals;
  }

  std::printf("[blue:thread_4kb_signature] sensor 2: 4KB signature buffer pattern scan\n");
  constexpr std::string_view bad_strings[] = {
      "OpenProcess", "ReadProcessMemory", "cheat", "radar", "VAC", "injection"};
  for (const auto s : bad_strings) {
    const std::size_t slen = s.size();
    if (w.thread_start_4kb.size() < slen) continue;
    for (std::size_t i = 0; i + slen <= w.thread_start_4kb.size(); ++i) {
      if (std::equal(s.begin(), s.end(),
                     w.thread_start_4kb.begin() +
                         static_cast<std::ptrdiff_t>(i))) {
        ++r.patterns_found;
        r.detail += std::string("pattern[") + std::string(s) + "] ";
        break;
      }
    }
  }
  if (r.patterns_found > 0) ++signals;

  std::printf("[blue:thread_4kb_signature] sensor 3: return-address chain residual (LESSON)\n");
  int suspicious_frames = 0;
  for (const auto addr : w.thread_return_chain) {
    bool in_mod = false;
    for (const auto& [pid, proc] : w.processes) {
      (void)pid;
      for (const auto& mod : proc.modules) {
        if (addr >= mod.base && addr < mod.base + mod.size) {
          in_mod = true;
          break;
        }
      }
      if (in_mod) break;
    }
    if (!in_mod) ++suspicious_frames;
  }
  const bool chain_hit = !w.return_address_chain_clean || suspicious_frames > 0;
  if (chain_hit) {
    r.detail += "return_chain_residual ";
    r.detail += "suspicious_frames=" + std::to_string(suspicious_frames) + " ";
    ++signals;
  }

  std::printf("[blue:thread_4kb_signature] sensor 4: foreign VM_READ co-occurrence\n");
  bool foreign_handle = false;
  for (const auto& h : w.handles_to(w.game_pid(), true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      foreign_handle = true;
      break;
    }
  }
  if (foreign_handle) {
    r.detail += "foreign_VM_READ ";
    ++signals;
  }

  // Multi-reason: require ≥2 independent sensors (clean 4KB alone is insufficient).
  r.detected = signals >= 2;
  if (r.detail.empty()) r.detail = "no_thread_signature_signals";

  std::printf("[blue:thread_4kb_signature] patterns=%d rwx=%d chain=%d handle=%d signals=%d detected=%d\n",
              r.patterns_found, r.rwx_detected ? 1 : 0, chain_hit ? 1 : 0,
              foreign_handle ? 1 : 0, signals, r.detected ? 1 : 0);
  return r;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  BlueResult r = detect(w);
  if (r.detected) {
    w.ranked_access_denied = true;
    w.note("thread_4kb_signature: BLUE mitigated — thread flagged for server-side review");
  }
  std::printf("[blue:thread_4kb_signature] mitigate detected=%d\n", r.detected ? 1 : 0);
  return r;
}

}  // namespace strategy::t1_thread_4kb_signature
