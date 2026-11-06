#pragma once

// End-to-end lab session orchestrator: fixture → memory → AOB → API table →
// disguise → draw list → blue scan detection. Educational only.

#include "lab/aob_scanner.hpp"
#include "lab/api_table.hpp"
#include "lab/cheat_sig_db.hpp"
#include "lab/disguise_engine.hpp"
#include "lab/draw_list.hpp"
#include "lab/fixture_process.hpp"
#include "lab/lab_memory.hpp"
#include "lab/scan_detector.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace lab {

struct LabSessionConfig {
  ac::DisguiseProfile disguise = ac::DisguiseProfile::RivaTuner;
  bool plant_cs2_markers = true;
  bool resolve_apis_via_peb = true;
  bool build_esp_drawlist = true;
  ApiResolvePath api_path = ApiResolvePath::PebEatWalk;
};

struct LabSessionReport {
  bool attached = false;
  bool markers_planted = false;
  int entities = 0;
  int aob_found = 0;
  int aob_attempted = 0;
  int apis_resolved = 0;
  double api_stealth = 0.0;
  bool disguise_verified = false;
  bool esp_signature = false;
  bool scan_suspected = false;
  double scan_risk = 0.0;
  int draw_commands = 0;
  std::vector<std::string> steps;
  std::string detail;
  bool success = false;
};

class LabSession {
 public:
  explicit LabSession(LabSessionConfig config = {});

  // Run against the global fixture (unit-test path).
  LabSessionReport run_fixture_pipeline();

  // Run against a sim::World arena (strategy path).
  LabSessionReport run_world_pipeline(sim::World& world, std::uint32_t cheat_pid);

  const LabSessionReport& last_report() const { return last_; }
  const AobScanReport& last_aob() const { return last_aob_; }
  const DrawList& draw_list() const { return draw_; }
  const ApiTable& api_table() const { return apis_; }
  LabMemoryBackend& backend() { return backend_; }

 private:
  LabSessionConfig config_;
  LabMemoryBackend backend_;
  ApiTable apis_;
  DrawList draw_;
  AobScanReport last_aob_{};
  LabSessionReport last_{};

  void build_esp_from_fixture(const FixtureProcess& fx);
};

}  // namespace lab
