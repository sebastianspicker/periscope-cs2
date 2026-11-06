// BLUE example implementation for strategy `physmem_map`.
// Multi-reason sensors: physmem open residual, mapping device, mapping driver.

#include "blue_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::physmem_map {

BlueResult detect(sim::World& w) {
  BlueResult r{};

  support::add_signal(r.signals, r.reasons, w.physmem_device_open,
                      "physical-memory mapping path opened");
  support::add_signal(r.signals, r.reasons,
                      support::has_device(w, "\\\\.\\PhysicalMemoryLab"),
                      "physical mapping device exposed");
  support::add_signal(r.signals, r.reasons,
                      support::has_driver(w, "physmap-lab.sys"),
                      "physical mapping driver loaded");

  r.detected = r.signals >= 2;
  r.risk = support::risk_for(r.signals);
  r.detail = "physmem_map signals=" + std::to_string(r.signals) +
             " risk=" + std::to_string(r.risk);

  for (const auto& reason : r.reasons) {
    std::printf("[T2 physmem_map] BLUE: %s\n", reason.c_str());
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
  n.counter(sim::Side::Blue, "physmem_map",
            "Inspect physical mapping capability.");
  return detect(w);
}

}  // namespace examples::physmem_map
