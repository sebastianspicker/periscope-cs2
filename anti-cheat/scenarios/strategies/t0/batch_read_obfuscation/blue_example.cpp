#include "blue_example.hpp"
#include <cstdio>

namespace examples::batch_read_obfuscation {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.batch_read_obfuscated) {
    result.reasons.emplace_back("batch read obfuscation active");
    result.scatter_detected = true;
  }
  if (w.batch_read_shuffled) result.reasons.emplace_back("batch reads Fisher-Yates shuffled");
  if (w.batch_read_jittered) result.reasons.emplace_back("batch reads timing jittered");
  if (w.batch_read_count > 0) result.reasons.emplace_back("batch read count=" + std::to_string(w.batch_read_count));
  if (w.scattered_read_pattern) result.reasons.emplace_back("scattered read pattern active");
  if (w.read_timing_jitter) result.reasons.emplace_back("read timing jitter active");
  if (w.remote_read_ops > 0) result.reasons.emplace_back("remote read ops=" + std::to_string(w.remote_read_ops));

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.scatter_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 batch_read_obfuscation] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::batch_read_obfuscation
