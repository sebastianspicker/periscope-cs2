#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `kernel_ioctl`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::kernel_ioctl::run_red(w, n);
  const auto blue = examples::kernel_ioctl::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_03_kernel_ioctl() {
  return {{ "03_kernel_ioctl", "kernel ioctl", Family::Delivery, "T2",
           "Red multi-step lab path for kernel_ioctl",
           "Blue multi-reason lab path for kernel_ioctl"},
          run};
}
}  // namespace strategies
