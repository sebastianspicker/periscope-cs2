#include "blue_example.hpp"
#include "sim/strategy_example.hpp"

#include <cstdio>

namespace examples::efi_boot_entry {

BlueResult detect(sim::World& w) {
  sim::strategy_example::BlueOutcome outcome;
  std::printf("[blue:efi_boot_entry] signal 1: validate game inspection context\n");
  sim::strategy_example::add_common_signals(w, outcome);
  std::printf("[blue:efi_boot_entry] signal 2: evaluate strategy-specific posture\n");
  if (w.trust.unexpected_efi_entry) {
    sim::strategy_example::add_signal(outcome, "unexpected EFI boot entry is present", 0.45);
  }
  std::printf("[blue:efi_boot_entry] signal 3: corroborate independent residual\n");
  if (!w.trust.efi_entry_name.empty()) {
    sim::strategy_example::add_signal(outcome, "EFI entry has a non-baseline name", 0.35);
  }
  outcome = sim::strategy_example::finish_blue("efi_boot_entry", std::move(outcome));
  BlueResult r{false, 0, {}, 0.0, false, ""};
  r.detected = outcome.detected;
  r.signals = outcome.signals;
  r.reasons = std::move(outcome.reasons);
  r.risk = outcome.risk;
  r.mitigated = r.risk >= 0.65;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "efi_boot_entry blue signals=" + std::to_string(r.signals) + " risk=" + std::to_string(r.risk);
  w.note(r.detail);
  return r;
}

BlueResult run_blue(sim::World& w, sim::Narrator& n) {
  n.counter(sim::Side::Blue, "efi_boot_entry", "Correlating independent lab signals.");
  auto result = detect(w);
  n.say(sim::Side::Blue, result.detail);
  return result;
}

}  // namespace examples::efi_boot_entry
