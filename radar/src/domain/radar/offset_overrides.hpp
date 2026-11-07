// offset_overrides.hpp: R9: runtime offsets fallback (pure parser/merger).
//
// Adapted from the research ledger (R9): staromodny/miata reads a plain
// `offsets.txt` at runtime (`offsets.client.dwEntityList=30500696` lines) so
// a game update never requires a recompile; cs2-dma resolves runtime offsets
// with an explicit precedence chain and reports every key's source.
//
// This module is the portable half: parsing, precedence merge and the
// resolver report. Wiring a live source to it is an adapter concern and stays
// out of the domain.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace radar_domain {

enum class OffsetSource: std::uint8_t {
  Builtin = 0,
  File = 1,
  Cli = 2,
};

struct ResolvedOffset {
  std::int64_t value = 0;
  OffsetSource source = OffsetSource::Builtin;
};

struct OffsetResolutionReport {
  std::map<std::string, ResolvedOffset> resolved;
  std::vector<std::string> errors;  // one line per rejected input line
  std::size_t overridden = 0;       // keys whose source is not Builtin
};

// Parses one `offsets.client.<key>=<int>` line. `#`/`//` comments and blank
// lines are skipped. Returns false (with a reason in `error_out`) for
// malformed lines; the caller decides strict vs lenient handling.
bool parse_offset_line(const std::string& line, std::string& key_out,
                       std::int64_t& value_out, std::string& error_out);

// Merges offset maps in precedence order: builtin < file < cli. Later maps
// win on conflicts. Keys present only in later maps are added; nothing is
// silently dropped.
void merge_offsets(const std::map<std::string, std::int64_t>& builtin,
                   const std::map<std::string, std::int64_t>& file_map,
                   const std::map<std::string, std::int64_t>& cli,
                   OffsetResolutionReport& report);

// Parses a whole offsets file (lenient: malformed lines are recorded in
// report.errors and skipped, matching the runtime-fallback use case).
void parse_offset_file(const std::vector<std::string>& lines,
                       std::map<std::string, std::int64_t>& out,
                       OffsetResolutionReport& report);

// Human-readable source label for reports ("builtin", "file", "cli").
const char* offset_source_name(OffsetSource source);

}  // namespace radar_domain
