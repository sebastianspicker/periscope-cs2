#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/hijack_reader.hpp"
#include "real/cs2/offsets.hpp"
#include "real/cs2/periscope_hud.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
static real::cs2::Cs2MemoryReader* g_r;
static bool rfn(uint64_t a,void*b,size_t n){
  if(real::cs2::hijack::g_Hijack().is_ready()){
    if(real::cs2::hijack::g_Hijack().read(a,b,n)) return true;
  }
  if(!g_r) return false;
  auto rr=g_r->read(a,n);
  if(rr.status!=ac::Status::Ok||rr.bytes.size()!=n)return false;
  memcpy(b,rr.bytes.data(),n);return true;
}
int main(){
  setvbuf(stdout,0,_IONBF,0);
  auto& api=real::win::g_Api();
  if(!api.resolved){puts("icvar_probe: API table resolution failed");return 1;}
  auto att=real::cs2::attach_to_cs2();
  if(!att.attached){printf("icvar_probe: attach failed: %s\n",att.error_msg.c_str());return 1;}
  bool ok=real::cs2::hijack::g_Hijack().setup(att.pid,(void*)(uintptr_t)att.handle);
  auto& hj=real::cs2::hijack::g_Hijack();
  printf("hijack=%d donor=%u err=%s\n",ok?1:0,hj.donor_pid(),hj.last_error());
  // PE probe at cs2 base via hijack
  uint16_t mz=0;
  bool r1=hj.read(att.base_address,&mz,2);
  printf("hijack read MZ @base: ok=%d mz=0x%04X (expect 0x5A4D)\n",r1?1:0,mz);
  uint64_t client=0;size_t csz=0;
  real::cs2::find_client_module(att.pid,att.handle,client,csz);
  printf("client=0x%llx size=0x%zx\n",(unsigned long long)client,csz);
  uint16_t mz2=0; bool r2=hj.read(client,&mz2,2);
  printf("hijack read MZ @client: ok=%d mz=0x%04X\n",r2?1:0,mz2);
  // full path via memory reader
  real::cs2::Cs2MemoryReader reader;
  reader.attach(ac::Tier::T0_UsermodeRpm,att.pid); g_r=&reader;
  printf("backend=%s\n",reader.active_backend_name());
  auto rr=reader.read(client,2);
  printf("reader client MZ status=%d size=%zu\n",(int)rr.status,rr.bytes.size());
  if(rr.bytes.size()>=2) printf("  bytes %02X %02X\n",rr.bytes[0],rr.bytes[1]);
  // cvars
  uint64_t tier0=0,eng=0;size_t tsz=0,esz=0;
  real::cs2::find_module_by_basename(att.pid,att.handle,"tier0.dll",tier0,tsz);
  real::cs2::find_module_by_basename(att.pid,att.handle,"engine2.dll",eng,esz);
  real::cs2::periscope::CvarManager cm;
  cm.initialize(eng,esz,tier0,tsz,client,csz,0,rfn);
  printf("cvars path=%d cl_radar=%.3f\n",cm.values().resolutionPath,cm.values().clRadarScale);
  reader.detach();
  real::cs2::hijack::g_Hijack().shutdown();
  real::cs2::detach_from_cs2(att.handle);
  return 0;
}
