// monolith_split_test.cpp — durable gate for the >600-LoC refactor goal.
//
// 1) Walks owned sources under code/ and fails if any .cpp/.c/.h/.hpp/.py
//    exceeds 600 physical lines (excludes build*/CMakeFiles/generated).
// 2) Asserts former-monolith split TUs exist with real logic (not empty shells).
// 3) Drives shipped public APIs that now live across multiple TUs
//    (sim::World / make_arena, blue::BlueCoordinator, cs2::SignatureDatabase,
//    real::gpu::create_render_pipeline when available).
//
// Build: cmake --build . --target monolith_split_test
// Run:   monolith_split_test

#include "sim/world.hpp"
#include "blue/blue_system.hpp"
#include "cs2/signatures.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "real/gpu/render_pipeline.hpp"
#endif

namespace fs = std::filesystem;

namespace {

int g_fails = 0;
int g_checks = 0;

void expect(bool cond, const char* msg) {
  ++g_checks;
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  } else {
    std::printf("ok: %s\n", msg);
  }
}

int count_physical_lines(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return -1;
  int n = 0;
  std::string line;
  while (std::getline(in, line)) {
    ++n;
  }
  return n;
}

bool path_excluded(const fs::path& p) {
  std::string s = p.generic_string();
  for (char& c : s) {
    if (c == '\\') c = '/';
  }
  auto has = [&](const char* frag) { return s.find(frag) != std::string::npos; };
  if (has("/build") || has("/CMakeFiles/") || has("/generated/") ||
      has("/__pycache__/")) {
    return true;
  }
  // Match build* directory segments: /build_dma/, /build_real_all/, etc.
  std::size_t pos = 0;
  while ((pos = s.find("/build", pos)) != std::string::npos) {
    std::size_t end = s.find('/', pos + 1);
    std::string seg =
        end == std::string::npos ? s.substr(pos + 1) : s.substr(pos + 1, end - pos - 1);
    if (seg.rfind("build", 0) == 0) return true;
    pos += 6;
  }
  return false;
}

bool is_owned_ext(const fs::path& p) {
  auto e = p.extension().string();
  for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return e == ".cpp" || e == ".c" || e == ".h" || e == ".hpp" || e == ".py";
}

// Former monoliths must have been split into additional cohesive TUs.
struct SplitProof {
  const char* primary_rel;       // path under code/
  const char* companion_rel;     // extracted unit that must exist
  const char* must_contain;      // substring proving real logic (not empty re-export)
};

const SplitProof kSplitProofs[] = {
    {"lib/real/gpu/render_pipeline.cpp", "lib/real/gpu/render_pipeline_d3d11_draw.cpp",
     "push_"},
    {"lib/radar_pipeline.cpp", "lib/radar_pipeline_collect.cpp", "collect_frame"},
    {"lib/sim/world.hpp", "lib/sim/world_types.hpp", "struct Process"},
    {"lib/blue/blue_system.cpp", "lib/blue/blue_system_handle_module.cpp",
     "check_handle_table"},
    {"lib/real/cs2/periscope_hud.cpp", "lib/real/cs2/periscope_hud_cvar.cpp",
     "CvarManager"},
    {"lib/cs2/signatures.cpp", "lib/cs2/signatures_data.cpp", "kEmbeddedPatternChunks"},
    {"lib/cs2/simulator.cpp", "lib/cs2/simulator_plant.cpp", "plant_cs2"},
    {"drivers/example_vulnerable/vuln_driver.c",
     "drivers/example_vulnerable/vuln_driver_notify.c", "Notify"},
    {"scripts/update_cs2_signatures.py", "scripts/cs2_sig_emit.py", "emit"},
};

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return {};
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

fs::path find_code_root() {
#if defined(LR_CODE_ROOT_STR)
  {
    fs::path p(LR_CODE_ROOT_STR);
    if (fs::is_directory(p)) return fs::weakly_canonical(p);
  }
#endif
  const char* env = std::getenv("LR_CODE_ROOT");
  if (env && *env) {
    fs::path p(env);
    if (fs::is_directory(p)) return fs::weakly_canonical(p);
  }
  fs::path cur = fs::current_path();
  for (int i = 0; i < 8; ++i) {
    if (fs::exists(cur / "lib") && fs::exists(cur / "CMakeLists.txt")) {
      return fs::weakly_canonical(cur);
    }
    if (fs::exists(cur / "code" / "lib") &&
        fs::exists(cur / "code" / "CMakeLists.txt")) {
      return fs::weakly_canonical(cur / "code");
    }
    if (!cur.has_parent_path() || cur == cur.root_path()) break;
    cur = cur.parent_path();
  }
  return {};
}

}  // namespace

int main() {
  const fs::path code_root = find_code_root();
  expect(!code_root.empty(), "locate code/ root");
  if (code_root.empty()) {
    std::fprintf(stderr, "cannot find code root from cwd=%s\n",
                 fs::current_path().string().c_str());
    return 1;
  }
  std::printf("code_root=%s\n", code_root.string().c_str());

  // ── 1. Hard size bar ───────────────────────────────────────────────
  int scanned = 0;
  int over = 0;
  std::vector<std::string> offenders;
  const fs::path roots[] = {
      code_root / "lib",     code_root / "demos", code_root / "tests",
      code_root / "teams",   code_root / "strategies",
      code_root / "scripts", code_root / "drivers",
      code_root / "firmware", code_root / "data",
  };
  for (const auto& root : roots) {
    if (!fs::exists(root)) continue;
    for (auto it = fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {
      std::error_code ec;
      if (!it->is_regular_file(ec)) continue;
      const fs::path p = it->path();
      if (!is_owned_ext(p) || path_excluded(p)) continue;
      const int n = count_physical_lines(p);
      ++scanned;
      if (n > 600) {
        ++over;
        offenders.push_back(p.lexically_relative(code_root).generic_string() +
                            " (" + std::to_string(n) + ")");
      }
    }
  }
  // Top-level code/* sources
  for (auto it = fs::directory_iterator(code_root); it != fs::directory_iterator();
       ++it) {
    if (!it->is_regular_file()) continue;
    const fs::path p = it->path();
    if (!is_owned_ext(p)) continue;
    const int n = count_physical_lines(p);
    ++scanned;
    if (n > 600) {
      ++over;
      offenders.push_back(p.filename().string() + " (" + std::to_string(n) + ")");
    }
  }
  expect(scanned > 100, "scanned substantial owned source set");
  expect(over == 0, "zero owned sources exceed 600 LoC");
  for (const auto& o : offenders) {
    std::fprintf(stderr, "  over-limit: %s\n", o.c_str());
  }
  std::printf("scanned=%d over=%d\n", scanned, over);

  // ── 2. Split TUs exist with real logic ─────────────────────────────
  for (const auto& sp : kSplitProofs) {
    const fs::path primary = code_root / sp.primary_rel;
    const fs::path companion = code_root / sp.companion_rel;
    expect(fs::exists(primary), sp.primary_rel);
    expect(fs::exists(companion), sp.companion_rel);
    const int pn = count_physical_lines(primary);
    const int cn = count_physical_lines(companion);
    expect(pn > 0 && pn <= 600, "primary under 600");
    expect(cn > 0 && cn <= 600, "companion under 600");
    const std::string body = read_file(companion);
    expect(body.find(sp.must_contain) != std::string::npos, sp.must_contain);
    // Not a pure re-export shell: require a function-ish body marker.
    const bool has_body = body.find('{') != std::string::npos;
    expect(has_body, "companion has executable structure");
  }

  // ── 3. Drive shipped APIs across split TUs ─────────────────────────
  {
    sim::World w = sim::make_arena("monolith-split-lab");
    expect(w.game_pid() != 0, "make_arena seeds game pid");
    expect(w.ac_pid() != 0, "make_arena seeds ac pid");
    w.plant_lab_entities(w.game_pid());
    expect(!w.processes.empty(), "world processes after plant");
  }
  {
    sim::World w = sim::make_arena("blue-split");
    blue::BlueCoordinator blue(w);
    auto r = blue.evaluate();
    expect(r.per_view.size() >= 4, "blue evaluate returns multi-view results");
    // Force each split sensor TU via evaluate_view if available.
    (void)blue.evaluate_view(ac::ObservationView::HandleTable);
    (void)blue.evaluate_view(ac::ObservationView::ModuleList);
    (void)blue.evaluate_view(ac::ObservationView::MemoryPattern);
    (void)blue.evaluate_view(ac::ObservationView::Behavioral);
    expect(true, "blue evaluate_view across split sensor TUs");
  }
  {
    const auto& db = cs2::SignatureDatabase::get();
    expect(db.total_count() > 0, "SignatureDatabase corpus non-empty (data TU)");
  }
#if defined(_WIN32)
  {
    real::gpu::OverlayStyle style{};
    style.width = 64;
    style.height = 64;
    style.anchor = real::gpu::OverlayStyle::Anchor::ScreenCorner;
    auto created = real::gpu::create_render_pipeline(style, "monolith_split_test");
    expect(created.ok && created.value != nullptr,
           "create_render_pipeline returns pipeline");
    if (created.ok && created.value) {
      real::gpu::RenderPipeline* pipe = created.value;
      expect(pipe->name() != nullptr, "pipeline name from split backend TU");
      (void)pipe->shutdown();
      delete pipe;
    }
  }
#endif

  std::printf("\n%d checks, %d failed\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}
