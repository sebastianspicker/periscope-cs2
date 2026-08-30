// BLUE example implementation for strategy `kernel_ioctl`.
// Multi-reason sensors: LabMemRw device, untrusted mem-rw provider, remote read ops.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::kernel_ioctl {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons,
                      support::has_device(w, "\\\\.\\LabMemRw"),
                      "LabMemRw device exposes memory IOCTL");
  support::add_signal(r.signals, r.reasons, support::has_untrusted_memrw_driver(w),
                      "non-AC kernel memory provider");
  support::add_signal(r.signals, r.reasons, w.remote_read_ops > 0,
                      "kernel-backed read activity");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "kernel_ioctl signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 kernel_ioctl] BLUE: %s\n", reason.c_str());
  }
  return r;
}

bool mitigate(sim::World& w) {
  if (!detect(w).detected) {
    return false;
  }
  w.ranked_access_denied = true;
  return true;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "kernel_ioctl",
            "Inspect exposed IOCTL memory paths.");
  auto r = detect(w);
  std::printf("[blue/kernel_ioctl] signals=%d risk=%.2f\n", r.signals, r.risk);
  return r;
}

}  // namespace examples::kernel_ioctl
