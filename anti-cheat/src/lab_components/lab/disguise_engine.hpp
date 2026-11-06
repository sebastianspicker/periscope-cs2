#pragma once

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lab {

struct DisguiseAttributes {
  std::string process_name;
  std::string window_class;
  std::string mutex_name;
  std::string signer;  // simulated code-signing subject
  double memory_read_rate_kbps = 0.0;
  std::size_t working_set_kb = 0;
  bool looks_reputable = false;
  bool hw_monitor_family = false;  // RTSS / Afterburner / NV family
};

struct DisguiseVerifyReport {
  bool process_name_ok = false;
  bool window_class_ok = false;
  bool working_set_ok = false;
  bool reputation_ok = false;
  bool hw_flag_ok = false;
  int signals_matched = 0;
  int signals_total = 5;
  bool verified = false;  // multi-signal: ≥3 matched
  std::vector<std::string> reasons;
  std::string detail;
};

class DisguiseEngine {
 public:
  static DisguiseAttributes attributes_for(ac::DisguiseProfile profile);
  static std::string_view profile_name(ac::DisguiseProfile profile);

  static void apply_to_world(sim::World& world, std::uint32_t cheat_pid,
                             ac::DisguiseProfile profile);

  static bool verify_disguise(const sim::World& world, std::uint32_t cheat_pid,
                              ac::DisguiseProfile expected);

  // Multi-signal verification used by blue lessons.
  static DisguiseVerifyReport verify_report(const sim::World& world,
                                            std::uint32_t cheat_pid,
                                            ac::DisguiseProfile expected);
};

}  // namespace lab
