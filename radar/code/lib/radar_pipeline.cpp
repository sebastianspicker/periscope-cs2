// radar_pipeline.cpp — Complete radar frame pipeline implementation.
//
// ALL 15+ ac_sim features are integrated here. Previously each was
// fully implemented but NEVER instantiated or called from any pipeline.
// This file resolves that by wiring everything into a coherent frame loop.
//
// Frame flow:
//   pre_frame()   → RNG, temporal, disguise, self-verify, API integrity, system normalize
//   collect_frame() → batch read, entity collect, HUD, CVar
//   post_frame()  → health ladder, accept gate, decoy render, behavioral filter, ETL, DllWatch, forensic

#include "radar_pipeline.hpp"

#include "ac/types.hpp"
#include "real/win/anti_debug.hpp"
#include "real/win/pe_hide.hpp"
#include "real/win/hook_detect.hpp"
#include "real/win/veh_anti_debug.hpp"
#include "real/win/timing.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/cs2/offsets.hpp"          // find_client_module / find_module_by_basename
#include "real/cs2/periscope_scanner.hpp" // scan_all_patterns / PeriscopeOffsets
#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace radar {

FramePipeline* g_pipeline = nullptr;
const char* kDepRng[] = {"RNG"};

}  // namespace radar
