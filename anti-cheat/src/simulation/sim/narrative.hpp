// narrative.hpp — terminal Narrator for red/blue lesson text (not the World model).
// Side tags + move/counter/result helpers for strategy_lab and demos.

#pragma once

#include <cstdio>
#include <string>
#include <string_view>

namespace sim {

// Narrative speaker: Red / Blue / System / Lesson.
enum class Side { Red, Blue, System, Lesson };

// Map Side to a short tag for Narrator prefixes.
inline const char* side_tag(Side s) {
  switch (s) {
    case Side::Red:
      return "RED ";
    case Side::Blue:
      return "BLUE";
    case Side::System:
      return "SYS ";
    case Side::Lesson:
      return "TIP ";
  }
  return "????";
}

/// Colored-ish plain narrative for terminal learning (no deps).
class Narrator {
 public:
  // Print one [SIDE] lesson line.
  void say(Side side, std::string_view msg) {
    std::printf("[%s] %.*s\n", side_tag(side), static_cast<int>(msg.size()),
                msg.data());
  }
  // Print a MOVE banner (title + why).
  void move(Side side, std::string_view title, std::string_view why) {
    std::printf("\n── %s MOVE: %.*s ──\n", side_tag(side),
                static_cast<int>(title.size()), title.data());
    std::printf("    why: %.*s\n", static_cast<int>(why.size()), why.data());
  }
  // Print a COUNTER banner (title + how).
  void counter(Side side, std::string_view title, std::string_view how) {
    std::printf("\n── %s COUNTER: %.*s ──\n", side_tag(side),
                static_cast<int>(title.size()), title.data());
    std::printf("    how: %.*s\n", static_cast<int>(how.size()), how.data());
  }
  // Print BLUE/RED outcome banner + detail.
  void result(bool blue_wins, std::string_view detail) {
    std::printf("\n══ RESULT: %s ══\n    %.*s\n\n",
                blue_wins ? "BLUE detects / constrains" : "RED slips past this layer",
                static_cast<int>(detail.size()), detail.data());
  }
};

}  // namespace sim
