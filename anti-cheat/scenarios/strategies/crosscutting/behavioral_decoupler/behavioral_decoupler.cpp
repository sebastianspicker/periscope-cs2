#include "behavioral_decoupler.hpp"

#include <cmath>
#include <cstring>
#include <sstream>

namespace features {

BehavioralDecoupler::BehavioralDecoupler(sim::World& world) : world_(world) {
  std::random_device rd;
  rng_.seed(rd());
}

void BehavioralDecoupler::set_config(const DecouplerConfig& cfg) {
  config_ = cfg;
}

void BehavioralDecoupler::reset() {
  report_ = BehavioralReport{};
  blind_spots_.clear();
}

ac::EntitySnapshot BehavioralDecoupler::process(
    const ac::EntitySnapshot& raw, std::uint64_t current_tick) {
  report_.total_frames++;
  if (should_skip_frame()) {
    report_.frames_skipped++;
    return raw;
  }
  if (should_skip_entity(raw.id)) {
    report_.entities_omitted++;
    return raw;
  }

  // Blind spot check
  for (auto it = blind_spots_.begin(); it != blind_spots_.end();) {
    if (it->expire_tick <= current_tick) {
      it = blind_spots_.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto& bs : blind_spots_) {
    if (bs.entity_id == raw.id) {
      return raw;  // Hidden by blind spot
    }
  }
  if (config_.blind_spots_enabled && random_double() < 0.03) {
    blind_spots_.push_back(
        {raw.id, current_tick + static_cast<std::uint64_t>(random_int(
                                  config_.blind_spot_min_ticks,
                                  config_.blind_spot_max_ticks))});
  }

  ac::EntitySnapshot result = raw;
  result.origin = fuzz_position(raw.origin);
  if (std::memcmp(&result.origin, &raw.origin, sizeof(ac::Vec3)) != 0) {
    report_.entities_fuzzed++;
  }

  update_vacnet_risk_score();
  return result;
}

std::vector<ac::EntitySnapshot> BehavioralDecoupler::process_batch(
    const std::vector<ac::EntitySnapshot>& raw, std::uint64_t current_tick) {
  std::vector<ac::EntitySnapshot> result;
  for (const auto& entity : raw) {
    auto processed = process(entity, current_tick);
    result.push_back(processed);
  }
  return result;
}

int BehavioralDecoupler::humanize_counter_strafe(int raw_delta) {
  if (config_.counter_strafe_jitter <= 0.0) {
    return raw_delta;
  }
  double jitter = 1.0 + (random_double() * 2.0 - 1.0) *
                            config_.counter_strafe_jitter;
  int jittered =
      static_cast<int>(std::round(static_cast<double>(raw_delta) * jitter));
  return (jittered < 1) ? 1 : jittered;
}

double BehavioralDecoupler::random_double() {
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  return dist(rng_);
}

int BehavioralDecoupler::random_int(int min, int max) {
  if (min >= max) {
    return min;
  }
  std::uniform_int_distribution<int> dist(min, max);
  return dist(rng_);
}

bool BehavioralDecoupler::should_skip_frame() {
  return config_.frame_skip_rate > 0.0 &&
         random_double() < config_.frame_skip_rate;
}

bool BehavioralDecoupler::should_skip_entity(std::uint32_t entity_id) {
  (void)entity_id;
  return config_.entity_skip_rate > 0.0 &&
         random_double() < config_.entity_skip_rate;
}

ac::Vec3 BehavioralDecoupler::fuzz_position(const ac::Vec3& pos) {
  if (config_.position_noise_px <= 0.0) {
    return pos;
  }
  ac::Vec3 fuzzed = pos;
  auto rng_float = [&](double range) -> float {
    return static_cast<float>((random_double() * 2.0 - 1.0) * range);
  };
  fuzzed.x += rng_float(config_.position_noise_px);
  fuzzed.y += rng_float(config_.position_noise_px);
  return fuzzed;
}

void BehavioralDecoupler::update_vacnet_risk_score() {
  double risk = 0.2;
  if (report_.total_frames > 0) {
    double skip_rate = static_cast<double>(report_.frames_skipped) /
                       static_cast<double>(report_.total_frames);
    if (skip_rate < config_.frame_skip_rate * 0.5) {
      risk += 0.3;
    }
  }
  if (report_.total_frames > 0 && report_.total_frames < 1000) {
    double omit_rate = static_cast<double>(report_.entities_omitted) /
                       static_cast<double>(report_.total_frames);
    if (omit_rate < config_.entity_skip_rate * 0.5) {
      risk += 0.3;
    }
  }
  report_.vacnet_risk_score = std::min(risk, 1.0);
}

}  // namespace features
