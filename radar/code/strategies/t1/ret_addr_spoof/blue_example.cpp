#include "blue_example.hpp"

#include <cstdio>

namespace strategy::t1_ret_addr_spoof {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult r;
  r.chain_frames_checked = static_cast<int>(w.thread_return_chain.size());

  std::printf("[blue:ret_addr_spoof] sensor 1: walk return address chain\n");
  for (std::size_t i = 0; i < w.thread_return_chain.size(); ++i) {
    const std::uint64_t addr = w.thread_return_chain[i];
    bool in_known_module = false;
    for (const auto& [pid, proc] : w.processes) {
      (void)pid;
      for (const auto& mod : proc.modules) {
        if (addr >= mod.base && addr < mod.base + mod.size) {
          in_known_module = true;
          break;
        }
      }
      if (in_known_module) break;
    }
    if (!in_known_module) {
      ++r.suspicious_frames;
      r.detail += "frame[" + std::to_string(i) + "]=" + std::to_string(addr) +
                  "(RWX/no_module) ";
    }
  }
  if (r.suspicious_frames > 0) {
    r.reasons.emplace_back("suspicious_return_frames");
  }

  std::printf("[blue:ret_addr_spoof] sensor 2: start vs chain consistency\n");
  if (w.thread_start_clean && !w.return_address_chain_clean) {
    r.reasons.emplace_back("start_clean_but_chain_dirty");
    r.detail += "start_clean_but_chain_dirty ";
  }
  if (!w.return_address_chain_clean) {
    r.reasons.emplace_back("return_chain_not_clean");
    r.detail += "return_chain_not_clean ";
  }

  std::printf("[blue:ret_addr_spoof] sensor 3: foreign handle co-occurrence\n");
  for (const auto& h : w.handles_to(w.game_pid(), true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      r.reasons.emplace_back("foreign_VM_READ");
      r.detail += "foreign_VM_READ ";
      break;
    }
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2;
  std::printf("[blue:ret_addr_spoof] frames=%d suspicious=%d signals=%d detected=%s\n",
              r.chain_frames_checked, r.suspicious_frames, r.signals,
              r.detected ? "true" : "false");
  return r;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  BlueResult r = detect(w);
  if (r.detected) {
    w.ranked_access_denied = true;
    w.thread_return_chain.clear();
    w.thread_start_clean = false;
    w.return_address_chain_clean = false;
    r.mitigated = true;
    w.note("ret_addr_spoof: BLUE mitigated - thread flagged, suspicious frames logged");
  }
  std::printf("[blue:ret_addr_spoof] mitigate detected=%d\n", r.detected ? 1 : 0);
  return r;
}

}  // namespace strategy::t1_ret_addr_spoof
