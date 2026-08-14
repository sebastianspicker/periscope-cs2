#pragma once

// Shared red/blue outcome types for strategy pairs and the support library.
// Pairs may still define their own result structs; these are the canonical
// support-library types used by multi-reason, scar sensors, and pair_util.

#include <string>
#include <vector>

namespace strategies {

/// Canonical red-team apply outcome (multi-step narrative).
struct RedOutcome {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Canonical blue-team detect outcome (multi-reason narrative).
struct BlueOutcome {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0.0;
  std::vector<std::string> reasons;
  std::string detail;
};

/// One weighted reason contributed to a multi-reason blue verdict.
struct SignalHit {
  std::string reason;
  double weight = 0.0;
  bool present = false;
};

}  // namespace strategies

// Back-compat alias used by many pairs under examples::support.
namespace examples::support {
using RedOutcome = strategies::RedOutcome;
using BlueOutcome = strategies::BlueOutcome;
using SignalHit = strategies::SignalHit;
}  // namespace examples::support
