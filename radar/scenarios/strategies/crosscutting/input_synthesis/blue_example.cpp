#include "blue_example.hpp"

#include <cstdio>

namespace examples::input_synthesis {

BlueResult detect(sim::World& w) {
  BlueResult r;

  int arduino = 0;
  int kmbox = 0;
  int injected = 0;
  for (const auto& e : w.inputs) {
    if (e.source.find("arduino") != std::string::npos) ++arduino;
    else if (e.source.find("kmbox") != std::string::npos) ++kmbox;
    else if (e.source == "injected") ++injected;
  }

  if (arduino > 0) {
    r.reasons.emplace_back("serial_arduino_source count=" + std::to_string(arduino));
  }
  if (kmbox > 0) {
    r.reasons.emplace_back("kmbox_source count=" + std::to_string(kmbox));
  }
  if (injected > 0) {
    r.reasons.emplace_back("injected_source count=" + std::to_string(injected));
  }
  if (w.raw_sendinput_mixed) {
    r.reasons.emplace_back("raw_sendinput_mixed residual");
  }
  if (arduino > 0 && kmbox > 0) {
    r.reasons.emplace_back("multi_mcu_provenance mix");
  }

  const int bad = arduino + kmbox + injected;
  const bool specific_scar = bad >= 2 || (bad >= 1 && w.raw_sendinput_mixed);
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: synthetic HID provenance");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2 && specific_scar;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.ranked_access_denied = true;
    w.raw_sendinput_mixed = false;
  }

  r.detail = "input_synthesis blue bad_sources=" + std::to_string(bad) +
             " signals=" + std::to_string(r.signals) +
             " detected=" + std::to_string(r.detected ? 1 : 0);
  w.note(r.detail);
  std::printf("[input_synthesis] BLUE signals=%d detected=%s\n", r.signals,
              r.detected ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[input_synthesis]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::input_synthesis
