// RED example implementation for strategy `early_load_race`.
// Multi-step World scars: boot-start driver ordered before the anti-cheat load order.

#include "red_example.hpp"

#include <cstdio>

namespace examples::early_load_race {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "early-load precondition failed: anti-cheat order unset";

  std::printf("[red:early_load_race] require a known anti-cheat driver load order\n");
  if (w.ac_driver_load_order <= 0) {
    return r;
  }

  std::printf("[red:early_load_race] register a boot-start driver before the AC order\n");
  const int early_order = w.ac_driver_load_order - 1;
  w.load_driver({"early-race.sys", "early-sha", "LabVendor", true, false, false,
                 false, true, early_order});
  if (w.drivers.empty() || w.drivers.back().name != "early-race.sys") {
    r.detail = "early-race.sys registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:early_load_race] verify boot_start and earlier load_order\n");
  const auto& d = w.drivers.back();
  if (!d.boot_start) {
    r.detail = "driver is not boot-start";
    return r;
  }
  ++r.steps;

  if (d.load_order >= w.ac_driver_load_order) {
    r.detail = "driver load_order is not before anti-cheat";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "boot-start driver ordered before anti-cheat verified";
  w.note(r.detail);
  std::printf("[red:early_load_race] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "early_load_race", "Register an earlier boot driver.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::early_load_race
