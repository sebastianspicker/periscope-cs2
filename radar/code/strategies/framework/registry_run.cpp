// registry_run.cpp — Strategy pair execution: run_one, run_all, run_filtered.
// Split from the original 610-line monolith registry.cpp.

#include "strategies/framework.hpp"
#include "sim/world.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace strategies {

// run_one: Execute one pair on a fresh World; 0 if blue wins or red fails.
int run_one(std::string_view id, bool verbose) {
  const auto* e = find(id);
  if (!e) {
    std::fprintf(stderr, "unknown strategy: %.*s\n", static_cast<int>(id.size()),
                 id.data());
    return 2;
  }
  sim::Narrator n;
  if (verbose) {
    std::printf("\n############################\n# %s — %s\n# RED: %s\n# BLUE: %s\n############################\n",
                e->meta.id, e->meta.title, e->meta.red_one_liner,
                e->meta.blue_one_liner);
  }
  auto world = sim::make_arena();
  auto res = e->run(world, n);
  if (verbose) {
    std::printf("    red_achieved=%d blue_detected=%d blue_mitigated=%d\n    %s\n",
                res.red_achieved ? 1 : 0, res.blue_detected ? 1 : 0,
                res.blue_mitigated ? 1 : 0, res.summary.c_str());
  }
  const bool ok = res.blue_detected || res.blue_mitigated || !res.red_achieved;
  return ok ? 0 : 1;
}

// run_all: Run every catalog entry; return 0 only if all pass.
int run_all(bool verbose) {
  int fails = 0;
  for (const auto& e : catalog()) {
    const int rc = run_one(e.meta.id, verbose);
    if (rc != 0) {
      std::fprintf(stderr, "FAIL %s\n", e.meta.id);
      ++fails;
    }
  }
  std::printf("\nCatalog: %zu strategies, %d failure(s)\n", catalog().size(), fails);
  return fails == 0 ? 0 : 1;
}

// run_filtered: run catalog subset by family name and/or tier substring.
int run_filtered(const char* family_filter, const char* tier_filter, bool verbose,
                 CatalogStats* out_stats) {
  CatalogStats stats;
  int fails = 0;
  for (const auto& e : catalog()) {
    if (family_filter && family_filter[0] &&
        !iequals(family_name(e.meta.family), family_filter)) continue;
    if (tier_filter && tier_filter[0]) {
      if (!e.meta.tiers) continue;
      // Crosscutting strategies marked "all" apply to every tier.
      if (!std::strstr(e.meta.tiers, "all") &&
          !std::strstr(e.meta.tiers, tier_filter)) continue;
    }
    ++stats.total;
    const int fi = static_cast<int>(e.meta.family);
    if (fi >= 0 && fi < 5) ++stats.by_family[fi];
    sim::Narrator n;
    auto world = sim::make_arena();
    auto res = e.run(world, n);
    if (res.red_achieved) ++stats.red_achieved;
    if (res.blue_detected) ++stats.blue_detected;
    if (res.blue_mitigated) ++stats.blue_mitigated;
    const bool ok = res.blue_detected || res.blue_mitigated || !res.red_achieved;
    if (!ok) { ++fails; ++stats.failures; if (verbose) std::fprintf(stderr, "FAIL %s\n", e.meta.id); }
    else if (verbose) std::printf("ok %s red=%d det=%d mit=%d\n", e.meta.id,
                    res.red_achieved ? 1 : 0, res.blue_detected ? 1 : 0,
                    res.blue_mitigated ? 1 : 0);
  }
  stats.detail = "run_filtered total=" + std::to_string(stats.total) +
                 " fails=" + std::to_string(stats.failures);
  if (out_stats) *out_stats = stats;
  return fails == 0 ? 0 : 1;
}

}  // namespace strategies
