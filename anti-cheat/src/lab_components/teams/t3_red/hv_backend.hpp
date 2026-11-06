#pragma once

// T3 surface: memory reads mediated by a personal hypervisor channel.
// Never enters real VMX; World-backed primary path + lab fixture path.

#include "ac/memory_backend.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace t3_red {

// Lab type `HvReadBackend` used by this educational unit.
class HvReadBackend final : public ac::IMemoryBackend {
 public:
  ac::Tier tier() const override { return ac::Tier::T3_Hypervisor; }
  std::string_view name() const override { return name_.c_str(); }

  /// Lab-only fixture path (no World HV).
  ac::Status attach_fixture(std::uint32_t fixture_id);

  /// Primary: require personal HV active; bind reader+game for hv_read.
  ac::Status attach_world(sim::World& world, std::uint32_t reader_pid,
                          std::uint32_t game_pid);

  /// Clear VBS/HVCI and start personal HV (sim).
  ac::Status simulate_hv_init(sim::World& world, const std::string& vendor);

  ac::Status attach(std::uint32_t target_id) override;
  void detach() override;
  bool is_attached() const override { return attached_; }
  ac::ReadResult read(const ac::ReadRequest& req) override;

  bool exposes_usermode_handle_to_game() const { return false; }
  bool requires_vbs_off() const { return true; }
  bool hv_active_lab() const { return hv_active_; }
  std::uint32_t reader_pid() const { return reader_pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  int read_ops() const { return read_ops_; }
  std::uint64_t bytes_read_total() const { return bytes_read_; }

 private:
  enum class Mode : std::uint8_t { None, Fixture, World };

  Mode mode_ = Mode::None;
  bool attached_ = false;
  bool hv_active_ = false;
  std::uint32_t target_id_ = 0;
  std::uint32_t reader_pid_ = 0;
  std::uint32_t game_pid_ = 0;
  sim::World* world_ = nullptr;
  std::string name_ = "t3_hv";
  int read_ops_ = 0;
  std::uint64_t bytes_read_ = 0;
};

}  // namespace t3_red
