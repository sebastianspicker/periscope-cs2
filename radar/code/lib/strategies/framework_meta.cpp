// framework_meta.cpp — Family / string helpers for the strategies framework.
// Lives in ac_strategy_core so catalog_util and pair runners share one definition.

#include "strategies/framework.hpp"

#include <cctype>
#include <cstring>

namespace strategies {

const char* family_name(Family f) {
  switch (f) {
    case Family::Delivery:
      return "Delivery";
    case Family::Feature:
      return "Feature";
    case Family::Evasion:
      return "Evasion";
    case Family::Detection:
      return "Detection";
    case Family::Structural:
      return "Structural";
  }
  return "Unknown";
}

bool iequals(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (std::tolower(static_cast<unsigned char>(*a)) !=
        std::tolower(static_cast<unsigned char>(*b))) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == *b;
}

}  // namespace strategies
