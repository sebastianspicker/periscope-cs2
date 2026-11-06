// intervals.hpp — R3/R7: every read/publish cadence constant in one place.
//
// Upstream lesson preserved (cs2-dma): cadence constants scattered across
// files drift. All intervals live here; consumers reference the constants,
// they never restate them.

#pragma once

#include <cstddef>
#include <cstdint>

namespace radar_domain {

// Camera worker (fast view-matrix reads) and data worker (entity reads).
inline constexpr std::uint32_t kCameraHz = 300;
inline constexpr std::uint32_t kDataHz = 60;

// Snapshot ring depth (cs2-dma uses 8; enough for a writer to lap a reader
// without unbounded retries).
inline constexpr std::size_t kSnapshotRingSlots = 8;

// Embedded web-radar publish clamps (cs2-dma `webradar.h`): a publish
// interval below the floor thrashes the serializer, one above the ceiling
// looks dead in a browser.
inline constexpr std::uint32_t kWebRadarMinIntervalMs = 17;
inline constexpr std::uint32_t kWebRadarMaxIntervalMs = 2000;
inline constexpr std::uint32_t kWebRadarFallbackIntervalMs = 1000;

// Last-known retention bridge for torn samples (R2).
inline constexpr std::uint64_t kLastKnownMaximumAgeMs = 150;

// Adaptive sampling by viewer demand (tiansongyu): 4 Hz with no viewers,
// 10 Hz background sharing, 20 Hz with active viewers.
inline constexpr std::uint32_t kAdaptiveNoViewerHz = 4;
inline constexpr std::uint32_t kAdaptiveBackgroundHz = 10;
inline constexpr std::uint32_t kAdaptiveActiveViewerHz = 20;

enum class ViewerDemand : std::uint8_t {
  None = 0,
  Background = 1,
  Active = 2,
};

[[nodiscard]] inline std::uint32_t adaptive_publish_hz(ViewerDemand demand) {
  switch (demand) {
    case ViewerDemand::None:
      return kAdaptiveNoViewerHz;
    case ViewerDemand::Background:
      return kAdaptiveBackgroundHz;
    case ViewerDemand::Active:
      return kAdaptiveActiveViewerHz;
  }
  return kAdaptiveNoViewerHz;
}

// Clamp a requested publish interval into the legal band.
[[nodiscard]] inline std::uint32_t clamped_interval_ms(std::uint32_t requested) {
  if (requested < kWebRadarMinIntervalMs) {
    return kWebRadarMinIntervalMs;
  }
  if (requested > kWebRadarMaxIntervalMs) {
    return kWebRadarMaxIntervalMs;
  }
  return requested;
}

}  // namespace radar_domain
