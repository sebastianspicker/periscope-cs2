#pragma once

// Simulated AOB (Array of Bytes) / pattern scanner.
// Mirrors what real CS2 cheat engines use to resolve function addresses
// at runtime by scanning game memory for known byte signatures.

#include "ac/types.hpp"
#include "cs2/signatures.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lab {

// A single scan operation -- what was scanned, where, and the result.
struct ScanOperation {
  std::string pattern_name;
  std::uint64_t scan_base = 0;
  std::size_t scan_size = 0;
  std::uint64_t resolved_addr = 0;
  std::uint64_t offset_from_base = 0;
  double scan_time_ms = 0.0;
  bool found = false;
  int match_count = 0;  // how many matches if find-all mode
};

// Full scan report for telemetry/lessons.
struct AobScanReport {
  int total_patterns = 0;
  int attempted_scans = 0;
  int found_count = 0;
  int failed_count = 0;
  double total_scan_time_ms = 0.0;
  std::uint64_t bytes_examined = 0;
  std::vector<ScanOperation> operations;
  std::string detail;
};

// x64 rip-relative decode result (LEA/CALL/JMP-style E8/E9/48 8D).
struct RipRelativeHit {
  std::uint64_t instruction_addr = 0;
  std::uint64_t target_addr = 0;
  std::int32_t displacement = 0;
  bool valid = false;
};

// Simulated AOB scanner -- searches for patterns in a memory buffer.
class AobScanner {
 public:
  AobScanner();

  void set_memory_region(const std::uint8_t* memory, std::size_t size,
                         std::uint64_t base_address);

  // Optional window inside the current region (relative offsets).
  void set_scan_window(std::size_t rel_begin, std::size_t rel_end);
  void clear_scan_window();

  // When true, scan_one reports every match and sets match_count.
  void set_find_all(bool enabled) { find_all_ = enabled; }
  bool find_all() const { return find_all_; }

  std::uint64_t scan_pattern(std::string_view pattern_name);
  std::vector<ScanOperation> scan_category(std::string_view category_name);
  AobScanReport scan_all();
  AobScanReport scan_entity_radar_patterns();

  // Scan an ad-hoc IDA-style hex pattern not present in the CS2 DB.
  ScanOperation scan_raw(std::string_view pattern_name,
                         std::string_view pattern_hex);

  // Decode a 32-bit signed rip-relative displacement at instr+disp_offset.
  // Common layouts: CALL/JMP E8/E9 at +1; LEA r64, [rip+disp32] at +3.
  RipRelativeHit resolve_rip_relative(std::uint64_t instruction_addr,
                                      std::size_t disp_offset = 1,
                                      std::size_t instr_len = 5) const;

  // After a pattern hit, attempt rip-relative target resolution at match+offset.
  std::optional<std::uint64_t> resolve_target_from_match(
      std::uint64_t match_addr, std::size_t disp_offset = 1,
      std::size_t instr_len = 5) const;

  const AobScanReport& last_report() const { return last_report_; }
  std::uint64_t base_address() const { return base_address_; }
  std::size_t memory_size() const { return memory_size_; }
  bool has_memory() const { return memory_ != nullptr && memory_size_ > 0; }

  void reset();

 private:
  const std::uint8_t* memory_ = nullptr;
  std::size_t memory_size_ = 0;
  std::uint64_t base_address_ = 0;
  std::size_t window_begin_ = 0;
  std::size_t window_end_ = 0;  // exclusive; 0 means full region
  bool find_all_ = false;
  AobScanReport last_report_{};
  cs2::SignatureDatabase db_{cs2::SignatureDatabase::get()};

  bool match_at_offset(std::size_t offset, std::string_view pattern_hex) const;

  struct ParsedPattern {
    std::vector<std::uint8_t> bytes;
    std::vector<bool> mask;  // true = compare byte, false = wildcard
    bool valid() const { return !bytes.empty() && bytes.size() == mask.size(); }
  };
  ParsedPattern parse_pattern(std::string_view hex) const;
  ScanOperation scan_one(const cs2::SignaturePattern& pattern) const;
  ScanOperation scan_parsed(std::string_view name, const ParsedPattern& parsed) const;
  void finish_report(AobScanReport& report, std::string detail) const;
  std::size_t effective_begin() const;
  std::size_t effective_end() const;
};

}  // namespace lab
