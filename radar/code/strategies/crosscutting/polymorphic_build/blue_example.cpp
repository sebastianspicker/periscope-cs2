#include "blue_example.hpp"

#include <sstream>
#include <string>

namespace examples::polymorphic_build {

BlueResult detect(sim::World& w) {
  BlueResult r;

  // Reason 1: non-shared per-buyer binary build id.
  if (w.binary_build_id != "shared") {
    r.reasons.emplace_back("binary_build_id not shared baseline");
  }

  // Reason 2: layout marker in module text hash.
  for (const auto& [pid, process] : w.processes) {
    (void)pid;
    for (const auto& module : process.modules) {
      if (module.text_hash.find("layout-") != std::string::npos) {
        r.reasons.emplace_back("module text_hash layout marker");
        break;
      }
    }
  }

  // Reason 3: distribution watermark present.
  if (!w.build_watermark.empty() &&
      w.build_watermark.find("poly:") != std::string::npos) {
    r.reasons.emplace_back("polymorphic build_watermark cohort");
  }

  // Reason 4: shared section carrying layout residual.
  for (const auto& section : w.sections) {
    if (section.name.find("layout-") != std::string::npos ||
        section.name.find("poly.") != std::string::npos) {
      r.reasons.emplace_back("layout shared-section residual");
      break;
    }
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2;
  r.mitigated = r.signals >= 2;
  if (r.mitigated) w.ranked_access_denied = true;

  std::ostringstream oss;
  oss << "polymorphic_build blue signals=" << r.signals
      << " detected=" << (r.detected ? 1 : 0)
      << "; behavior is stronger than an unstable file hash";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::polymorphic_build
