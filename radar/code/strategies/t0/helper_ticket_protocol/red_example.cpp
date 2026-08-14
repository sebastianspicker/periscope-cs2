#include "red_example.hpp"
#include <cstdio>

namespace examples::helper_ticket_protocol {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto helper = w.spawn("helper.exe");
  const auto consumer = w.spawn("radar.exe");
  if (!w.proc(helper) || !w.proc(consumer))
    return {false, steps, "helper/consumer spawn failed"};

  sim::SharedSection sec;
  sec.name = "Local\\RadarHelperTicket";
  sec.creator_pid = helper;
  sec.consumer_pid = consumer;
  sec.carries_entity_bytes = true;
  w.add_section(sec);

  w.helper_ticket_active = true;
  w.helper_ticket_id = 0xABCD;
  w.helper_ticket_shared_memory_created = true;
  w.helper_ticket_claimed = true;

  const bool scar = w.helper_ticket_active && w.helper_ticket_shared_memory_created && w.helper_ticket_claimed;
  if (!scar)
    return {false, steps, "helper ticket scar failed"};

  std::printf("[T0 helper_ticket_protocol] step %d: ticket id=0x%04x claimed via shared memory\n", ++steps, w.helper_ticket_id);
  w.note("helper_ticket_protocol: shared memory ticket protocol");
  return {true, steps, "helper_ticket_protocol: ticket id=0x" + std::to_string(w.helper_ticket_id), w.helper_ticket_id, true, true};
}

}  // namespace examples::helper_ticket_protocol
