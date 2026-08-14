#pragma once

// T0 adversary: classic usermode RPM-shaped backend.
// Full educational paths:
//   1) Lab fixture (unit tests without a full World)
//   2) sim::World OpenProcess + ReadProcessMemory-shaped reads (primary)

#include "ac/memory_backend.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace t0_red {

/// Classic usermode RPM backend. Always exposes a cross-process read handle scar
/// when attached via World; fixture mode is handle-less but still LabOnly demo.
class RpmBackend final : public ac::IMemoryBackend {
 public:
  ac::Tier tier() const override { return ac::Tier::T0_UsermodeRpm; }
  std::string_view name() const override { return name_.c_str(); }

  /// Attach to in-process lab fixture (no World). Educational unit-test path.
  ac::Status attach_fixture(std::uint32_t fixture_id);

  /// Attach to sim game: spawn/open VM_READ from reader_pid → game_pid.
  /// Leaves a blue-visible handle scar on World.
  ac::Status attach_world(sim::World& world, std::uint32_t reader_pid,
                          std::uint32_t game_pid, bool syscall_path = false);

  /// IMemoryBackend: fixture id only unless world mode active.
  ac::Status attach(std::uint32_t target_id) override;
  void detach() override;
  bool is_attached() const override { return attached_; }
  ac::ReadResult read(const ac::ReadRequest& req) override;

  /// Blue-relevant: World attach always holds a cross-process VM_READ handle.
  bool exposes_usermode_handle() const { return mode_ == Mode::World; }

  std::uint32_t reader_pid() const { return reader_pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  std::uint32_t handle_count() const;
  bool last_read_required_handle() const { return last_require_handle_; }
  std::uint64_t bytes_read_total() const { return bytes_read_; }
  int read_ops() const { return read_ops_; }

 private:
  enum class Mode : std::uint8_t { None, Fixture, World };

  Mode mode_ = Mode::None;
  bool attached_ = false;
  std::uint32_t target_id_ = 0;
  std::uint32_t reader_pid_ = 0;
  std::uint32_t game_pid_ = 0;
  sim::World* world_ = nullptr;
  std::string name_ = "t0_rpm";
  bool last_require_handle_ = true;
  std::uint64_t bytes_read_ = 0;
  int read_ops_ = 0;
};

}  // namespace t0_red
