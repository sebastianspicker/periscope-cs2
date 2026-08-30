// hv_backend.cpp — T3 red HV path using World.trust posture flags.
// Simulated personal HV / attestation fields

#include "t3_red/hv_backend.hpp"

#include "lab/fixture_process.hpp"

namespace t3_red {

// HvReadBackend::simulate_hv_init: Simulate HV init residual.
ac::Status HvReadBackend::simulate_hv_init(sim::World& world,
                                           const std::string& vendor) {
  // Real packs: force VBS/HVCI off so personal HV can own VMX root.
  if (world.trust.vbs || world.trust.hvci) {
    hv_active_ = false;
    world.note("t3 HvReadBackend simulate_hv_init DENIED (VBS/HVCI on)");
    return ac::Status::Denied;
  }
  if (!world.try_start_personal_hv(vendor)) {
    hv_active_ = false;
    return ac::Status::Denied;
  }
  hv_active_ = true;
  world_ = &world;
  name_ = "t3_hv_init";
  world.note("t3 HvReadBackend personal_hv vendor=" + vendor);
  return ac::Status::Ok;
}

// HvReadBackend::attach_fixture: Attach HV backend to fixture (lab only).
ac::Status HvReadBackend::attach_fixture(std::uint32_t fixture_id) {
  detach();
  if (!hv_active_) {
    // Fixture path may self-activate for unit tests without World.
    hv_active_ = true;
  }
  if (fixture_id != lab::global_fixture().id()) {
    return ac::Status::LabOnly;
  }
  mode_ = Mode::Fixture;
  target_id_ = fixture_id;
  attached_ = true;
  name_ = "t3_hv_fixture";
  return ac::Status::Ok;
}

// HvReadBackend::attach_world: Attach HV read backend to World pids.
ac::Status HvReadBackend::attach_world(sim::World& world,
                                       std::uint32_t reader_pid,
                                       std::uint32_t game_pid) {
  detach();
  if (!world.trust.personal_hv_active) {
    return ac::Status::Unavailable;
  }
  if (!world.proc(reader_pid) || !world.proc(game_pid)) {
    return ac::Status::InvalidArgument;
  }
  auto* g = world.proc(game_pid);
  if (!g || !g->is_game) {
    return ac::Status::InvalidArgument;
  }
  world_ = &world;
  reader_pid_ = reader_pid;
  game_pid_ = game_pid;
  target_id_ = game_pid;
  hv_active_ = true;
  mode_ = Mode::World;
  attached_ = true;
  name_ = "t3_hv_world";
  world.note("t3 HvReadBackend attach_world reader=" +
             std::to_string(reader_pid) + " game=" + std::to_string(game_pid));
  return ac::Status::Ok;
}

// HvReadBackend::attach: Bind backend/agent to a lab target id (fixture or World pid).
ac::Status HvReadBackend::attach(std::uint32_t target_id) {
  return attach_fixture(target_id);
}

// HvReadBackend::detach: Clear attach state.
void HvReadBackend::detach() {
  mode_ = Mode::None;
  attached_ = false;
  target_id_ = 0;
  reader_pid_ = 0;
  game_pid_ = 0;
  // Keep hv_active_ if World still has personal HV.
  if (world_ && world_->trust.personal_hv_active) {
    hv_active_ = true;
  } else {
    hv_active_ = false;
  }
  world_ = nullptr;
  name_ = "t3_hv";
}

// HvReadBackend::read: Perform a lab memory read (bytes + Status)
ac::ReadResult HvReadBackend::read(const ac::ReadRequest& req) {
  ac::ReadResult out;
  if (!attached_ || !hv_active_) {
    out.status = ac::Status::Unavailable;
    return out;
  }
  if (mode_ == Mode::Fixture) {
    out = lab::global_fixture().read_bytes(req.address, req.size);
  } else if (mode_ == Mode::World && world_) {
    std::vector<std::uint8_t> buf;
    if (!world_->hv_read(reader_pid_, game_pid_, req.address, req.size, buf)) {
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
    ++read_ops_;
  }
  return out;
}

}  // namespace t3_red
