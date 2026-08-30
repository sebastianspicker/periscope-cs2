#pragma once

// Pure catalog query helpers. Operate on StrategyEntry collections without
// re-implementing the registry — callers pass strategies::catalog() or a
// custom subset. Fully functional; no deferred paths.

#include "strategies/framework.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace strategies {

/// Family histogram over a catalog slice.
struct FamilyHistogram {
  int delivery = 0;
  int feature = 0;
  int evasion = 0;
  int detection = 0;
  int structural = 0;
  int total = 0;
};

/// Tier membership for a meta.tiers string ("T0", "T0-T1", "all", ...).
bool tier_matches(const char* tiers, const char* tier_filter);

/// Case-insensitive family name match.
bool family_matches(Family f, const char* family_filter);

/// Filter entries by optional family and/or tier (nullptr = no filter).
std::vector<const StrategyEntry*> filter_entries(
    const std::vector<StrategyEntry>& cat, const char* family_filter,
    const char* tier_filter);

/// Count entries by family.
FamilyHistogram family_histogram(const std::vector<StrategyEntry>& cat);

/// Count how many entries support real mode (uses supports_real_mode).
int count_real_capable(const std::vector<StrategyEntry>& cat);

/// Build a human-readable stats table (families + real-capable).
std::string format_catalog_stats(const std::vector<StrategyEntry>& cat);

/// Validate catalog integrity: unique ids, non-null run fn, non-empty title.
/// Returns empty string on success; otherwise a multi-line error report.
std::string validate_catalog(const std::vector<StrategyEntry>& cat);

/// Collect all meta.id values (stable catalog order).
std::vector<std::string> list_ids(const std::vector<StrategyEntry>& cat);

/// Find index of id in catalog; -1 if missing.
int index_of(const std::vector<StrategyEntry>& cat, std::string_view id);

}  // namespace strategies
