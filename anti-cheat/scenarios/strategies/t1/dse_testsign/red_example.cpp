// RED example implementation for this strategy pair.
// Multi-step World scars for learners; pairs with blue_example detect/mitigate.

#include "red_example.hpp"

#include <cstdio>

namespace examples::dse_testsign {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  std::printf("[red:dse] verify a game is present\n");
  if (game == 0 || w.proc(game) == nullptr) { r.detail = "precondition failed: game missing"; return r; }
  ++r.steps;
  std::printf("[red:dse] enable the simulated test-signing state\n");
  w.trust.test_signing = true;
  w.trust.dse_enforced = false;
  if (!w.trust.test_signing || w.trust.dse_enforced) { r.detail = "could not change signing state"; return r; }
  ++r.steps;
  std::printf("[red:dse] register a lab-only test-signed memory driver\n");
  w.load_driver(sim::Driver{"lab-testsign.sys", "test-signing-demo", "Test Lab", false, false, false, true, true});
  if (w.drivers.empty() || !w.drivers.back().provides_mem_rw) { r.detail = "driver registration failed"; return r; }
  ++r.steps;
  std::printf("[red:dse] expose its lab device and verify the artifact\n");
  w.create_device(sim::Device{"\\\\.\\LabTestSign", "lab-testsign.sys", true});
  if (w.devices.empty() || !w.devices.back().mem_rw_ioctl) { r.detail = "device creation failed"; return r; }
  ++r.steps;
  r.achieved = true;
  r.detail = "test-signing disabled DSE and admitted a memory-capable lab driver";
  w.note(r.detail);
  return r;
}

}  // namespace examples::dse_testsign
