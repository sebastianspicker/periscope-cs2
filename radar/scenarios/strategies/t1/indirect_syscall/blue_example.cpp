// BLUE example implementation for this strategy pair.
// Multi-reason sensors on World scars; educational detect/mitigate path.

#include "blue_example.hpp"

#include "ac/telemetry.hpp"
#include "t1_blue/syscall_aware_monitor.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace examples::indirect_syscall {

BlueResult detect(sim::World& w) {
  ac::MemoryTelemetrySink sink;
  t1_blue::T1Agent agent(w, sink);
  auto d = agent.full_scan();

  BlueResult r;
  r.reasons = d.reasons;

  // Independent reasons beyond agent summary — not single-flag echo.
  if (d.syscall_handles > 0) {
    r.reasons.emplace_back("syscall-path handles=" + std::to_string(d.syscall_handles));
  }
  if (d.handle_truth_hit) {
    r.reasons.emplace_back("handle-table truth residual on game target");
  }
  if (d.hooks_blind) {
    r.reasons.emplace_back("usermode ntdll hooks blind while handle truth fires");
  }
  if (d.foreign_vm_read > 0) {
    r.reasons.emplace_back("foreign VM_READ count=" + std::to_string(d.foreign_vm_read));
  }
  if (w.remote_read_ops > 0) {
    r.reasons.emplace_back("remote read ops=" + std::to_string(w.remote_read_ops));
  }

  // Unique-ish collapse is unnecessary; signals = reasons count.
  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::max(d.risk, std::min(1.0, r.signals * 0.22));
  r.detected = r.signals >= 2 && (d.handle_truth_hit || d.syscall_handles > 0);
  r.mitigated = r.detected && (d.hooks_blind || r.signals >= 3);
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = d.summary.empty()
                 ? ("signals=" + std::to_string(r.signals))
                 : d.summary + " signals=" + std::to_string(r.signals);

  std::printf(
      "[blue:indirect_syscall] detected=%d mitigated=%d handles=%d "
      "hooks_blind=%d signals=%d risk=%.2f\n",
      static_cast<int>(r.detected), static_cast<int>(r.mitigated),
      d.syscall_handles, static_cast<int>(d.hooks_blind), r.signals, r.risk);
  for (const auto& reason : r.reasons) {
    std::printf("[blue:indirect_syscall]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::indirect_syscall
