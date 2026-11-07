// radar_offsets_test.cpp: R9: runtime offsets fallback parser and merger.

#include "radar/offset_overrides.hpp"

#include <cstdio>
#include <string>

namespace {

using radar_domain::OffsetSource;
using radar_domain::OffsetResolutionReport;
using radar_domain::merge_offsets;
using radar_domain::offset_source_name;
using radar_domain::parse_offset_file;
using radar_domain::parse_offset_line;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

void test_parse_line() {
  std::string key;
  std::int64_t value = 0;
  std::string error;

  check(parse_offset_line("offsets.client.dwEntityList=30500696", key, value,
                          error),
        "canonical line parses");
  check(key == "offsets.client.dwEntityList" && value == 30500696,
        "canonical line values");

  check(parse_offset_line("  offsets.client.m_iHealth = 844  ", key, value,
                          error),
        "whitespace tolerated");
  check(key == "offsets.client.m_iHealth" && value == 844, "ws values");

  // Comments and blank lines report false with an empty error: they are
  // skipped silently by the lenient file parser.
  std::string skip_error;
  check(!parse_offset_line("# comment", key, value, skip_error) &&
            skip_error.empty(),
        "comment skipped");
  check(!parse_offset_line("", key, value, skip_error) && skip_error.empty(),
        "blank skipped");
  check(!parse_offset_line("// note", key, value, skip_error) &&
            skip_error.empty(),
        "c-comment skipped");

  check(!parse_offset_line("dwEntityList=30500696", key, value, error) &&
            error.find("offsets.") != std::string::npos,
        "bad prefix rejected");
  check(!parse_offset_line("offsets.client.m_iHealth=", key, value, error),
        "empty value rejected");
  check(!parse_offset_line("offsets.client.m_iHealth=84a", key, value, error),
        "non-numeric rejected");
  check(!parse_offset_line("offsets.client.m_iHealth", key, value, error),
        "missing equals rejected");
  check(!parse_offset_line("=844", key, value, error),
        "missing key rejected");
}

void test_precedence_merge() {
  std::map<std::string, std::int64_t> builtin{
      {"offsets.client.dwEntityList", 100},
      {"offsets.client.dwLocalPlayerPawn", 200},
  };
  std::map<std::string, std::int64_t> file_map{
      {"offsets.client.dwEntityList", 150},
      {"offsets.client.m_iTeamNum", 1003},
  };
  std::map<std::string, std::int64_t> cli{
      {"offsets.client.dwEntityList", 175},
  };

  OffsetResolutionReport report;
  merge_offsets(builtin, file_map, cli, report);

  check(report.resolved.size() == 3, "union of keys");
  check(report.resolved.at("offsets.client.dwEntityList").value == 175 &&
            report.resolved.at("offsets.client.dwEntityList").source ==
                OffsetSource::Cli,
        "cli wins conflict");
  check(report.resolved.at("offsets.client.dwLocalPlayerPawn").value == 200 &&
            report.resolved.at("offsets.client.dwLocalPlayerPawn").source ==
                OffsetSource::Builtin,
        "builtin survives");
  check(report.resolved.at("offsets.client.m_iTeamNum").source ==
            OffsetSource::File,
        "file adds key");
  check(report.overridden == 2, "two keys overridden");
  check(std::string(offset_source_name(OffsetSource::Cli)) == "cli",
        "source label");
  check(report.errors.empty(), "no errors in clean merge");
}

void test_lenient_file_parse() {
  OffsetResolutionReport report;
  std::map<std::string, std::int64_t> parsed;
  parse_offset_file(
      {
          "# auto offsets",
          "offsets.client.dwEntityList=30500696",
          "garbage line",
          "offsets.client.m_iHealth=844",
          "",
      },
      parsed, report);

  check(parsed.size() == 2, "two valid lines parsed");
  check(report.errors.size() == 1, "one malformed line recorded");
  check(report.errors[0].find("not an integer") == std::string::npos,
        "error text for garbage line");
}

}  // namespace

int main() {
  test_parse_line();
  test_precedence_merge();
  test_lenient_file_parse();

  if (failures != 0) {
    std::printf("%d offsets check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_offsets_test: all checks passed\n");
  return 0;
}
