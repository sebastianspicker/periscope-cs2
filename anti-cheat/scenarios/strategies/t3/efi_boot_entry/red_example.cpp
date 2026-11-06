#include "red_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::efi_boot_entry {

RedResult apply(sim::World& w) {
  RedResult r{false, 0, ""};
  const auto game = w.game_pid();
  std::printf("[red:efi_boot_entry] precondition: validate game world\n");
  if (game == 0 || w.proc(game) == nullptr) {
    return {false, r.steps, "game process is unavailable"};
  }
  ++r.steps;
  std::printf("[red:efi_boot_entry] step %d: establish lab actor\n", r.steps);
  const auto actor = w.spawn("efi_boot_entry-lab-actor.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    return {false, r.steps, "lab actor creation failed"};
  }
  ++r.steps;
  std::printf("[red:efi_boot_entry] step %d: apply isolated strategy residual\n", r.steps);
  w.trust.unexpected_efi_entry = true; w.trust.efi_entry_name = "LabRecovery";
  if (!(w.trust.unexpected_efi_entry)) {
    return {false, r.steps, "strategy residual was not recorded"};
  }
  ++r.steps;
  std::printf("[red:efi_boot_entry] step %d: validate recorded world state\n", r.steps);
  if (!(!w.trust.efi_entry_name.empty())) {
    return {false, r.steps, "world-state verification failed"};
  }
  ++r.steps;
  r.achieved = true;
  r.detail = "efi_boot_entry completed " + std::to_string(r.steps) + " lab-only steps";
  w.note(r.detail);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "efi_boot_entry", "Executing lab-only multi-step simulation.");
  auto result = apply(w);
  n.say(sim::Side::Red, result.detail);
  return result;
}

}  // namespace examples::efi_boot_entry
