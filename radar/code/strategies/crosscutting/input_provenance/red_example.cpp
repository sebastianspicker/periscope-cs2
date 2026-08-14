#include "red_example.hpp"
namespace examples::input_provenance {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("input-bridge.exe");
  w.push_input(sim::InputEvent{0.1, "injected", 12.f, 3.f});
  w.push_input(sim::InputEvent{0.2, "raw_hid", 2.f, 1.f});
  w.push_input(sim::InputEvent{0.3, "injected", 15.f, -4.f});
  w.push_input(sim::InputEvent{0.4, "serial_arduino", 8.f, 2.f});
  w.raw_sendinput_mixed = true;
  for (const auto& e : w.inputs) if (e.source == "injected" || e.source.find("serial")==0 || e.source.find("kmbox")==0) ++r.injected;
  r.achieved = r.injected >= 2 && w.raw_sendinput_mixed;
  r.detail = "input_provenance red injectedish=" + std::to_string(r.injected) + " mixed=1";
  w.note(r.detail);
  return r;
}
}  // namespace examples::input_provenance
