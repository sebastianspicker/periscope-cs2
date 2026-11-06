// RED example implementation for strategy `physmem_map`.
// Multi-step World scars: physmem open residual + physmap-lab.sys + PhysicalMemoryLab device.

#include "red_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::physmem_map {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "physical map precondition failed: game unavailable";

  std::printf("[red:physmem_map] require a game process present\n");
  if (!w.game_pid()) {
    return r;
  }

  std::printf("[red:physmem_map] open the physical-memory mapping residual\n");
  w.physmem_device_open = true;
  if (!w.physmem_device_open) {
    r.detail = "failed to set physmem_device_open";
    return r;
  }
  ++r.steps;

  std::printf("[red:physmem_map] load physmap-lab.sys mapping driver\n");
  w.load_driver({"physmap-lab.sys", "physmap-sha", "LabVendor", false, false,
                 false, false, true});
  if (!support::has_driver(w, "physmap-lab.sys")) {
    r.detail = "physmap-lab.sys registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:physmem_map] create PhysicalMemoryLab device and verify\n");
  w.create_device({"\\\\.\\PhysicalMemoryLab", "physmap-lab.sys", true});
  if (!w.physmem_device_open ||
      !support::has_device(w, "\\\\.\\PhysicalMemoryLab")) {
    r.detail = "physical-memory mapping residual verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "physical-memory mapping surface and device verified";
  w.note(r.detail);
  std::printf("[red:physmem_map] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "physmem_map", "Open a physical mapping surface.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::physmem_map
