#pragma once
#include "sim/world.hpp"
#include <string>
namespace examples::schema_saas_product {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::schema_saas_product
