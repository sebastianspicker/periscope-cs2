// live_radar_support.cpp — helpers for live_radar demo.
#include "demos/live_radar_support.hpp"
#include "demos/radar_shared.hpp"
#include "real/cs2/hijack_reader.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include "real/cs2/periscope_scanner.hpp"
#include "real/win/api_table.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

real::cs2::Cs2MemoryReader* g_reader = nullptr;
real::cs2::stack::DonorIpcClient* g_donor_ipc = nullptr;
bool g_prefer_hijack = false;
bool g_prefer_worker = false;
std::uint64_t g_live_reads = 0;
std::uint64_t g_live_bytes = 0;
std::uint64_t g_entity_rng = 0xDEADBEEFCAFEULL;

void logf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
  std::fflush(stdout);
}

bool live_read(std::uint64_t addr, void* buf, std::size_t size) {
#if LR_PLATFORM_WINDOWS
  if (g_prefer_hijack && real::cs2::hijack::g_Hijack().is_ready()) {
    if (real::cs2::hijack::g_Hijack().read(addr, buf, size)) {
      ++g_live_reads;
      g_live_bytes += size;
      return true;
    }
  }
  if (g_prefer_worker && g_donor_ipc && g_donor_ipc->is_connected()) {
    if (g_donor_ipc->read(addr, buf, size)) {
      ++g_live_reads;
      g_live_bytes += size;
      return true;
    }
  }
#endif
  if (!g_reader) return false;
  auto rr = g_reader->read(addr, size);
  if (rr.status != ac::Status::Ok || rr.bytes.size() != size) return false;
  std::memcpy(buf, rr.bytes.data(), size);
  ++g_live_reads;
  g_live_bytes += size;
  return true;
}

float smooth_angle_deg(float current, float target, float max_step_deg) {
  float d = target - current;
  while (d > 180.f) d -= 360.f;
  while (d < -180.f) d += 360.f;
  if (d > max_step_deg) d = max_step_deg;
  if (d < -max_step_deg) d = -max_step_deg;
  float out = current + d;
  while (out > 180.f) out -= 360.f;
  while (out < -180.f) out += 360.f;
  return out;
}

static std::uint64_t g_cached_c_hud = 0;
static std::uint64_t g_cached_c_hud_client = 0;

std::uint64_t resolve_c_hud(std::uint64_t client_base, std::size_t client_size) {
  if (!client_base || client_size == 0) return 0;
  if (g_cached_c_hud && g_cached_c_hud_client == client_base) {
    std::uint64_t probe = 0;
    if (live_read(g_cached_c_hud, &probe, 8) && probe > 0x10000)
      return g_cached_c_hud;
  }
  auto results = real::cs2::periscope::scan_all_patterns(client_base, client_size, live_read);
  std::uint64_t slot = 0;
  for (const auto& r : results) {
    if (!r.found || r.address == 0) continue;
    if (r.name == "c_hud" || r.name == "c_hud_fallback1" || r.name == "c_hud_fallback2") {
      slot = r.address;
      if (r.name == "c_hud") break;
    }
  }
  if (!slot) return 0;
  std::uint64_t obj = 0;
  if (!live_read(slot, &obj, sizeof(obj))) return 0;
  g_cached_c_hud = obj;
  g_cached_c_hud_client = client_base;
  return obj;
}

bool read_planted_bomb(std::uint64_t client_base, real::cs2::Cs2PlayerEntity& out) {
  using namespace real::cs2::snapshot;
  std::uint64_t ptr = 0;
  if (!live_read(client_base + globals::dwPlantedC4, &ptr, sizeof(ptr)) || !ptr)
    return false;
  std::uint64_t bomb = ptr;
  std::uint64_t maybe = 0;
  if (live_read(ptr, &maybe, sizeof(maybe)) && maybe > 0x10000) {
    std::uint8_t tick = 0;
    if (live_read(maybe + fields::m_bBombTicking, &tick, 1) && tick) bomb = maybe;
  }
  std::uint8_t ticking = 0;
  if (!live_read(bomb + fields::m_bBombTicking, &ticking, 1) || !ticking) {
    if (!live_read(ptr + fields::m_bBombTicking, &ticking, 1) || !ticking)
      return false;
    bomb = ptr;
  }
  int site = -1;
  live_read(bomb + fields::m_nBombSite, &site, 4);
  float blow = 0.f;
  live_read(bomb + fields::m_flC4Blow, &blow, 4);
  std::uint8_t defusing = 0;
  live_read(bomb + fields::m_bBeingDefused, &defusing, 1);
  ac::Vec3 origin{};
  std::uint64_t scene = 0;
  if (live_read(bomb + fields::m_pGameSceneNode, &scene, sizeof(scene)) && scene) {
    live_read(scene + fields::m_vecAbsOrigin, &origin, sizeof(origin));
  } else {
    live_read(bomb + fields::m_vOldOrigin, &origin, sizeof(origin));
  }
  out = {};
  out.is_bomb = true;
  out.is_alive = true;
  out.spotted = true;
  out.health = 100;
  out.team = 2;
  out.origin = origin;
  out.bomb_site = site;
  out.bomb_blow_time = blow;
  out.bomb_defusing = defusing != 0;
  return origin.x != 0.f || origin.y != 0.f || origin.z != 0.f;
}

const char* filter_name(BlipFilter f) {
  switch (f) {
    case BlipFilter::EnemyOnly: return "enemy";
    case BlipFilter::TeamOnly: return "team";
    default: return "all";
  }
}

int run_simulation_fallback(const char* reason) {
  logf("\n=== LIVE RADAR SIMULATION FALLBACK ===\n");
  logf("  reason: %s\n", reason ? reason : "CS2 unavailable");
  TierDoc doc{"Live Radar (SIM)",
              "attach ladder L2/L1/L3 + entity list + polar HUD projection",
              "VM_READ / donor IPC / OpenProcess scar depends on ladder stage",
              "handle graph + volume + co-occurrence dual score",
              "PPL, ETW TI, donor isolation, ranked policy",
              "demos/live_radar.cpp + lib/real/cs2/live_radar_stack.hpp"};
  auto ents = make_sim_entities(2);
  ac::Vec3 origin{};
  float yaw = 0.f;
  for (const auto& e : ents) {
    if (e.is_local) {
      origin = e.origin;
      yaw = e.angles.y;
    }
  }
  auto blips = project_to_radar(ents, origin, yaw);
  render_console(doc, ents, blips, 12.0);
  print_tier_surface(doc, static_cast<int>(ents.size()),
                     static_cast<int>(blips.size()), /*live=*/false);
  logf("live_radar: SIMULATION complete entities=%zu blips=%zu\n", ents.size(),
       blips.size());
  return 0;
}
