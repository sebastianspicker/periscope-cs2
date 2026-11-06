#include "cs2/diagnostic_stack_analysis.hpp"

#include <algorithm>

namespace cs2::sensors {
namespace {

constexpr std::size_t kMaxCapturedStackFrames = 2944 / sizeof(std::uint64_t);
constexpr std::uint64_t kSimulatedCheatModuleSize = 0x100000;

}  // namespace

StackAnalysisSensor::StackAnalysisSensor(sim::World& world) : world_(world) {}

void StackAnalysisSensor::analyze_stack_for_cheat_frames(std::uint32_t cheat_pid) {
  const auto* cheat = world_.proc(cheat_pid);
  if (!cheat || world_.diagnostic_state.exceptions.empty()) return;

  auto& last_exception = world_.diagnostic_state.exceptions.back();
  const std::uint64_t cheat_base = cheat->base;
  const std::uint64_t cheat_end = cheat_base + kSimulatedCheatModuleSize;
  const std::size_t captured_frames =
      std::min(last_exception.stack_frames.size(), kMaxCapturedStackFrames);

  last_exception.contains_cheat_frames = false;
  last_exception.stack_depth = static_cast<std::uint32_t>(captured_frames);
  for (std::size_t index = 0; index < captured_frames; ++index) {
    auto& frame = last_exception.stack_frames[index];
    frame.in_cheat_module = frame.return_address >= cheat_base &&
                            frame.return_address <= cheat_end;
    last_exception.contains_cheat_frames |= frame.in_cheat_module;
  }

  if (last_exception.contains_cheat_frames) {
    world_.note("[StackAnalysis] Cheat frames detected on exception stack!");
  }
}

bool StackAnalysisSensor::is_self_patching_active() const {
  return world_.game_self_patching_active;
}

}  // namespace cs2::sensors
