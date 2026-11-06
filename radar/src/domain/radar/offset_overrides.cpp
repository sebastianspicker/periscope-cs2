// offset_overrides.cpp — R9 runtime offsets fallback. See offset_overrides.hpp.

#include "radar/offset_overrides.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace radar_domain {
namespace {

std::string trim(const std::string& text) {
  const char* ws = " \t\r\n";
  const std::size_t begin = text.find_first_not_of(ws);
  if (begin == std::string::npos) {
    return {};
  }
  const std::size_t end = text.find_last_not_of(ws);
  return text.substr(begin, end - begin + 1);
}

const char* source_name(OffsetSource source) {
  switch (source) {
    case OffsetSource::Builtin: return "builtin";
    case OffsetSource::File: return "file";
    case OffsetSource::Cli: return "cli";
  }
  return "builtin";
}

void apply_map(const std::map<std::string, std::int64_t>& values,
               OffsetSource source, OffsetResolutionReport& report) {
  for (const auto& [key, value] : values) {
    auto it = report.resolved.find(key);
    if (it == report.resolved.end()) {
      report.resolved.emplace(key, ResolvedOffset{value, source});
    } else {
      it->second.value = value;
      it->second.source = source;
      ++report.overridden;
    }
  }
}

}  // namespace

bool parse_offset_line(const std::string& line, std::string& key_out,
                       std::int64_t& value_out, std::string& error_out) {
  const std::string trimmed = trim(line);
  if (trimmed.empty() || trimmed[0] == '#' || trimmed.rfind("//", 0) == 0) {
    return false;  // skipped silently by callers
  }

  const std::size_t eq = trimmed.find('=');
  if (eq == std::string::npos || eq == 0 || eq + 1 == trimmed.size()) {
    error_out = "missing '=' separator";
    return false;
  }

  const std::string key = trim(trimmed.substr(0, eq));
  const std::string value_text = trim(trimmed.substr(eq + 1));
  if (key.rfind("offsets.", 0) != 0) {
    error_out = "key does not start with 'offsets.': " + key;
    return false;
  }
  if (value_text.empty()) {
    error_out = "empty value";
    return false;
  }

  errno = 0;
  char* end = nullptr;
  const long long parsed = std::strtoll(value_text.c_str(), &end, 10);
  if (errno != 0 || end == value_text.c_str() ||
      end != value_text.c_str() + value_text.size()) {
    error_out = "value is not an integer: " + value_text;
    return false;
  }

  key_out = key;
  value_out = static_cast<std::int64_t>(parsed);
  error_out.clear();
  return true;
}

void parse_offset_file(const std::vector<std::string>& lines,
                       std::map<std::string, std::int64_t>& out,
                       OffsetResolutionReport& report) {
  for (const std::string& line : lines) {
    std::string key;
    std::int64_t value = 0;
    std::string error;
    if (parse_offset_line(line, key, value, error)) {
      out[key] = value;
    } else if (!error.empty()) {
      report.errors.push_back(error);
    }
  }
}

void merge_offsets(const std::map<std::string, std::int64_t>& builtin,
                   const std::map<std::string, std::int64_t>& file_map,
                   const std::map<std::string, std::int64_t>& cli,
                   OffsetResolutionReport& report) {
  apply_map(builtin, OffsetSource::Builtin, report);
  apply_map(file_map, OffsetSource::File, report);
  apply_map(cli, OffsetSource::Cli, report);
}

const char* offset_source_name(OffsetSource source) {
  return source_name(source);
}

}  // namespace radar_domain
