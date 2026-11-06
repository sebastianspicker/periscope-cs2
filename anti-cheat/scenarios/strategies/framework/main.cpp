// main.cpp — strategy_lab CLI:
//   list [--family <F>] [--tier <T>] | stats | all [--quiet] [--family <F>]
//   [--tier <T>] | run <id> | attach | mode.
// Dual-mode dispatch: strategies run against real CS2 and/or sim::World.
//
// Educational design:
//   - `run` defaults to HYBRID mode: tries real CS2, falls back to sim.
//   - `attach` finds and inspects a real CS2 process.
//   - `mode` displays or sets the runtime mode (real/sim/hybrid).
//   - `list` shows all strategies; T0/T1 strategies support real mode.
//   - `stats` prints a summary table (families, tiers, real-capable).
//
// Quick start:
//   ./build/strategy_lab attach              # Check if CS2 is running
//   ./build/strategy_lab run 01_external_rpm # Dual mode (real + sim)
//   ./build/strategy_lab mode sim            # Force sim-only mode
//   LR_MODE=real ./build/strategy_lab run 01_external_rpm  # Env override

#include "strategies/framework.hpp"

#include <cstdio>
#include <cstring>
#include <string_view>

#ifndef LR_HAS_REAL_PLATFORM
#define LR_HAS_REAL_PLATFORM 0
#endif


static void usage(const char* argv0) {
  std::printf(
      "Usage:\n"
      "  %s list [--family <F>] [--tier <T>]   List strategy pairs (filtered)\n"
      "  %s stats                               Print catalog summary table\n"
      "  %s all [--quiet] [--family <F>] [--tier <T>]\n"
      "                                         Run all (or filtered) strategy pairs (sim mode)\n"
      "  %s run <strategy_id>     Run one strategy (hybrid: real CS2 + sim fallback)\n"
      "  %s attach                Try to find and inspect the real CS2 process\n"
      "  %s mode [real|sim|hybrid] Show or set the runtime execution mode\n"
      "\n"
      "Filters (case-insensitive):\n"
      "  --family <F>    Delivery | Feature | Evasion | Detection | Structural\n"
      "  --tier <T>      T0 | T1 | T2 | T3 | T4  (crosscutting 'all' matches any tier)\n"
      "\n"
      "Environment:\n"
      "  LR_MODE=real|sim|hybrid   Force execution mode\n"
      "  LR_VERBOSE=0|1           Suppress or enable verbose output\n"
      "\n"
      "Examples:\n"
      "  %s stats                  # Show family/tier/real-capable counts\n"
      "  %s list --family Evasion  # Only evasion pairs\n"
      "  %s all --tier T0          # Run only T0 pairs (sim mode)\n"
      "  %s run 01_external_rpm    # Hybrid: real CS2 + sim comparison\n"
      "  %s mode sim               # Sim-only (no CS2 required)\n"
      "  %s attach                 # Find CS2, print process info\n",
      argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0,
      argv0, argv0);
}

// ── Filter helpers ─────────────────────────────────────────────────
namespace {

// Canonicalize a family name (case-insensitive); nullptr if not a family.
const char* canonical_family(const char* s) {
  for (int i = 0; i < 5; ++i) {
    const auto f = static_cast<strategies::Family>(i);
    if (strategies::iequals(strategies::family_name(f), s)) {
      return strategies::family_name(f);
    }
  }
  return nullptr;
}

// Canonicalize a tier name to "T0".."T4"; nullptr if not a valid tier.
const char* canonical_tier(const char* s) {
  static const char* const kTiers[] = {"T0", "T1", "T2", "T3", "T4"};
  for (const char* t : kTiers) {
    if (strategies::iequals(t, s)) return t;
  }
  return nullptr;
}

// Print an error listing valid family names.
void print_unknown_family(const char* s) {
  std::fprintf(stderr, "Unknown family: %s\nValid families: ", s);
  for (int i = 0; i < 5; ++i) {
    if (i > 0) std::fprintf(stderr, ", ");
    std::fprintf(stderr, "%s",
                 strategies::family_name(static_cast<strategies::Family>(i)));
  }
  std::fprintf(stderr, "\n");
}

void print_unknown_tier(const char* s) {
  std::fprintf(stderr,
               "Unknown tier: %s\nValid tiers: T0, T1, T2, T3, T4\n", s);
}

}  // namespace

// ── CLI entry ─────────────────────────────────────────────────────
int main(int argc, char** argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }

  const std::string_view cmd = argv[1];

  // ── list ────────────────────────────────────────────────────────
  if (cmd == "list") {
    const char* family = nullptr;
    const char* tier = nullptr;
    for (int i = 2; i < argc; ++i) {
      if (std::strcmp(argv[i], "--family") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "list: --family requires a value\n");
          usage(argv[0]);
          return 2;
        }
        family = canonical_family(argv[++i]);
        if (!family) {
          print_unknown_family(argv[i]);
          return 2;
        }
      } else if (std::strcmp(argv[i], "--tier") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "list: --tier requires a value\n");
          usage(argv[0]);
          return 2;
        }
        tier = canonical_tier(argv[++i]);
        if (!tier) {
          print_unknown_tier(argv[i]);
          return 2;
        }
      } else {
        std::fprintf(stderr, "list: unknown argument: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    for (const auto& e : strategies::catalog()) {
      if (family &&
          !strategies::iequals(strategies::family_name(e.meta.family),
                               family)) {
        continue;
      }
      if (tier) {
        const char* t = e.meta.tiers;
        const bool cross = t && std::strstr(t, "all");
        if (!cross && (!t || !std::strstr(t, tier))) continue;
      }
      const char* real_tag = strategies::supports_real_mode(e.meta)
                                 ? "[real-capable]"
                                 : "[sim-only]    ";
      std::printf("%-22s %s [%s] %s\n", e.meta.id, real_tag,
                  e.meta.tiers, e.meta.title);
      std::printf("%-22s  red: %s\n", "", e.meta.red_one_liner);
      std::printf("%-22s  blue: %s\n\n", "", e.meta.blue_one_liner);
    }
    return 0;
  }

  // ── stats ───────────────────────────────────────────────────────
  if (cmd == "stats") {
    const auto& cat = strategies::catalog();
    int by_family[5] = {};
    int by_tier[5] = {};
    int crosscutting = 0;
    int real_capable = 0;
    static const char* const kTierNames[5] = {"T0", "T1", "T2", "T3", "T4"};

    for (const auto& e : cat) {
      const int fi = static_cast<int>(e.meta.family);
      if (fi >= 0 && fi < 5) ++by_family[fi];
      if (strategies::supports_real_mode(e.meta)) ++real_capable;
      const char* t = e.meta.tiers;
      if (!t) continue;
      if (std::strstr(t, "all")) {
        ++crosscutting;
      } else {
        for (int i = 0; i < 5; ++i) {
          if (std::strstr(t, kTierNames[i])) ++by_tier[i];
        }
      }
    }

    std::printf("%-12s %6d\n", "total", static_cast<int>(cat.size()));
    std::printf("\n%-12s %6s\n", "family", "count");
    for (int i = 0; i < 5; ++i) {
      std::printf("%-12s %6d\n",
                  strategies::family_name(static_cast<strategies::Family>(i)),
                  by_family[i]);
    }
    std::printf("\n%-12s %6s\n", "tier", "count");
    for (int i = 0; i < 5; ++i) {
      std::printf("%-12s %6d\n", kTierNames[i], by_tier[i]);
    }
    std::printf("%-12s %6d\n", "all", crosscutting);
    std::printf("\n%-12s %6d\n", "real-capable", real_capable);
    return 0;
  }

  // ── all ─────────────────────────────────────────────────────────
  if (cmd == "all") {
    bool verbose = true;
    const char* family = nullptr;
    const char* tier = nullptr;
    for (int i = 2; i < argc; ++i) {
      if (std::strcmp(argv[i], "--quiet") == 0) {
        verbose = false;
      } else if (std::strcmp(argv[i], "--family") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "all: --family requires a value\n");
          usage(argv[0]);
          return 2;
        }
        family = canonical_family(argv[++i]);
        if (!family) {
          print_unknown_family(argv[i]);
          return 2;
        }
      } else if (std::strcmp(argv[i], "--tier") == 0) {
        if (i + 1 >= argc) {
          std::fprintf(stderr, "all: --tier requires a value\n");
          usage(argv[0]);
          return 2;
        }
        tier = canonical_tier(argv[++i]);
        if (!tier) {
          print_unknown_tier(argv[i]);
          return 2;
        }
      } else {
        std::fprintf(stderr, "all: unknown argument: %s\n", argv[i]);
        usage(argv[0]);
        return 2;
      }
    }

    if (family || tier) {
      std::printf("[mode] Running filtered strategies in SIM mode (deterministic)\n");
      strategies::CatalogStats stats;
      const int rc = strategies::run_filtered(family, tier, verbose, &stats);
      std::printf("\nFiltered: %d strategies, %d failure(s)\n", stats.total,
                  stats.failures);
      return rc;
    }
    std::printf("[mode] Running all strategies in SIM mode (deterministic)\n");
    return strategies::run_all(verbose);
  }

  // ── run ─────────────────────────────────────────────────────────
  if (cmd == "run" && argc >= 3) {
    const auto* entry = strategies::find(argv[2]);
    if (!entry) {
      std::fprintf(stderr, "Unknown strategy: %s\n", argv[2]);
      return 2;
    }

    // Dual mode (hybrid default): try real CS2 + sim comparison
    auto result = strategies::run_dual(argv[2], true);

    // Show summary
    std::printf("\n═══ RESULT ═══\n");
    std::printf("Mode: %s\n", result.mode_used);
    std::printf("%s\n", result.describe().c_str());

    // Pass condition: blue detected/mitigated OR red didn't achieve
    const auto& effective = result.sim;
    const bool pass = effective.blue_detected ||
                      effective.blue_mitigated ||
                      !effective.red_achieved;
    return pass ? 0 : 1;
  }

  // ── attach ──────────────────────────────────────────────────────
  if (cmd == "attach") {
    std::fprintf(stderr, "attach unavailable: real platform support is not built.\n");
    return 1;
  }

  // ── mode ────────────────────────────────────────────────────────
  if (cmd == "mode") {
    if (argc >= 3 && std::string_view(argv[2]) != "sim") {
      std::fprintf(stderr, "Unsupported mode: %s (simulation build supports only sim)\n",
                   argv[2]);
      return 2;
    }
    std::printf("sim\n");
    return 0;
  }

  usage(argv[0]);
  return 2;
}
