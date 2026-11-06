#pragma once

// T1 adversary: syscall-shaped RPM backend.
// Bypasses usermode ntdll/API hooks in the lab model, but still leaves a
// cross-process VM_READ handle scar (via_syscall_path=true).

#include "ac/memory_backend.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace t1_red {

// Lab type `SyscallBackend` used by this educational unit.
class SyscallBackend final : public ac::IMemoryBackend {
 public:
  ac::Tier tier() const override { return ac::Tier::T1_SyscallSoft; }
  std::string_view name() const override { return name_.c_str(); }

  /// Unit-test path: lab fixture only (no World handle).
  ac::Status attach_fixture(std::uint32_t fixture_id);

  /// Primary path: NtOpenProcess-shaped open with syscall_path=true on World.
  ac::Status attach_world(sim::World& world, std::uint32_t reader_pid,
                          std::uint32_t game_pid);

  ac::Status attach(std::uint32_t target_id) override;
  void detach() override;
  bool is_attached() const override { return attached_; }
  ac::ReadResult read(const ac::ReadRequest& req) override;

  /// Always true in World mode — the T1 lesson is handle remains.
  bool exposes_usermode_handle() const { return mode_ == Mode::World; }
  /// Lab model: direct/indirect syscalls never enter hooked ntdll stubs.
  bool bypasses_usermode_api_hooks() const { return true; }

  std::uint32_t reader_pid() const { return reader_pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  std::uint32_t handle_count() const;
  int read_ops() const { return read_ops_; }
  std::uint64_t bytes_read_total() const { return bytes_read_; }
  bool last_via_syscall() const { return mode_ == Mode::World; }

 private:
  enum class Mode : std::uint8_t { None, Fixture, World };

  Mode mode_ = Mode::None;
  bool attached_ = false;
  std::uint32_t reader_pid_ = 0;
  std::uint32_t game_pid_ = 0;
  sim::World* world_ = nullptr;
  std::string name_ = "t1_syscall";
  int read_ops_ = 0;
  std::uint64_t bytes_read_ = 0;
};

}  // namespace t1_red
