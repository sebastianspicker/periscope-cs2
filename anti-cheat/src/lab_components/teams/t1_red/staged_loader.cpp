// staged_loader.cpp — implements staged_loader (T1 red).
// Key methods: authenticate_lab, map_payload_lab, run_on_world, to_string, to_string.

#include "t1_red/staged_loader.hpp"

#include <cstring>

namespace t1_red {

// StagedLoader::authenticate_lab: Staged loader auth step (lab token
ac::Status StagedLoader::authenticate_lab(const char* token) {
  if (token == nullptr || std::strlen(token) == 0) {
    return ac::Status::Denied;
  }
  state_ = StageState::Authenticated;
  return ac::Status::Ok;
}

// StagedLoader::map_payload_lab: Map payload region flags in World (manual-map residual).
ac::Status StagedLoader::map_payload_lab(const std::vector<std::uint8_t>& bytes) {
  if (state_ != StageState::Authenticated) {
    return ac::Status::Denied;
  }
  if (bytes.empty()) {
    return ac::Status::InvalidArgument;
  }
  payload_ = bytes;
  state_ = StageState::PayloadMapped;
  return ac::Status::Ok;
}

// StagedLoader::run_on_world: Run staged load sequence residuals on World processes.
StageReport StagedLoader::run_on_world(sim::World& w, const char* token,
                                       const std::vector<std::uint8_t>& payload,
                                       const std::string& payload_name) {
  last_ = {};
  // Step 1: short-lived stub parent (loader).
  const auto stub = w.spawn("stage-stub.exe");
  last_.stub_pid = stub;
  last_.short_lived_stub = true;
  last_.state = StageState::StubOnly;

  auto st = authenticate_lab(token);
  if (st != ac::Status::Ok) {
    last_.detail = "auth_failed";
    w.note("t1 staged_loader auth_failed");
    return last_;
  }
  last_.state = StageState::Authenticated;

  st = map_payload_lab(payload);
  if (st != ac::Status::Ok) {
    last_.detail = "map_failed";
    return last_;
  }

  // Step 2: payload process as child of stub (lineage scar).
  const auto child = w.spawn(payload_name, false, false, stub);
  last_.payload_pid = child;
  if (auto* p = w.proc(child)) {
    p->manual_mapped_region = true;
    p->reader_active = true;
  }
  // Optional: inject manual-map module into payload for PEB-hide narrative.
  sim::Module m;
  m.name = "payload.bin";
  m.base = 0x70000000ull;
  m.size = payload.size() < 0x1000 ? 0x1000 : payload.size();
  m.linked_in_peb = false;
  m.headers_erased = true;
  m.text_hash = "staged";
  w.inject_module(child, m, /*manual_map=*/true);

  last_.private_rx = true;
  last_.payload_bytes = payload.size();
  last_.state = StageState::PayloadMapped;
  last_.detail = "stub=" + std::to_string(stub) + " payload=" +
                 std::to_string(child) + " rx=1 bytes=" +
                 std::to_string(payload.size());
  w.note("t1 staged_loader " + last_.detail);
  // C2-shaped net scar (supportive).
  w.add_net(sim::NetFlow{child, "offsets.seller.example:443", true, false});
  return last_;
}

}  // namespace t1_red
