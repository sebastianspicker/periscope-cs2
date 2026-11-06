#pragma once

// Internal helpers shared across signatures translation units.
#include "cs2/signatures.hpp"

#include <string>

namespace cs2 {
namespace sig_detail {

// Concatenated embedded AOB pattern corpus (name + hex bytes lines).
std::string embedded_patterns_text();

}  // namespace sig_detail
}  // namespace cs2
