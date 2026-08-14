#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include "demos/radar_shared.hpp"
#include <cstdio>
#include <cmath>
#include <cstring>

static real::cs2::Cs2MemoryReader* g_r;
static bool rfn(uint64_t a, void* b, size_t n) {
  auto rr = g_r->read(a, n);
  if (rr.status != ac::Status::Ok || rr.bytes.size()!=n) return false;
  memcpy(b, rr.bytes.data(), n); return true;
}
template<class T> bool rd(uint64_t a, T& o){ return rfn(a,&o,sizeof(T)); }

int main() {
  setvbuf(stdout,nullptr,_IONBF,0);
  if (!real::win::g_Api().resolved) {
    puts("scale_probe: API table resolution failed");
    return 1;
  }
  auto att = real::cs2::attach_to_cs2();
  if (!att.attached) {
    printf("scale_probe: attach failed: %s\n", att.error_msg.c_str());
    return 1;
  }
  uint64_t client=0; size_t csz=0;
  real::cs2::find_client_module(att.pid, att.handle, client, csz);
  real::cs2::Cs2MemoryReader reader;
  reader.attach(ac::Tier::T0_UsermodeRpm, att.pid); g_r=&reader;
  auto offs = real::cs2::offsets_from_snapshot(client);
  using namespace real::cs2::snapshot;
  uint64_t local=0; rd(offs.local_player, local);
  float o[3]={}, va[3]={};
  if (local) rd(local+fields::m_vOldOrigin, o);
  rd(offs.view_angles, va);
  float yaw=va[1];
  printf("local=(%.0f,%.0f,%.0f) yaw=%.1f\n", o[0],o[1],o[2], yaw);
  auto ents = real::cs2::read_entity_list(reader, offs, att.pid);
  float cf=cosf(yaw*3.14159265f/180.f), sf=sinf(yaw*3.14159265f/180.f);
  float cl=0.7f;
  float S_old = radar_world_scale(cl); // current
  // Correct Source-style: edge = k / cl_radar_scale
  // Diagnosis: r_draw = 0.5 * r_true => S_true = S_old/2
  float S_fix = S_old * 0.5f;
  printf("current radar_world_scale(0.7)=%.1f\n", S_old);
  printf("if 50%% too close, true scale≈%.1f  (edge@scale1=%.1f)\n", S_fix, S_fix*cl);
  printf("\ndist(u)  r@old  r@half  (half should match in-game if diagnosis holds)\n");
  for (auto& e: ents.entities) {
    if (!e.is_alive||e.is_local_player) continue;
    float dx=e.origin.x-o[0], dy=e.origin.y-o[1];
    float dist=sqrtf(dx*dx+dy*dy);
    float f=dx*cf+dy*sf, r=dx*sf-dy*cf;
    float ro=sqrtf((r/S_old)*(r/S_old)+(f/S_old)*(f/S_old));
    float rh=sqrtf((r/S_fix)*(r/S_fix)+(f/S_fix)*(f/S_fix));
    printf("%6.0f   %.3f   %.3f\n", dist, ro, rh);
  }
  reader.detach(); real::cs2::detach_from_cs2(att.handle);
  return 0;
}
