#include "blue_example.hpp"

#include <string>

namespace examples::obfuscation_crypto {

BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  if (w.entity_stream_encrypted && w.client_has_stream_key) ++reasons;
  for (const auto& section : w.sections) {
    if (section.name.find("encrypted-offset") != std::string::npos) ++reasons;
  }

  r.detected = reasons >= 2;
  r.mitigated = reasons >= 2;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "crypto telemetry reasons=" + std::to_string(reasons) +
             "; weak static-key obfuscation remains attributable";
  return r;
}

}  // namespace examples::obfuscation_crypto
