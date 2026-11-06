#pragma once

// Donor/proxy model for the educational simulation. It records the observable
// process, handle, injection, and section artifacts of a donor relay.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

/// A candidate donor process that already holds a handle to the game.
struct DonorCandidate {
  std::uint32_t pid = 0;
  std::string name;
  bool has_game_handle = false;
  std::uint32_t handle_access_mask = 0;
  double legitimacy_score = 0.0;
  ac::HandleAcquisitionModel acquisition = ac::HandleAcquisitionModel::None;
};

/// Handle transfer record for a donor-to-consumer duplication simulation.
struct HandleTransferRecord {
  std::uint32_t source_pid = 0;
  std::uint32_t target_pid = 0;
  std::uint32_t new_owner_pid = 0;
  std::uint32_t original_access = 0;
  std::uint32_t transferred_access = 0;
  bool rights_reduced = false;
  std::uint64_t transfer_tick = 0;
};

/// Simulated injected reader payload; no executable bytes are produced.
struct ShellcodePayload {
  std::uint32_t donor_pid = 0;
  std::uint64_t payload_address = 0;
  std::size_t payload_size = 0;
  std::uint32_t game_pid = 0;
  std::uint32_t handle_value = 0;
  bool direct_syscall = false;
  bool injected = false;
};

/// Shared-memory relay metadata between a donor and consumer process.
struct SharedMemorySection {
  std::string name;
  std::uint32_t creator_pid = 0;
  std::uint32_t consumer_pid = 0;
  std::size_t size = 0;
  bool name_randomized = false;
  bool carries_entity_bytes = false;
};

/// Donor discovery and relay artifact orchestration.
class DonorDiscovery {
 public:
  explicit DonorDiscovery(World& world);

  std::vector<DonorCandidate> find_candidates(std::uint32_t game_pid);
  DonorCandidate select_best(const std::vector<DonorCandidate>& candidates);
  bool discover_and_select(std::uint32_t game_pid);

  const DonorCandidate& selected() const { return selected_; }

  HandleTransferRecord transfer_handle(std::uint32_t donor_pid,
                                       std::uint32_t game_pid,
                                       std::uint32_t cheat_pid);
  ShellcodePayload inject_payload(std::uint32_t donor_pid,
                                  std::uint32_t game_pid,
                                  std::uint32_t handle_value,
                                  bool use_direct_syscall);
  SharedMemorySection create_shared_section(std::uint32_t creator_pid,
                                            std::uint32_t consumer_pid,
                                            std::size_t size,
                                            bool randomize_name);

 private:
  World& world_;
  DonorCandidate selected_{};
};

}  // namespace sim
