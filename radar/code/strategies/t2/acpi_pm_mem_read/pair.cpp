#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::acpi_pm_mem_read::apply(w);
  const auto blue = examples::acpi_pm_mem_read::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.75;
  result.summary = red.detail + " | blue=" +
                   std::to_string(blue.signals) + "sig risk=" +
                   std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_68_acpi_pm_mem_read() {
  return {{"68_acpi_pm_mem_read", "ACPI PM memory read", Family::Delivery,
           "T2", "Red reads memory via ACPI PM timer SMI, no driver or handle needed",
           "Blue detects via SMM residual + firmware monitoring (extremely hard)"},
          run};
}

}  // namespace strategies
