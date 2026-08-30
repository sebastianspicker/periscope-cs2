#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

static real::cs2::Cs2MemoryReader* g_reader = nullptr;
static bool rfn(uint64_t addr, void* buf, size_t size) {
  if (!g_reader) return false;
  auto rr = g_reader->read(addr, size);
  if (rr.status != ac::Status::Ok || rr.bytes.size() != size) return false;
  std::memcpy(buf, rr.bytes.data(), size);
  return true;
}

template<typename T>
static bool read_t(uint64_t a, T& o) { return rfn(a, &o, sizeof(T)); }

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  auto& api = real::win::g_Api();
  if (!api.resolved) { puts("api fail"); return 1; }
  auto attach = real::cs2::attach_to_cs2();
  if (!attach.attached) { printf("attach fail: %s\n", attach.error_msg.c_str()); return 1; }

  uint64_t client=0; size_t csize=0;
  if (!real::cs2::find_client_module(attach.pid, attach.handle, client, csize)) {
    puts("no client.dll"); return 1;
  }
  printf("=== CS2 live readout ===\n");
  printf("pid=%u cs2_base=0x%llx\n", attach.pid, (unsigned long long)attach.base_address);
  printf("client.dll base=0x%llx size=0x%zx (%.1f MB)\n",
         (unsigned long long)client, csize, csize/1048576.0);

  real::cs2::Cs2MemoryReader reader;
  if (reader.attach(ac::Tier::T0_UsermodeRpm, attach.pid) != ac::Status::Ok) {
    puts("reader fail"); return 1;
  }
  g_reader = &reader;

  using namespace real::cs2::snapshot;
  auto offsets = real::cs2::offsets_from_snapshot(client);
  printf("\n-- globals (abs) --\n");
  printf("entity_list=0x%llx  local_pawn=0x%llx  local_ctrl_rva=0x%llx\n",
         (unsigned long long)offsets.entity_list,
         (unsigned long long)offsets.local_player,
         (unsigned long long)(client + globals::dwLocalPlayerController));
  printf("view_matrix=0x%llx  view_angles=0x%llx\n",
         (unsigned long long)offsets.view_matrix,
         (unsigned long long)offsets.view_angles);

  uint64_t ent_list_ptr=0, local_pawn=0, local_ctrl=0;
  read_t(offsets.entity_list, ent_list_ptr);
  read_t(offsets.local_player, local_pawn);
  read_t(client + globals::dwLocalPlayerController, local_ctrl);
  printf("\n-- pointers --\n");
  printf("*dwEntityList=0x%llx\n", (unsigned long long)ent_list_ptr);
  printf("*dwLocalPlayerPawn=0x%llx\n", (unsigned long long)local_pawn);
  printf("*dwLocalPlayerController=0x%llx\n", (unsigned long long)local_ctrl);

  // highest index
  if (ent_list_ptr) {
    int32_t highest = 0;
    read_t(ent_list_ptr + globals::dwGameEntitySystem_highestEntityIndex, highest);
    printf("highestEntityIndex=%d\n", highest);
  }

  // view angles (global)
  float va[3] = {};
  read_t(offsets.view_angles, va[0]);
  read_t(offsets.view_angles + 4, va[1]);
  read_t(offsets.view_angles + 8, va[2]);
  printf("\n-- view angles (dwViewAngles) --\n");
  printf("pitch=%.2f yaw=%.2f roll=%.2f\n", va[0], va[1], va[2]);

  // local pawn fields
  if (local_pawn) {
    int32_t hp=0; uint8_t life=0; uint32_t team=0;
    float origin[3]={}, eye[3]={};
    read_t(local_pawn + fields::m_iHealth, hp);
    read_t(local_pawn + fields::m_lifeState, life);
    read_t(local_pawn + fields::m_iTeamNum, team);
    read_t(local_pawn + fields::m_vOldOrigin, origin);
    read_t(local_pawn + fields::m_angEyeAngles, eye);
    // also try scene node abs origin
    uint64_t scene=0;
    float abs_o[3]={};
    if (read_t(local_pawn + fields::m_pGameSceneNode, scene) && scene) {
      read_t(scene + fields::m_vecAbsOrigin, abs_o);
    }
    printf("\n-- local pawn --\n");
    printf("pawn=0x%llx team=%u hp=%d life=%u\n",
           (unsigned long long)local_pawn, team & 0xff, hp, life);
    printf("m_vOldOrigin  = (%.1f, %.1f, %.1f)\n", origin[0], origin[1], origin[2]);
    printf("m_vecAbsOrigin= (%.1f, %.1f, %.1f) scene=0x%llx\n",
           abs_o[0], abs_o[1], abs_o[2], (unsigned long long)scene);
    printf("m_angEyeAngles= pitch=%.2f yaw=%.2f roll=%.2f\n", eye[0], eye[1], eye[2]);
  }

  // entity list walk
  auto ents = real::cs2::read_entity_list(reader, offsets, attach.pid);
  printf("\n-- entity list --\n");
  printf("%s\n", ents.describe().c_str());

  struct Row {
    int idx; uint8_t team; int hp; bool local;
    float x,y,z; float eye_yaw;
    float dx, dy, dist;
    float rx_old, ry_old; // live-radar XZ mapping
    float rx_xy, ry_xy;   // horizontal XY mapping
    float bearing_deg;
  };
  std::vector<Row> rows;
  float lx=0, ly=0, lz=0, yaw_deg = va[1];
  if (local_pawn) {
    float o[3]={}; float e[3]={};
    read_t(local_pawn + fields::m_vOldOrigin, o);
    read_t(local_pawn + fields::m_angEyeAngles, e);
    lx=o[0]; ly=o[1]; lz=o[2];
    if (std::isfinite(e[1])) yaw_deg = e[1];
  }
  // Prefer global viewangles yaw when sane
  if (std::isfinite(va[1]) && std::fabs(va[1]) < 720.f) yaw_deg = va[1];

  const float yaw = yaw_deg * 3.14159265f / 180.f;
  const float scale = 3000.f;
  const float c = std::cos(-yaw), s = std::sin(-yaw);
  const float cf = std::cos(yaw), sf = std::sin(yaw);

  int i = 0;
  for (const auto& e : ents.entities) {
    Row r{};
    r.idx = i++;
    r.team = (uint8_t)e.team;
    r.hp = e.health;
    r.local = e.is_local_player;
    r.x = e.origin.x; r.y = e.origin.y; r.z = e.origin.z;
    r.eye_yaw = e.eye_angles.y;
    r.dx = e.origin.x - lx;
    r.dy = e.origin.y - ly;
    float dz = e.origin.z - lz;
    r.dist = std::sqrt(r.dx*r.dx + r.dy*r.dy);
    // Live-radar XZ mapping
    r.rx_old = (r.dx * c - dz * s) / scale;
    r.ry_old = (r.dx * s + dz * c) / scale;
    // Horizontal XY mapping: forward is up and right is positive screen X.
    // forward=(cos,sin), right=(sin,-cos)
    float forward = r.dx * cf + r.dy * sf;
    float right   = r.dx * sf - r.dy * cf;
    r.rx_xy = right / scale;
    r.ry_xy = -forward / scale; // +forward => up on screen after draw Y-flip
    r.bearing_deg = std::atan2(r.dy, r.dx) * 180.f / 3.14159265f;
    rows.push_back(r);
  }

  printf("\n-- direction analysis (local yaw=%.2f deg) --\n", yaw_deg);
  printf("world: +X forward at yaw=0, +Y left at yaw=0 (Source)\n");
  printf("idx team hp  world(x,y,z)           dXY   bearing  XZ(rx,ry)   XY(rx,ry fwd)\n");
  for (const auto& r : rows) {
    printf("%2d  %u  %3d  (%7.0f,%7.0f,%6.0f) %6.0f %7.1f  (%+.3f,%+.3f)  (%+.3f,%+.3f)%s\n",
           r.idx, r.team, r.hp, r.x, r.y, r.z, r.dist, r.bearing_deg,
           r.rx_old, r.ry_old, r.rx_xy, r.ry_xy,
           r.local ? " LOCAL" : "");
  }

  // Looking toward an entity should produce positive forward distance before the screen flip.
  printf("\n-- direction check --\n");
  printf("For each enemy: world bearing vs local yaw => relative angle (0=ahead, +90=left in Source)\n");
  for (const auto& r : rows) {
    if (r.local || r.dist < 1.f) continue;
    float rel = r.bearing_deg - yaw_deg;
    while (rel > 180.f) rel -= 360.f;
    while (rel < -180.f) rel += 360.f;
    // In the horizontal mapping, ahead produces a negative screen-space Y value.
    // forward>0 ahead, right>0 to the right
    const char* sector =
      (std::fabs(rel) < 30) ? "AHEAD" :
      (std::fabs(rel) > 150) ? "BEHIND" :
      (rel > 0) ? "LEFT" : "RIGHT";
    // Expect: LEFT => rx<0, RIGHT => rx>0, AHEAD => ry<0 (up after flip)
    const char* lr_ok =
      (std::fabs(rel) < 20) ? ((std::fabs(r.rx_xy) < 0.15f) ? "ok" : "check") :
      (rel > 0) ? (r.rx_xy < 0 ? "ok" : "FLIP?") :
                  (r.rx_xy > 0 ? "ok" : "FLIP?");
    const char* fb_ok = (std::fabs(rel) > 150)
                            ? (r.ry_xy > 0 ? "ok" : "FLIP?")
                            : (std::fabs(rel) < 60)
                                  ? (r.ry_xy < 0 ? "ok" : "FLIP?")
                                  : "-";
    printf("  ent%2d rel=%+6.1f° %-6s  rx=%+.3f ry=%+.3f  L/R=%s F/B=%s\n",
           r.idx, rel, sector, r.rx_xy, r.ry_xy, lr_ok, fb_ok);
  }

  // overlay layout
  auto* game = real::gpu::find_cs2_game_window();
  real::gpu::OverlayStyle st{};
  st.anchor = real::gpu::OverlayStyle::Anchor::InGameRadar;
  auto lay = real::gpu::compute_ingame_radar_layout(game, st);
  printf("\n-- overlay layout --\n");
  printf("game_hwnd=%p valid=%d size=%d @(%d,%d) client=%dx%d\n",
         game, lay.valid?1:0, lay.size, lay.x, lay.y, lay.client_w, lay.client_h);

  // dump a few floats near local for angEyeAngles confirmation
  if (local_pawn) {
    printf("\n-- raw floats around m_angEyeAngles (0x%llx) --\n",
           (unsigned long long)fields::m_angEyeAngles);
    for (int off = -16; off <= 32; off += 4) {
      float f=0;
      read_t(local_pawn + fields::m_angEyeAngles + off, f);
      printf("  %+4d: %g\n", off, f);
    }
  }

  reader.detach();
  real::cs2::detach_from_cs2(attach.handle);
  puts("\nDone.");
  return 0;
}
