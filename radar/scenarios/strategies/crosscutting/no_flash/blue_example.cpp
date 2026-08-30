// BLUE: multi-reason flash strip residual (FX + inject helper).

#include "blue_example.hpp"

#include <cstdio>
#include <sstream>

namespace examples::no_flash {

BlueResult detect(sim::World& w) {
  BlueResult r;
  r.fx_strip = w.no_flash_active && w.flash_alpha_forced < 0.1f;

  bool helper = false;
  if (const auto* g = w.proc(w.game_pid())) {
    for (const auto& m : g->modules) {
      if (m.name.find("noflash") != std::string::npos || m.text_hash == "patched") {
        helper = true;
        break;
      }
    }
  }

  if (r.fx_strip) r.reasons.emplace_back("flash_alpha_forced_to_zero");
  if (helper) r.reasons.emplace_back("noflash_helper_module_in_game");
  if (w.no_flash_active) r.reasons.emplace_back("no_flash_session_flag");
  if (w.flash_alpha_forced < 0.1f) {
    r.reasons.emplace_back("flash_alpha=" + std::to_string(w.flash_alpha_forced));
  }

  const bool specific_scar = r.fx_strip;
  if (specific_scar) {
    r.reasons.emplace_back("strategy scar: client flash FX strip");
  }

  r.signals = static_cast<int>(r.reasons.size());
  // Multi-reason score: need FX strip plus at least one inject residual.
  r.detected = r.signals >= 2 && specific_scar;
  r.mitigated = r.detected && helper;
  if (r.detected) {
    w.flash_alpha_forced = 1.f;
    w.no_flash_active = false;
  }
  if (r.mitigated) {
    w.ranked_access_denied = true;
  }

  std::ostringstream oss;
  oss << "no_flash blue fx_strip=" << (r.fx_strip ? 1 : 0)
      << " helper=" << (helper ? 1 : 0)
      << " signals=" << r.signals
      << " detected=" << (r.detected ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  std::printf("[no_flash] BLUE signals=%d detected=%s\n", r.signals,
              r.detected ? "true" : "false");
  for (const auto& reason : r.reasons) {
    std::printf("[no_flash]   %s\n", reason.c_str());
  }
  return r;
}

}  // namespace examples::no_flash
