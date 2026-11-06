#include "red_example.hpp"
#include <cstdio>

namespace examples::handle_hijack_proxy_donor {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "precondition failed: game process unavailable"};

  const auto donor = w.spawn("donor.exe");
  auto* donor_proc = w.proc(donor);
  if (!donor_proc)
    return {false, steps, "donor spawn verification failed"};

  const auto consumer = w.spawn("radar-consumer.exe");
  if (!w.proc(consumer))
    return {false, steps, "consumer spawn verification failed"};

  if (!w.open_process(donor, game_pid, sim::AccessMask::VmRead, false))
    return {false, steps, "donor OpenProcess VM_READ failed"};

  bool donor_has_handle = false;
  for (const auto& h : w.handles_to(game_pid))
    if (h.owner_pid == donor && sim::has(h.access, sim::AccessMask::VmRead))
      donor_has_handle = true;
  if (!donor_has_handle)
    return {false, steps, "donor VM_READ handle verification failed"};

  auto read = w.read_mem(donor, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4)
    return {false, steps, "donor verification read failed"};

  w.donor_hijack_active = true;
  w.donor_hijack_donor_pid = donor;
  w.donor_hijack_consumer_pid = consumer;
  w.donor_handle_duplicated = true;

  const bool scar_ok = w.donor_hijack_active && w.donor_handle_duplicated;
  if (!scar_ok)
    return {false, steps, "scar verification failed: donor hijack state"};

  std::printf("[T0 handle_hijack_proxy_donor] step %d: donor hijack active, consumer=%u donor=%u\n", ++steps, consumer, donor);
  const std::string detail = "handle_hijack_proxy_donor: NtQSI+DuplicateHandle via donor pid=" + std::to_string(donor) + " consumer=" + std::to_string(consumer);
  w.note(detail);
  return {true, steps, detail, donor, consumer, true};
}

}  // namespace examples::handle_hijack_proxy_donor
