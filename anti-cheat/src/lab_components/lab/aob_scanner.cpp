// aob_scanner.cpp -- simulation-only CS2 AOB scanning for lab exercises.

#include "lab/aob_scanner.hpp"

#include <cctype>
#include <cstring>
#include <utility>

namespace lab {
namespace {

constexpr double kNanosecondsPerScannedByte = 1.0;

int hex_value(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  value = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
  return value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1;
}

}  // namespace

AobScanner::AobScanner() { reset(); }

void AobScanner::set_memory_region(const std::uint8_t* memory, std::size_t size,
                                   std::uint64_t base_address) {
  memory_ = memory;
  memory_size_ = size;
  base_address_ = base_address;
  window_begin_ = 0;
  window_end_ = 0;
  reset();
}

void AobScanner::set_scan_window(std::size_t rel_begin, std::size_t rel_end) {
  window_begin_ = rel_begin;
  window_end_ = rel_end;
}

void AobScanner::clear_scan_window() {
  window_begin_ = 0;
  window_end_ = 0;
}

std::size_t AobScanner::effective_begin() const {
  if (window_end_ == 0) return 0;
  return window_begin_ < memory_size_ ? window_begin_ : memory_size_;
}

std::size_t AobScanner::effective_end() const {
  if (window_end_ == 0) return memory_size_;
  return window_end_ < memory_size_ ? window_end_ : memory_size_;
}

AobScanner::ParsedPattern AobScanner::parse_pattern(std::string_view hex) const {
  ParsedPattern parsed;
  for (std::size_t pos = 0; pos < hex.size();) {
    while (pos < hex.size() && std::isspace(static_cast<unsigned char>(hex[pos]))) ++pos;
    if (pos >= hex.size()) break;

    const char first = hex[pos++];
    if (first == '?') {
      if (pos < hex.size() && hex[pos] == '?') ++pos;
      parsed.bytes.push_back(0);
      parsed.mask.push_back(false);
      continue;
    }
    if (pos >= hex.size()) return {};
    const int high = hex_value(first);
    const int low = hex_value(hex[pos++]);
    if (high < 0 || low < 0) return {};
    parsed.bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    parsed.mask.push_back(true);
  }
  return parsed;
}

bool AobScanner::match_at_offset(std::size_t offset,
                                 std::string_view pattern_hex) const {
  const auto pattern = parse_pattern(pattern_hex);
  if (!memory_ || !pattern.valid() || offset > memory_size_ ||
      pattern.bytes.size() > memory_size_ - offset) {
    return false;
  }
  for (std::size_t i = 0; i < pattern.bytes.size(); ++i) {
    if (pattern.mask[i] && memory_[offset + i] != pattern.bytes[i]) return false;
  }
  return true;
}

ScanOperation AobScanner::scan_parsed(std::string_view name,
                                      const ParsedPattern& parsed) const {
  ScanOperation operation;
  operation.pattern_name = std::string(name);
  operation.scan_base = base_address_;
  const std::size_t begin = effective_begin();
  const std::size_t end = effective_end();
  operation.scan_size = end > begin ? end - begin : 0;

  if (!memory_ || !parsed.valid() || begin >= end ||
      parsed.bytes.size() > end - begin) {
    return operation;
  }

  std::size_t scanned_bytes = 0;
  std::uint64_t first_addr = 0;
  int matches = 0;

  for (std::size_t offset = begin; offset + parsed.bytes.size() <= end; ++offset) {
    bool matches_here = true;
    for (std::size_t i = 0; i < parsed.bytes.size(); ++i) {
      if (parsed.mask[i] && memory_[offset + i] != parsed.bytes[i]) {
        matches_here = false;
        break;
      }
    }
    if (!matches_here) continue;

    ++matches;
    if (matches == 1) {
      first_addr = base_address_ + offset;
      operation.offset_from_base = offset;
      scanned_bytes = (offset - begin) + parsed.bytes.size();
      if (!find_all_) break;
    }
  }

  if (matches > 0) {
    operation.found = true;
    operation.resolved_addr = first_addr;
    operation.match_count = matches;
  } else {
    scanned_bytes = end - begin;
  }

  operation.scan_time_ms =
      static_cast<double>(scanned_bytes) * kNanosecondsPerScannedByte / 1'000'000.0;
  return operation;
}

ScanOperation AobScanner::scan_one(const cs2::SignaturePattern& pattern) const {
  return scan_parsed(pattern.name, parse_pattern(pattern.bytes_hex));
}

ScanOperation AobScanner::scan_raw(std::string_view pattern_name,
                                   std::string_view pattern_hex) {
  AobScanReport report;
  auto op = scan_parsed(pattern_name, parse_pattern(pattern_hex));
  report.operations.push_back(op);
  finish_report(report, "ad-hoc raw AOB scan complete");
  last_report_ = std::move(report);
  return last_report_.operations.front();
}

void AobScanner::finish_report(AobScanReport& report, std::string detail) const {
  report.total_patterns = db_.total_count();
  report.attempted_scans = static_cast<int>(report.operations.size());
  report.bytes_examined = 0;
  for (const auto& operation : report.operations) {
    report.total_scan_time_ms += operation.scan_time_ms;
    report.bytes_examined += operation.scan_size;
    if (operation.found) {
      ++report.found_count;
    } else {
      ++report.failed_count;
    }
  }
  report.detail = std::move(detail);
}

std::uint64_t AobScanner::scan_pattern(std::string_view pattern_name) {
  AobScanReport report;
  if (const auto* pattern = db_.find(pattern_name)) {
    report.operations.push_back(scan_one(*pattern));
  }
  finish_report(report, report.operations.empty() ? "pattern name not in CS2 database"
                                                  : "single simulated AOB scan complete");
  last_report_ = std::move(report);
  return last_report_.operations.empty() ? 0 : last_report_.operations.front().resolved_addr;
}

std::vector<ScanOperation> AobScanner::scan_category(std::string_view category_name) {
  AobScanReport report;
  if (const auto* category = db_.category(category_name)) {
    for (int i = 0; i < category->count; ++i) {
      report.operations.push_back(scan_one(category->patterns[i]));
    }
  }
  finish_report(report, "simulated category AOB scan complete");
  last_report_ = std::move(report);
  return last_report_.operations;
}

AobScanReport AobScanner::scan_all() {
  AobScanReport report;
  for (const auto& category : db_.categories) {
    for (int i = 0; i < category.count; ++i) {
      report.operations.push_back(scan_one(category.patterns[i]));
    }
  }
  finish_report(report, "full simulated CS2 AOB scan complete");
  last_report_ = report;
  return last_report_;
}

AobScanReport AobScanner::scan_entity_radar_patterns() {
  static constexpr std::string_view kNames[] = {
      "GETENTITYBYINDEX", "GETBASEENTITY", "GETLOCALPLAYERCONTROLLER",
      "GETLOCALPAWN", "GETPLAYERCONTROLLER", "GETENTITYHANDLE", "PENTITYLIST",
      "PENTITYSYSTEM", "PVIEWMATRIX", "PVIEWRENDER", "FINDENTITYBYCLASSNAME",
      "GETABSORIGIN", "GETBONEPOSITIONBYNAME", "GETTRANSFORMSFORHITBOXLIST",
      "CALCWORLDSPACEBONES", "C_BASEENTITY_GETBONEIDBYNAME",
      "C_BASEENTITY_GETHITBOXSET",
  };
  AobScanReport report;
  for (const auto name : kNames) {
    if (const auto* pattern = db_.find(name)) {
      report.operations.push_back(scan_one(*pattern));
    }
  }
  finish_report(report, "simulated entity/radar AOB scan complete");
  last_report_ = report;
  return last_report_;
}

RipRelativeHit AobScanner::resolve_rip_relative(std::uint64_t instruction_addr,
                                                std::size_t disp_offset,
                                                std::size_t instr_len) const {
  RipRelativeHit hit;
  hit.instruction_addr = instruction_addr;
  if (!memory_ || instr_len < disp_offset + 4) return hit;
  if (instruction_addr < base_address_) return hit;
  const std::size_t rel = static_cast<std::size_t>(instruction_addr - base_address_);
  if (rel + disp_offset + 4 > memory_size_) return hit;

  std::int32_t disp = 0;
  std::memcpy(&disp, memory_ + rel + disp_offset, sizeof(disp));
  hit.displacement = disp;
  hit.target_addr = instruction_addr + static_cast<std::uint64_t>(instr_len) +
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(disp));
  hit.valid = true;
  return hit;
}

std::optional<std::uint64_t> AobScanner::resolve_target_from_match(
    std::uint64_t match_addr, std::size_t disp_offset,
    std::size_t instr_len) const {
  const auto hit = resolve_rip_relative(match_addr, disp_offset, instr_len);
  if (!hit.valid) return std::nullopt;
  return hit.target_addr;
}

void AobScanner::reset() { last_report_ = {}; }

}  // namespace lab
