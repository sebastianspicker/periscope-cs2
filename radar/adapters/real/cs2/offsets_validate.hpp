// offsets_validate.hpp — Pure functional validation of resolved offsets
// (assessment ledger E12).
//
// Freshness checks (snapshot mtime, dump timestamp) prove *when* an offset
// set was produced, not that it is *semantically sane*. This header classifies
// a live observation — the field values actually read through candidate
// offsets — into one of three states:
//
//   Ok         offsets produced plausible, self-consistent entity data
//   NoPlayers  offsets are probably fine; the walk simply saw no players
//              (empty server, lobby, main menu) — not an offset failure
//   Wrong      the offsets produced implausible data (bogus reads, values
//              outside sanity ranges) and must not be trusted
//
// The classifier is deliberately conservative: Wrong requires *evidence*
// (bogus rate above a threshold, or a plausible read with impossible values),
// because a wrong classification triggers a re-resolve which is itself an
// observable event.
//
// Pure and host-testable: no I/O, no platform headers.

#pragma once

#include <cstddef>
#include <cstdint>

namespace real::cs2::validate {

enum class OffsetsValidation {
  Ok,
  NoPlayers,
  Wrong,
};

/// Raw tallies from one live observation pass over the entity list.
struct Observation {
  int players_seen = 0;        ///< identity slots that resolved to a controller
  int pawns_resolved = 0;      ///< controller→pawn chains that resolved
  int plausible_fields = 0;    ///< field reads inside sanity ranges
  int bogus_fields = 0;        ///< field reads outside sanity ranges
  int failed_reads = 0;        ///< memory reads that returned an error
};

/// Sanity ranges (mirrors the memory-path gates in entity_layout.hpp).
struct SanityRanges {
  std::int32_t health_min = 0;
  std::int32_t health_max = 500;
  std::uint32_t team_min = 0;
  std::uint32_t team_max = 3;
};

/// More bogus than plausible fields ⇒ the offsets are misreadings.
inline constexpr double kMaxBogusRatio = 0.5;

inline OffsetsValidation classify(const Observation& obs) {
  // Not a single controller resolved anywhere: either the entity-list
  // pointer itself is wrong or there is genuinely nobody enumerated. Distinguish
  // by whether reads succeeded at all — systematic read failure with zero
  // resolutions is an offset/address problem, a clean walk with zero players
  // is a lobby.
  if (obs.players_seen == 0) {
    return obs.failed_reads > 0 ? OffsetsValidation::Wrong
                                : OffsetsValidation::NoPlayers;
  }
  if (obs.pawns_resolved == 0) {
    // Controllers resolve but no pawn chain does: the controller→pawn offset
    // or the handle→pawn math is wrong.
    return OffsetsValidation::Wrong;
  }
  const int total_fields = obs.plausible_fields + obs.bogus_fields;
  if (total_fields > 0) {
    const double bogus_ratio =
        static_cast<double>(obs.bogus_fields) / static_cast<double>(total_fields);
    if (bogus_ratio > kMaxBogusRatio) {
      return OffsetsValidation::Wrong;
    }
  }
  if (obs.plausible_fields == 0 && obs.bogus_fields == 0) {
    // Players and pawns resolved but no field observations were recorded —
    // the caller's walk never reached the field stage; treat as Ok only when
    // reads did not fail. Otherwise inconclusive data is treated as Wrong to
    // force a re-check.
    return obs.failed_reads > 0 ? OffsetsValidation::Wrong : OffsetsValidation::Ok;
  }
  return OffsetsValidation::Ok;
}

/// Does one observed (team, health) pair fall inside the sanity ranges?
inline bool field_plausible(std::uint32_t team_raw,
                            std::int32_t health,
                            const SanityRanges& ranges = {}) {
  const bool team_ok = team_raw >= ranges.team_min && team_raw <= ranges.team_max;
  const bool health_ok = health >= ranges.health_min && health <= ranges.health_max;
  return team_ok && health_ok;
}

}  // namespace real::cs2::validate
