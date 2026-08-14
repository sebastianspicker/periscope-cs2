#include "blue_example.hpp"

#include <cstdio>

namespace strategy::t2_module_list_hide {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult result;
  if (w.game_pid() == 0) return result;
  const auto* g = w.proc(w.game_pid());

  std::printf("[blue:module_list_hide] sensor 1: module shadowing / Ldr unlink scar\n");
  if (w.module_shadowing_active) {
    result.discrepancy_detected = true;
    result.reasons.emplace_back("module_shadowing_active (Ldr unlink scar)");
  }

  std::printf("[blue:module_list_hide] sensor 2: PE header / unlinked mapping residual\n");
  if (w.module_loaded && !w.module_is_signed) {
    result.pe_header_scan_detected = true;
    result.reasons.emplace_back("unsigned_module_residual_present");
  }
  int unlinked = 0;
  if (g != nullptr) {
    for (const auto& m : g->modules) {
      if (!m.linked_in_peb) ++unlinked;
    }
    if (g->manual_mapped_region) {
      result.reasons.emplace_back("manual_mapped_region residual");
    }
  }
  if (unlinked > 0) {
    result.reasons.emplace_back("unlinked_PEB_modules=" + std::to_string(unlinked));
  }

  std::printf("[blue:module_list_hide] sensor 3: foreign thread origin\n");
  if (g != nullptr && g->has_foreign_thread) {
    result.reasons.emplace_back("foreign_thread_origin");
  }
  if (w.module_loaded) {
    result.reasons.emplace_back("module_loaded surface");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detection_count = result.signals;
  result.detected = result.signals >= 2 &&
                    (result.discrepancy_detected || result.pe_header_scan_detected ||
                     unlinked > 0);
  if (result.reasons.empty()) {
    result.detail = "No hidden-module indicators";
  } else {
    result.detail = "signals=" + std::to_string(result.signals);
    for (const auto& r : result.reasons) result.detail += "; " + r;
  }
  std::printf("[blue:module_list_hide] signals=%d detected=%d | %s\n", result.signals,
              result.detected ? 1 : 0, result.detail.c_str());
  return result;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  auto result = detect(w);
  if (result.detected) {
    w.note("Blue: flagged hidden module (PE header present but Ldr entry missing)");
    w.note("flag:module_hide_violation");
    w.ranked_access_denied = true;
    w.module_shadowing_active = false;
    result.mitigated = true;
  }
  return result;
}

}  // namespace strategy::t2_module_list_hide
