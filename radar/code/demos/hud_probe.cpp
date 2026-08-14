#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/periscope_scanner.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>

static real::cs2::Cs2MemoryReader* g_r;
static bool rfn(uint64_t a,void*b,size_t n){auto rr=g_r->read(a,n);if(rr.status!=ac::Status::Ok||rr.bytes.size()!=n)return false;memcpy(b,rr.bytes.data(),n);return true;}

int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  if(!real::win::g_Api().resolved){
    std::printf("hud_probe: API table resolution failed\n");
    return 1;
  }
  auto att=real::cs2::attach_to_cs2();
  if(!att.attached){
    std::printf("hud_probe: attach failed: %s\n", att.error_msg.c_str());
    return 1;
  }
  uint64_t client=0;size_t csz=0; real::cs2::find_client_module(att.pid,att.handle,client,csz);
  real::cs2::Cs2MemoryReader reader; reader.attach(ac::Tier::T0_UsermodeRpm,att.pid); g_r=&reader;
  auto results=real::cs2::periscope::scan_all_patterns(client,csz,rfn);
  uint64_t slot=0; for(auto&r:results) if(r.found&&r.name=="c_hud") slot=r.address;
  uint64_t chud=0; rfn(slot,&chud,8);
  uint64_t data=0; rfn(chud+0x268,&data,8);
  uint64_t q[4]; rfn(data+6*0x20,q,32);
  uint64_t rb=q[3];
  printf("radar panel=0x%llx\n",(unsigned long long)rb);

  // local origin for comparison
  uint64_t pawn=0; rfn(client+real::cs2::snapshot::globals::dwLocalPlayerPawn,&pawn,8);
  float lo[3]={}; if(pawn) rfn(pawn+real::cs2::snapshot::fields::m_vOldOrigin,lo,12);
  printf("local origin (%.1f,%.1f,%.1f)\n", lo[0],lo[1],lo[2]);

  printf("\nAll finite floats 0x0..0x800:\n");
  for(int off=0; off<0x800; off+=4){
    float v=0; rfn(rb+off,&v,4);
    if(!std::isfinite(v) || v==0) continue;
    // show all reasonable
    if(std::fabs(v)<1e-4f || std::fabs(v)>1e8f) continue;
    printf("  +0x%03X = %g\n", off, v);
  }

  // follow pointers in first 0x100 for nested objects with map pos
  printf("\nNested pointer scan:\n");
  for(int off=0; off<0x100; off+=8){
    uint64_t p=0; rfn(rb+off,&p,8);
    if(p<0x100000 || p>0x7FFFFFFFFFFFULL) continue;
    // look for world-like vec3 in nested
    for(int o2=0; o2<0x200; o2+=4){
      float x=0,y=0,z=0;
      rfn(p+o2,&x,4); rfn(p+o2+4,&y,4); rfn(p+o2+8,&z,4);
      if(std::fabs(x)>100 && std::fabs(x)<20000 && std::fabs(y)>100 && std::fabs(y)<20000 && std::fabs(z)<2000){
        float dx=x-lo[0], dy=y-lo[1];
        float d=std::sqrt(dx*dx+dy*dy);
        if(d<5000) // near local or map-related
          printf("  panel+0x%X -> 0x%llx +0x%X vec=(%.1f,%.1f,%.1f) distLocal=%.0f\n",
            off,(unsigned long long)p,o2,x,y,z,d);
      }
    }
  }

  reader.detach(); real::cs2::detach_from_cs2(att.handle);
  return 0;
}
