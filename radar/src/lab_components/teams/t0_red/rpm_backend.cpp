// rpm_backend.cpp — T0 IMemoryBackend over sim World open_process/read_mem scars.
// Educational RPM stand-in; never touches real process memory.

#include "t0_red/rpm_backend.hpp"

#include "lab/fixture_process.hpp"

namespace t0_red {

// RpmBackend::attach_fixture: Attach to lab FixtureProcess id.
ac::Status RpmBackend::attach_fixture(std::uint32_t fixture_id) {
  detach();
  if (fixture_id != lab::global_fixture().id()) {
    // Intentionally LabOnly: real process attach is out of scope for this lab.
    return ac::Status::LabOnly;
  }
  mode_ = Mode::Fixture;
  target_id_ = fixture_id;
  attached_ = true;
  name_ = "t0_rpm_fixture";
  return ac::Status::Ok;
}

// RpmBackend::attach_world: OpenProcess VmRead on World game pid (primary T0 scar).
ac::Status RpmBackend::attach_world(sim::World& world, std::uint32_t reader_pid,
                                    std::uint32_t game_pid, bool syscall_path) {
  detach();
  if (!world.proc(reader_pid) || !world.proc(game_pid)) {
    return ac::Status::InvalidArgument;
  }
  auto* g = world.proc(game_pid);
  if (!g || !g->is_game) {
    return ac::Status::InvalidArgument;
  }
  // Primary T0 scar: OpenProcess(PROCESS_VM_READ).
  if (!world.open_process(reader_pid, game_pid, sim::AccessMask::VmRead,
                          syscall_path)) {
    return ac::Status::Denied;
  }
  world_ = &world;
  reader_pid_ = reader_pid;
  game_pid_ = game_pid;
  target_id_ = game_pid;
  mode_ = Mode::World;
  attached_ = true;
  name_ = syscall_path ? "t0_rpm_world_syscall" : "t0_rpm_world_winapi";
  world.note("t0 RpmBackend attach_world reader=" + std::to_string(reader_pid) +
             " game=" + std::to_string(game_pid) +
             (syscall_path ? " path=syscall" : " path=winapi"));
  return ac::Status::Ok;
}

// RpmBackend::attach: Bind backend/agent to a lab target id (fixture or World pid).
ac::Status RpmBackend::attach(std::uint32_t target_id) {
  // Default IMemoryBackend path: fixture only (safe unit tests).
  return attach_fixture(target_id);
}

// RpmBackend::detach: Clear attach state.
void RpmBackend::detach() {
  if (mode_ == Mode::World && world_ && reader_pid_) {
    world_->close_handles_from(reader_pid_);
    world_->note("t0 RpmBackend detach closed handles from " +
                 std::to_string(reader_pid_));
  }
  mode_ = Mode::None;
  attached_ = false;
  target_id_ = 0;
  reader_pid_ = 0;
  game_pid_ = 0;
  world_ = nullptr;
  name_ = "t0_rpm";
}

// RpmBackend::read: Perform a lab memory read (bytes + Status)
ac::ReadResult RpmBackend::read(const ac::ReadRequest& req) {
  ac::ReadResult out;
  if (!attached_) {
    out.status = ac::Status::Unavailable;
    return out;
  }
  if (mode_ == Mode::Fixture) {
    last_require_handle_ = false;
    out = lab::global_fixture().read_bytes(req.address, req.size);
    if (out.status == ac::Status::Ok) {
      bytes_read_ += out.bytes.size();
      ++read_ops_;
    }
    return out;
  }
  if (mode_ == Mode::World && world_) {
    last_require_handle_ = true;
    out = world_->read_mem(reader_pid_, game_pid_, req.address, req.size,
                           /*require_handle=*/true);
    if (out.status == ac::Status::Ok) {
      bytes_read_ += out.bytes.size();
      ++read_ops_;
    }
    return out;
  }
  out.status = ac::Status::Unavailable;
  return out;
}

// RpmBackend::handle_count: How many VmRead handles this backend currently exposes.
std::uint32_t RpmBackend::handle_count() const {
  if (mode_ != Mode::World || !world_) {
    return 0;
  }
  std::uint32_t n = 0;
  for (const auto& h : world_->handles_to(game_pid_)) {
    if (h.owner_pid == reader_pid_ &&
        sim::has(h.access, sim::AccessMask::VmRead)) {
      ++n;
    }
  }
  return n;
}

}  // namespace t0_red
