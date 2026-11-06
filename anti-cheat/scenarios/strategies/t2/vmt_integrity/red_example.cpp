#include "red_example.hpp"

#include <cstdio>

bool vmt_integrity_red_apply(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "step 1: validate game modules for VMT surface");
  auto& vmts = w.diagnostic_state.vmts;
  vmts.clear();
  const auto* game = w.proc(w.game_pid());
  if (game == nullptr || game->modules.empty()) return false;
  const auto base = game->modules.front().base;
  const auto& host = game->modules.front().name;

  n.say(sim::Side::Red, "step 2: snapshot baseline interface VMT table (112 slots)");
  for (int index = 0; index != 112; ++index) {
    vmts.push_back({base + 0x1000u + static_cast<std::uint64_t>(index) * 8u,
                    "interface_" + std::to_string(index), host, false, true});
  }
  if (vmts.size() != 112) return false;

  // Capture expected CRC baseline before tamper so blue can compare.
  std::uint32_t baseline_crc = 0;
  {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const auto& e : vmts) {
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(&e.address);
      for (std::size_t i = 0; i < sizeof(e.address); ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit != 8; ++bit)
          crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
      }
    }
    baseline_crc = ~crc;
  }
  w.diagnostic_state.expected_vmt_crc32 = baseline_crc;

  n.say(sim::Side::Red, "step 3: replace Message-160 inventory slot with foreign target");
  vmts[17].address = base + 0x900000;
  vmts[17].owner = "Message160Inventory";
  vmts[17].module_name = "foreign_vmt_stub";
  vmts[17].expected = false;

  n.say(sim::Side::Red, "step 4: plant co-scar — secondary unexpected entity slot");
  vmts[42].address = base + 0x910000;
  vmts[42].owner = "EntityListProxy";
  vmts[42].module_name = "foreign_vmt_stub";
  vmts[42].expected = false;
  vmts[42].entity_vmt = true;

  n.say(sim::Side::Red, "step 5: verify multi-slot tamper residual");
  int unexpected = 0;
  for (const auto& e : vmts) {
    if (!e.expected) ++unexpected;
  }
  if (unexpected < 2 || vmts[17].module_name != "foreign_vmt_stub") return false;

  n.say(sim::Side::Red, "step 6: record VMT integrity scar for blue sensors");
  w.diagnostic_state.anomalous_vmt = true;
  w.diag_vmt_collected = false;  // blue will collect
  std::printf("[red:vmt_integrity] achieved: slots=%zu unexpected=%d slot17=%s\n",
              vmts.size(), unexpected, vmts[17].module_name.c_str());
  w.note("vmt_integrity: multi-slot foreign VMT targets planted");
  return true;
}
