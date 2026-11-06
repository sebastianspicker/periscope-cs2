#pragma once

// Simulation-only proxy reader lesson. No operating-system handle or IPC is created.
#include "sim/world.hpp"
#include "sim/narrative.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace examples::proxy_hijack_reader {

struct ReadRequest {
  std::string resource;
  std::uint64_t address = 0;
  std::size_t size = 0;
};

struct RedResult {
  bool achieved = false;
  std::string detail;
  std::uint32_t proxy_pid = 0;
  std::uint32_t radar_pid = 0;
};

RedResult apply(sim::World& w, sim::Narrator& n);

}  // namespace examples::proxy_hijack_reader
