// syscall_backend.cpp — T1 red syscall-soft path (indirect/stub) over lab World.
// Plants educational residuals.

#include "t1_red/syscall_backend.hpp"

#include "lab/fixture_process.hpp"

namespace t1_red {

// SyscallBackend::attach_fixture: Attach syscall backend to fixture process.
ac::Status SyscallBackend::attach_fixture(std::uint32_t fixture_id) {
  detach();
  if (fixture_id != lab::global_fixture().id()) {
    // Real NtOpenProcess against live PIDs is out of lab scope.
    return ac::Status::LabOnly;
  }
  mode_ = Mode::Fixture;
  attached_ = true;
  name_ = "t1_syscall_fixture";
  return ac::Status::Ok;
}

// SyscallBackend::attach_world: Attach via World open_process with syscall_path=true.
ac::Status SyscallBackend::attach_world(sim::World& world,
                                        std::uint32_t reader_pid,
                                        std::uint32_t game_pid) {
  detach();
  if (!world.proc(reader_pid) || !world.proc(game_pid)) {
    return ac::Status::InvalidArgument;
  }
  auto* g = world.proc(game_pid);
  if (!g || !g->is_game) {
    return ac::Status::InvalidArgument;
  }
  // Syscall-shaped open: blue usermode hooks miss; handle table still records.
  if (!world.open_process(reader_pid, game_pid, sim::AccessMask::VmRead,
                          /*syscall_path=*/true)) {
    return ac::Status::Denied;
  }
  world_ = &world;
  reader_pid_ = reader_pid;
  game_pid_ = game_pid;
  mode_ = Mode::World;
  attached_ = true;
  name_ = "t1_syscall_world";
  world.note("t1 SyscallBackend attach_world reader=" +
             std::to_string(reader_pid) + " game=" + std::to_string(game_pid) +
             " path=syscall");
  return ac::Status::Ok;
}

// SyscallBackend::attach: Bind backend/agent to a lab target id (fixture or World pid).
ac::Status SyscallBackend::attach(std::uint32_t target_id) {
  return attach_fixture(target_id);
}

// SyscallBackend::detach: Clear attach state.
void SyscallBackend::detach() {
  if (mode_ == Mode::World && world_ && reader_pid_) {
    world_->close_handles_from(reader_pid_);
    world_->note("t1 SyscallBackend detach closed handles from " +
                 std::to_string(reader_pid_));
  }
  mode_ = Mode::None;
  attached_ = false;
  reader_pid_ = 0;
  game_pid_ = 0;
  world_ = nullptr;
  name_ = "t1_syscall";
}

// SyscallBackend::read: Perform a lab memory read (bytes + Status)
ac::ReadResult SyscallBackend::read(const ac::ReadRequest& req) {
  ac::ReadResult out;
  if (!attached_) {
    out.status = ac::Status::Unavailable;
    return out;
  }
  if (mode_ == Mode::Fixture) {
    out = lab::global_fixture().read_bytes(req.address, req.size);
  } else if (mode_ == Mode::World && world_) {
    // NtReadVirtualMemory-shaped: still requires the open handle in lab model.
    out = world_->read_mem(reader_pid_, game_pid_, req.address, req.size,
                           /*require_handle=*/true);
  } else {
    out.status = ac::Status::Unavailable;
    return out;
  }
  if (out.status == ac::Status::Ok) {
    bytes_read_ += out.bytes.size();
    ++read_ops_;
  }
  return out;
}

// SyscallBackend::handle_count: Count handles owned by this syscall backend.
std::uint32_t SyscallBackend::handle_count() const {
  if (mode_ != Mode::World || !world_) {
    return 0;
  }
  std::uint32_t n = 0;
  for (const auto& h : world_->handles_to(game_pid_, true)) {
    if (h.owner_pid == reader_pid_ &&
        sim::has(h.access, sim::AccessMask::VmRead) && h.via_syscall_path) {
      ++n;
    }
  }
  return n;
}

}  // namespace t1_red
