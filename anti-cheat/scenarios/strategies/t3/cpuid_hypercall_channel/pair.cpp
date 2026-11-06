#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T3 delivery pair. Red discovers a magic CPUID leaf exposed by the platform
// hypervisor and reads game data through unauthenticated hypercalls; blue
// scores leaf-probing telemetry, the covert-channel scar, and the
// unauthenticated-channel abuse, then requires channel authentication.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  // Lab precondition: platform HV with an exposed hypercall interface, VBS off.
  w.trust.platform_hv_active = true;
  w.trust.platform_hv_vendor = "Microsoft Hv";
  w.trust.vbs = false;
  const auto red = examples::cpuid_hypercall_channel::apply(w);
  const auto blue = examples::cpuid_hypercall_channel::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_155_cpuid_hypercall_channel() {
  return {{"155_cpuid_hypercall_channel", "cpuid hypercall covert channel", Family::Delivery,
           "T3",
           "Red probes a magic CPUID leaf and reads entities via unauthenticated hypercalls",
           "Blue detects leaf probing, channel abuse, and requires authentication"},
          run};
}
}  // namespace strategies
