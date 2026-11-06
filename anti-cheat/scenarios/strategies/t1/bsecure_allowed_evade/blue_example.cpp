#include "blue_example.hpp"

#include <cstdio>

namespace strategy::t1_bsecure_allowed_evade {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult r;

  std::printf("[blue:bsecure_allowed_evade] sensor 1: module stomp residual\n");
  if (w.module_is_stomped) {
    r.stomp_detected = true;
    r.reasons.emplace_back("module_stomped");
    r.detail += "module_stomped ";
    ++r.detection_count;
  }

  std::printf("[blue:bsecure_allowed_evade] sensor 2: signature inventory\n");
  if (w.module_is_signed) {
    r.signature_valid = true;
  } else {
    r.reasons.emplace_back("signature_invalid");
    r.detail += "signature_invalid ";
    ++r.detection_count;
  }

  std::printf("[blue:bsecure_allowed_evade] sensor 3: PE timestamp vs signature date\n");
  if (w.module_signature_date != w.module_timestamp) {
    r.timestamp_mismatch = true;
    r.reasons.emplace_back("timestamp_mismatch");
    r.detail += "timestamp_mismatch ";
    ++r.detection_count;
  }

  std::printf("[blue:bsecure_allowed_evade] sensor 4: size / integrity baseline\n");
  if (w.module_size != w.expected_module_size) {
    r.reasons.emplace_back("size_mismatch");
    r.detail += "size_mismatch ";
    ++r.detection_count;
  }
  if (const auto* g = w.proc(w.game_pid())) {
    for (const auto& m : g->modules) {
      if (m.text_hash == "stomped" || m.headers_erased) {
        r.reasons.emplace_back("host_text_integrity_dirty");
        r.detail += "host_text_integrity_dirty ";
        ++r.detection_count;
        break;
      }
    }
  }

  r.signals = r.detection_count;
  // Multi-reason: stomp-specific scar + at least one more independent check.
  r.detected = r.stomp_detected && r.signals >= 2;
  if (r.detection_count == 0) r.detail = "all_checks_passed";
  std::printf("[blue:bsecure_allowed_evade] count=%d detected=%d | %s\n",
              r.detection_count, r.detected ? 1 : 0, r.detail.c_str());
  return r;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  BlueResult r = detect(w);
  if (r.detected) {
    w.ranked_access_denied = true;
    w.module_loaded = false;
    w.module_is_stomped = false;
    r.mitigated = true;
    w.note("bsecure_allowed_evade: BLUE mitigated - stomp flagged, access denied");
  }
  std::printf("[blue:bsecure_allowed_evade] mitigate detection_count=%d\n",
              r.detection_count);
  return r;
}

}  // namespace strategy::t1_bsecure_allowed_evade
