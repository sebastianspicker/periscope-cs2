#include "red_example.hpp"

#include <cstdio>

namespace examples::handle_origin_chain {

RedResult apply(sim::World& w) {
  RedResult result;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    return {false, 0, "precondition failed: game process unavailable"};
  }

  const auto donor = w.spawn("origin-donor.exe");
  const auto relay = w.spawn("origin-relay.exe");
  const auto consumer = w.spawn("origin-consumer.exe");
  if (!w.proc(donor) || !w.proc(relay) || !w.proc(consumer)) {
    return {false, 0, "actor spawn verification failed"};
  }
  result.steps = 1;

  // Hop 1: the donor opens a legitimate VM_READ handle.
  if (!w.open_process(donor, game_pid, sim::AccessMask::VmRead)) {
    return {false, result.steps, "donor VM_READ open failed"};
  }
  result.steps = 2;

  // Hop 2: the relay duplicates from the donor. The system records the relay
  // as the immediate owner, which is what an owner-only sensor sees.
  w.record_handle_table(relay, game_pid,
                        static_cast<std::uint32_t>(sim::AccessMask::VmRead),
                        ac::HandleAcquisitionModel::HandleDuplicate,
                        /*ephemeral=*/false, /*via_proxy=*/true);
  result.steps = 3;

  // Hop 3: the consumer duplicates from the relay. The chain is now two deep;
  // the consumer's own handle points at the relay, not the donor.
  w.record_handle_table(consumer, game_pid,
                        static_cast<std::uint32_t>(sim::AccessMask::VmRead),
                        ac::HandleAcquisitionModel::HandleDuplicate,
                        /*ephemeral=*/false, /*via_proxy=*/true);
  w.donor_hijack_active = true;
  w.donor_hijack_donor_pid = donor;
  w.donor_hijack_consumer_pid = consumer;
  w.handle_chain_depth = 2;
  // Mirror the duplicated handle into the process edge graph so the read
  // path accepts it (the table entry above is the AC-visible record).
  w.handles.push_back(sim::Handle{consumer, game_pid, sim::AccessMask::VmRead,
                                  false, false, false, true});
  result.steps = 4;

  // The consumer reads through its duplicated handle. The data path works;
  // the attribution path is the interesting part.
  auto read = w.read_mem(consumer, game_pid,
                         game->base + w.lab_entity_table_rel, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    return {false, result.steps, "consumer read through duplicated handle failed"};
  }
  std::uint32_t entity_count = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    entity_count |= static_cast<std::uint32_t>(read.bytes[i]) << (8 * i);
  }
  if (entity_count == 0) {
    return {false, result.steps, "duplicated-handle read returned no entities"};
  }
  result.steps = 5;

  result.achieved = true;
  result.detail = "handle_origin_chain: chain_depth=2 consumer=" +
                  std::to_string(consumer) + " donor=" + std::to_string(donor) +
                  " entity_count=" + std::to_string(entity_count);
  std::printf("[T0 handle_origin_chain] step %d: two-hop duplicate read ok\n",
              result.steps);
  w.note(result.detail);
  return result;
}

}  // namespace examples::handle_origin_chain
