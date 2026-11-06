#pragma once

// Staged loader: stub → auth → in-memory payload.
// Leaves private_rx / short-lived stub / parent lineage scars on World.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t1_red {

enum class StageState : std::uint8_t {
  StubOnly = 0,
  Authenticated,
  PayloadMapped,
};

// Aggregate outcome fields for `StageReport` (lab narrative / tests).
struct StageReport {
  StageState state = StageState::StubOnly;
  bool private_rx = false;
  bool short_lived_stub = false;
  std::uint32_t stub_pid = 0;
  std::uint32_t payload_pid = 0;
  std::size_t payload_bytes = 0;
  std::string detail;
};

// Lab type `StagedLoader` used by this educational unit.
class StagedLoader {
 public:
  /// In-memory auth (lab token). No network.
  ac::Status authenticate_lab(const char* token);

  /// Map payload bytes into private RX (lab).
  ac::Status map_payload_lab(const std::vector<std::uint8_t>& bytes);

  /// Full multi-step on World: spawn stub parent → auth → spawn payload child
  /// with manual_mapped_region / parent lineage scars.
  StageReport run_on_world(sim::World& w, const char* token,
                           const std::vector<std::uint8_t>& payload,
                           const std::string& payload_name = "soft-radar.exe");

  StageState state() const { return state_; }
  bool has_private_rx_region() const {
    return state_ == StageState::PayloadMapped;
  }
  const std::vector<std::uint8_t>& payload() const { return payload_; }
  const StageReport& last() const { return last_; }

 private:
  StageState state_ = StageState::StubOnly;
  std::vector<std::uint8_t> payload_;
  StageReport last_{};
};

}  // namespace t1_red
