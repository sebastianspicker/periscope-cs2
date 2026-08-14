// ioctl_backend.cpp — implements ioctl_backend (T2 red).
// Key methods: attach_fixture, attach_world, to_string, attach, detach.

#include "t2_red/ioctl_backend.hpp"

#include "lab/fixture_process.hpp"

namespace t2_red {

// IoctlReadBackend::attach_fixture: Attach ioctl-shaped backend to fixture.
ac::Status IoctlReadBackend::attach_fixture(std::uint32_t fixture_id) {
  detach();
  if (fixture_id != lab::global_fixture().id()) {
    return ac::Status::LabOnly;
  }
  mode_ = Mode::Fixture;
  device_open_ = true;
  target_id_ = fixture_id;
  attached_ = true;
  name_ = "t2_ioctl_fixture";
  return ac::Status::Ok;
}

// IoctlReadBackend::attach_world: Attach via device ioctl path on World (no game handle).
ac::Status IoctlReadBackend::attach_world(sim::World& world,
                                          std::uint32_t opener_pid,
                                          std::uint32_t game_pid,
                                          const std::string& device) {
  detach();
  if (!world.proc(opener_pid) || !world.proc(game_pid)) {
    return ac::Status::InvalidArgument;
  }
  auto* g = world.proc(game_pid);
  if (!g || !g->is_game) {
    return ac::Status::InvalidArgument;
  }
  bool found = false;
  for (const auto& d : world.devices) {
    if (d.name == device && d.mem_rw_ioctl) {
      found = true;
      break;
    }
  }
  if (!found) {
    return ac::Status::Unavailable;
  }
  // Optional: BYOVD policy may already block — try a probe read later.
  world_ = &world;
  opener_pid_ = opener_pid;
  game_pid_ = game_pid;
  device_name_ = device;
  device_open_ = true;
  target_id_ = game_pid;
  mode_ = Mode::World;
  attached_ = true;
  name_ = "t2_ioctl_world";
  world.note("t2 IoctlReadBackend attach_world opener=" +
             std::to_string(opener_pid) + " device=" + device);
  return ac::Status::Ok;
}

// IoctlReadBackend::attach: Bind backend/agent to a lab target id (fixture or World pid).
ac::Status IoctlReadBackend::attach(std::uint32_t target_id) {
  return attach_fixture(target_id);
}

// IoctlReadBackend::detach: Clear attach state.
void IoctlReadBackend::detach() {
  mode_ = Mode::None;
  attached_ = false;
  device_open_ = false;
  target_id_ = 0;
  opener_pid_ = 0;
  game_pid_ = 0;
  world_ = nullptr;
  name_ = "t2_ioctl";
}

// IoctlReadBackend::read: Perform a lab memory read (bytes + Status)
ac::ReadResult IoctlReadBackend::read(const ac::ReadRequest& req) {
  ac::ReadResult out;
  if (!attached_ || !device_open_) {
    out.status = ac::Status::Unavailable;
    return out;
  }
  if (mode_ == Mode::Fixture) {
    out = lab::global_fixture().read_bytes(req.address, req.size);
  } else if (mode_ == Mode::World && world_) {
    std::vector<std::uint8_t> buf;
    if (!world_->device_ioctl_read(opener_pid_, device_name_, game_pid_,
                                   req.address, req.size, buf)) {
      out.status = ac::Status::Denied;
      return out;
    }
    out.bytes = std::move(buf);
    out.status = ac::Status::Ok;
  } else {
    out.status = ac::Status::Unavailable;
    return out;
  }
  if (out.status == ac::Status::Ok) {
    bytes_read_ += out.bytes.size();
    ++ioctl_ops_;
  }
  return out;
}

}  // namespace t2_red
