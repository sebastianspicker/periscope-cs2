#include "radar_pipeline.hpp"

#include "ac/types.hpp"

namespace radar {

bool FramePipeline::initialize(std::uint32_t, bool use_real_cs2) noexcept {
  // A simulation service cannot attach a real adapter. The flag is retained
  // for CLI compatibility and intentionally has no effect in this layer.
  (void)use_real_cs2;
  health_.initialize();
  gate_.initialize();
  sim::BehavioralFilterConfig config;
  config.applyDelay = false;
  behavioral_.initialize(config, 0xC0FFEEu);
  api_integrity_.set_known_good("simulation", 1, 1);
  api_integrity_ready_ = !api_integrity_.verify_all().empty();
  api_integrity_ok_ = api_integrity_ready_;
  initialized_ = true;
  return true;
}

bool FramePipeline::run_frame() noexcept {
  if (!initialized_) return false;
  ++frame_count_;
  ac::EntitySnapshot entities[4]{};
  entities[0] = ac::EntitySnapshot(1, {0.f, 0.f, 0.f}, 2, true);
  entities[0].is_local_player = true;
  entities[1] = ac::EntitySnapshot(2, {12.f, 4.f, 0.f}, 3, true);
  entities[2] = ac::EntitySnapshot(3, {40.f, -8.f, 0.f}, 3, true);
  entities[3] = ac::EntitySnapshot(4, {90.f, 20.f, 0.f}, 3, true);
  for (auto& entity : entities) entity.health = 100;
  behavioral_.filter_entities(entities, 4, {}, 0.f, false, 0.f, 0.f);
  last_entity_count_ = 4;
  last_filtered_entity_count_ = 0;
  for (const auto& entity : entities) {
    if (entity.alive && !entity.dormant) ++last_filtered_entity_count_;
  }
  health_.record_good_frame();
  health_.evaluate();
  gate_metrics_ = {};
  gate_metrics_.chunkCount = last_filtered_entity_count_ >= 2 ? 2 : 0;
  gate_metrics_.remoteEntityCount = last_filtered_entity_count_ > 0
      ? last_filtered_entity_count_ - 1 : 0;
  gate_metrics_.chunkRatio = static_cast<float>(last_filtered_entity_count_) / 4.f;
  gate_metrics_.zeroPushRatio = 0.f;
  gate_metrics_.heartbeatOk = api_integrity_ok_;
  gate_.evaluate(gate_metrics_);
  return true;
}

void FramePipeline::shutdown() noexcept { initialized_ = false; }

}  // namespace radar
