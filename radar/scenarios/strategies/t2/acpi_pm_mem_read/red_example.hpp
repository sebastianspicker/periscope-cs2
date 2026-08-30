#pragma once

#include "sim/world.hpp"

#include <string>

namespace examples::acpi_pm_mem_read {

struct RedResult {
  bool achieved;
  int steps;
  int smi_count;
  std::string detail;
};

RedResult apply(sim::World& w);

}  // namespace examples::acpi_pm_mem_read
