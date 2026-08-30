#include "cs2/signatures.hpp"

#include <sstream>
#include <string>

namespace cs2 {

PatternScanner::ParsedPattern PatternScanner::parse_pattern(std::string_view hex) const {
  ParsedPattern parsed;
  std::istringstream tokens{std::string(hex)};
  std::string token;
  while (tokens >> token) {
    if (token == "?" || token == "??") {
      parsed.bytes.push_back(0);
      parsed.mask.push_back(false);
      continue;
    }
    if (token.size() != 2) return {};
    unsigned int value = 0;
    std::istringstream byte_stream(token);
    byte_stream >> std::hex >> value;
    if (byte_stream.fail() || value > 0xff) return {};
    parsed.bytes.push_back(static_cast<std::uint8_t>(value));
    parsed.mask.push_back(true);
  }
  return parsed;
}

bool PatternScanner::match_pattern(const std::uint8_t* memory, std::size_t size,
                                   std::size_t offset,
                                   std::string_view pattern_hex) const {
  const ParsedPattern pattern = parse_pattern(pattern_hex);
  if (pattern.bytes.empty() || offset > size || pattern.bytes.size() > size - offset)
    return false;
  for (std::size_t i = 0; i < pattern.bytes.size(); ++i) {
    if (pattern.mask[i] && memory[offset + i] != pattern.bytes[i]) return false;
  }
  return true;
}

PatternScanResult PatternScanner::scan_one(std::string_view name,
                                           std::uint64_t game_memory_base,
                                           std::size_t game_memory_size,
                                           const std::uint8_t* game_memory) {
  const auto* pattern = db_.find(name);
  if (!pattern || !game_memory) return {name, false, 0, 0};
  const ParsedPattern parsed = parse_pattern(pattern->bytes_hex);
  if (parsed.bytes.empty() || parsed.bytes.size() > game_memory_size)
    return {pattern->name, false, 0, 0};
  for (std::size_t offset = 0; offset <= game_memory_size - parsed.bytes.size(); ++offset) {
    bool matches = true;
    for (std::size_t i = 0; i < parsed.bytes.size(); ++i) {
      if (parsed.mask[i] && game_memory[offset + i] != parsed.bytes[i]) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return {pattern->name, true, game_memory_base + offset, offset};
    }
  }
  return {pattern->name, false, 0, 0};
}

std::vector<PatternScanResult> PatternScanner::scan_all(
    std::uint64_t game_memory_base, std::size_t game_memory_size,
    const std::uint8_t* game_memory) {
  std::vector<PatternScanResult> results;
  results.reserve(static_cast<std::size_t>(db_.total_count()));
  for (const auto& category : db_.categories) {
    for (int i = 0; i < category.count; ++i) {
      const auto& pattern = category.patterns[i];
      const ParsedPattern parsed = parse_pattern(pattern.bytes_hex);
      PatternScanResult result{pattern.name, false, 0, 0};
      if (game_memory && !parsed.bytes.empty() && parsed.bytes.size() <= game_memory_size) {
        for (std::size_t offset = 0; offset <= game_memory_size - parsed.bytes.size(); ++offset) {
          bool matches = true;
          for (std::size_t i = 0; i < parsed.bytes.size(); ++i) {
            if (parsed.mask[i] && game_memory[offset + i] != parsed.bytes[i]) {
              matches = false;
              break;
            }
          }
          if (matches) {
            result = {pattern.name, true, game_memory_base + offset, offset};
            break;
          }
        }
      }
      results.push_back(result);
    }
  }
  return results;
}

}  // namespace cs2
