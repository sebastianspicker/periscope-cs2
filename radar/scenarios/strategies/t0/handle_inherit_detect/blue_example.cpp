#include "blue_example.hpp"

#include <cstdio>

namespace strategy::t0_handle_inherit_detect {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult result;
  const uint32_t game_pid = w.game_pid();
  if (game_pid == 0) return result;

  std::printf("[blue:handle_inherit_detect] sensor 1: proxy/inherited VM_READ on game\n");
  for (const auto& handle : w.handles) {
    if (handle.target_pid == game_pid && handle.via_proxy &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      result.inheritance_chain_detected = true;
      result.child_pid = handle.owner_pid;
      result.reasons.emplace_back("proxy/inherited VM_READ owner=" +
                                  std::to_string(handle.owner_pid));
      break;
    }
  }

  std::printf("[blue:handle_inherit_detect] sensor 2: parent also holds VM_READ\n");
  bool parent_cohold = false;
  if (result.inheritance_chain_detected) {
    const auto* child = w.proc(result.child_pid);
    if (child != nullptr && child->parent_pid != 0) {
      for (const auto& h : w.handles_to(game_pid, true)) {
        if (h.owner_pid == child->parent_pid &&
            sim::has(h.access, sim::AccessMask::VmRead)) {
          parent_cohold = true;
          result.reasons.emplace_back("parent co-holds VM_READ parent=" +
                                      std::to_string(child->parent_pid));
          break;
        }
      }
    }
  }

  std::printf("[blue:handle_inherit_detect] sensor 3: parent-child lineage join\n");
  if (result.inheritance_chain_detected) {
    const auto* child = w.proc(result.child_pid);
    if (child != nullptr && child->parent_pid != 0 && w.proc(child->parent_pid)) {
      result.reasons.emplace_back("lineage_parent=" +
                                  std::to_string(child->parent_pid));
    }
  }

  std::printf("[blue:handle_inherit_detect] sensor 4: remote-read co-occurrence\n");
  if (w.remote_read_ops > 0 || w.remote_read_bytes > 0) {
    result.reasons.emplace_back("remote read telemetry ops=" +
                                std::to_string(w.remote_read_ops));
  }

  result.signals = static_cast<int>(result.reasons.size());
  // Multi-reason: proxy scar plus at least one independent corroborating signal.
  result.detected = result.inheritance_chain_detected && result.signals >= 2;

  if (result.reasons.empty()) {
    result.detail = "No handle inheritance chains detected";
  } else {
    result.detail = "signals=" + std::to_string(result.signals) +
                    " parent_cohold=" + (parent_cohold ? "1" : "0");
    for (const auto& r : result.reasons) result.detail += "; " + r;
  }
  std::printf("[blue:handle_inherit_detect] signals=%d detected=%d child=%u | %s\n",
              result.signals, result.detected ? 1 : 0, result.child_pid,
              result.detail.c_str());
  return result;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  auto result = detect(w);
  if (result.detected) {
    for (auto& handle : w.handles) {
      if (handle.target_pid == w.game_pid() && handle.via_proxy) {
        handle.via_proxy = false;
        handle.access = sim::AccessMask::None;
      }
    }
    w.ranked_access_denied = true;
    w.note("Blue: mitigated handle inheritance chain for PID " +
           std::to_string(result.child_pid));
    result.mitigated = true;
    result.detail = "Mitigated: inherited handle chain broken";
  }
  return result;
}

}  // namespace strategy::t0_handle_inherit_detect
