#include "strategies/catalog_util.hpp"

#include <cctype>
#include <cstring>
#include <sstream>
#include <unordered_set>

namespace strategies {
namespace {

bool ieq_local(const char* a, const char* b) {
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

// Local family label — mirrors family_name without depending on registry TU.
const char* family_label(Family f) {
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

// Real-mode eligibility from meta.tiers alone (T0/T1/all). Platform presence is
// a separate gate applied by run_real / supports_real_mode in the framework.
bool meta_tiers_real_capable(const StrategyMeta& meta) {
  const char* t = meta.tiers;
  if (!t) return false;
  return std::strstr(t, "T0") != nullptr || std::strstr(t, "T1") != nullptr ||
         std::strstr(t, "all") != nullptr;
}

}  // namespace

bool tier_matches(const char* tiers, const char* tier_filter) {
  if (!tier_filter || !tier_filter[0]) return true;
  if (!tiers || !tiers[0]) return false;
  // Crosscutting "all" matches every tier filter.
  if (std::strstr(tiers, "all") != nullptr) return true;
  return std::strstr(tiers, tier_filter) != nullptr;
}

bool family_matches(Family f, const char* family_filter) {
  if (!family_filter || !family_filter[0]) return true;
  return ieq_local(family_label(f), family_filter);
}

std::vector<const StrategyEntry*> filter_entries(
    const std::vector<StrategyEntry>& cat, const char* family_filter,
    const char* tier_filter) {
  std::vector<const StrategyEntry*> out;
  out.reserve(cat.size());
  for (const auto& e : cat) {
    if (!family_matches(e.meta.family, family_filter)) continue;
    if (!tier_matches(e.meta.tiers, tier_filter)) continue;
    out.push_back(&e);
  }
  return out;
}

FamilyHistogram family_histogram(const std::vector<StrategyEntry>& cat) {
  FamilyHistogram h;
  for (const auto& e : cat) {
    ++h.total;
    switch (e.meta.family) {
      case Family::Delivery:
        ++h.delivery;
        break;
      case Family::Feature:
        ++h.feature;
        break;
      case Family::Evasion:
        ++h.evasion;
        break;
      case Family::Detection:
        ++h.detection;
        break;
      case Family::Structural:
        ++h.structural;
        break;
    }
  }
  return h;
}

int count_real_capable(const std::vector<StrategyEntry>& cat) {
  int n = 0;
  for (const auto& e : cat) {
    // Tier-string eligibility (T0/T1/all). Runtime platform availability is a
    // separate gate applied by strategies::supports_real_mode / run_real.
    if (meta_tiers_real_capable(e.meta)) ++n;
  }
  return n;
}

std::string format_catalog_stats(const std::vector<StrategyEntry>& cat) {
  const auto h = family_histogram(cat);
  const int real_n = count_real_capable(cat);
  std::ostringstream oss;
  oss << "Catalog: " << h.total << " pairs\n"
      << "  Delivery:   " << h.delivery << "\n"
      << "  Feature:    " << h.feature << "\n"
      << "  Evasion:    " << h.evasion << "\n"
      << "  Detection:  " << h.detection << "\n"
      << "  Structural: " << h.structural << "\n"
      << "  Real-capable: " << real_n << "\n";
  return oss.str();
}

std::string validate_catalog(const std::vector<StrategyEntry>& cat) {
  std::ostringstream err;
  std::unordered_set<std::string> seen;
  int problems = 0;
  for (std::size_t i = 0; i < cat.size(); ++i) {
    const auto& e = cat[i];
    if (!e.meta.id || !e.meta.id[0]) {
      err << "entry[" << i << "]: empty id\n";
      ++problems;
      continue;
    }
    if (!seen.insert(e.meta.id).second) {
      err << "duplicate id: " << e.meta.id << "\n";
      ++problems;
    }
    if (!e.meta.title || !e.meta.title[0]) {
      err << e.meta.id << ": empty title\n";
      ++problems;
    }
    if (!e.run) {
      err << e.meta.id << ": null run function\n";
      ++problems;
    }
    if (!e.meta.tiers || !e.meta.tiers[0]) {
      err << e.meta.id << ": empty tiers\n";
      ++problems;
    }
  }
  if (problems == 0) return {};
  err << "validate_catalog: " << problems << " problem(s)\n";
  return err.str();
}

std::vector<std::string> list_ids(const std::vector<StrategyEntry>& cat) {
  std::vector<std::string> ids;
  ids.reserve(cat.size());
  for (const auto& e : cat) {
    ids.emplace_back(e.meta.id ? e.meta.id : "");
  }
  return ids;
}

int index_of(const std::vector<StrategyEntry>& cat, std::string_view id) {
  for (std::size_t i = 0; i < cat.size(); ++i) {
    if (cat[i].meta.id && id == cat[i].meta.id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace strategies
