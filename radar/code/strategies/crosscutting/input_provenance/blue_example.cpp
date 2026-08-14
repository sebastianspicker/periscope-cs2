#include "blue_example.hpp"
namespace examples::input_provenance {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int inj = 0, raw = 0;
  for (const auto& e : w.inputs) {
    if (e.source == "injected" || e.source.find("arduino")!=std::string::npos || e.source.find("kmbox")!=std::string::npos) ++inj;
    if (e.source == "raw_hid") ++raw;
  }
  r.detected = inj >= 2 && (raw >= 1 || w.raw_sendinput_mixed);
  if (r.detected) { r.mitigated = true; w.overwatch_queued = true; }
  r.detail = "input_provenance blue inj=" + std::to_string(inj) + " raw=" + std::to_string(raw) + " mixed=" + std::to_string(w.raw_sendinput_mixed?1:0);
  w.note(r.detail);
  return r;
}
}  // namespace examples::input_provenance
