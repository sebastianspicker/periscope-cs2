#pragma once

// T2 surface: usermode client talks to a kernel read channel via IOCTL.
// No usermode VM_READ handle on the game. World-backed primary path.

#include "ac/memory_backend.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace t2_red {

// Lab type `IoctlReadBackend` used by this educational unit.
class IoctlReadBackend final : public ac::IMemoryBackend {
 public:
  ac::Tier tier() const override { return ac::Tier::T2_KernelByovd; }
  std::string_view name() const override { return name_.c_str(); }

  /// Unit-test path: lab fixture (no World drivers).
  ac::Status attach_fixture(std::uint32_t fixture_id);

  /// Primary: open mem-R/W device already present on World; bind target game.
  ac::Status attach_world(sim::World& world, std::uint32_t opener_pid,
                          std::uint32_t game_pid, const std::string& device);

  ac::Status attach(std::uint32_t target_id) override;
  void detach() override;
  bool is_attached() const override { return attached_; }
  ac::ReadResult read(const ac::ReadRequest& req) override;

  bool exposes_usermode_handle_to_game() const { return false; }
  bool opens_device() const { return device_open_; }
  const std::string& device_name() const { return device_name_; }
  std::uint32_t opener_pid() const { return opener_pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  int ioctl_ops() const { return ioctl_ops_; }
  std::uint64_t bytes_read_total() const { return bytes_read_; }

 private:
  enum class Mode : std::uint8_t { None, Fixture, World };

  Mode mode_ = Mode::None;
  bool attached_ = false;
  bool device_open_ = false;
  std::uint32_t target_id_ = 0;
  std::uint32_t opener_pid_ = 0;
  std::uint32_t game_pid_ = 0;
  sim::World* world_ = nullptr;
  std::string device_name_ = "\\\\.\\AcLabMemRw";
  std::string name_ = "t2_ioctl";
  int ioctl_ops_ = 0;
  std::uint64_t bytes_read_ = 0;
};

}  // namespace t2_red
