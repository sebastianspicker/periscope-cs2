#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/offsets.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>
static real::cs2::Cs2MemoryReader* g_r;
static bool rfn(uint64_t a,void*b,size_t n){auto rr=g_r->read(a,n);if(rr.status!=ac::Status::Ok||rr.bytes.size()!=n)return false;memcpy(b,rr.bytes.data(),n);return true;}
static uint64_t find_str(uint64_t base,size_t size,const char*t){size_t tl=strlen(t);std::vector<uint8_t> c(0x10000);for(uint64_t o=0;o+tl<size;o+=0x10000){size_t n=(std::min)((size_t)0x10000,size-(size_t)o);if(!rfn(base+o,c.data(),n))continue;for(size_t i=0;i+tl<n;++i)if(memcmp(c.data()+i,t,tl)==0&&c[i+tl]==0)return base+o+i;}return 0;}
int main(){setvbuf(stdout,0,_IONBF,0);if(!real::win::g_Api().resolved)return 1;auto att=real::cs2::attach_to_cs2();if(!att.attached)return 1;
uint64_t tier0=0,client=0;size_t tsz=0,csz=0;
real::cs2::find_module_by_basename(att.pid,att.handle,"tier0.dll",tier0,tsz);
real::cs2::find_module_by_basename(att.pid,att.handle,"client.dll",client,csz);
real::cs2::Cs2MemoryReader r;r.attach(ac::Tier::T0_UsermodeRpm,att.pid);g_r=&r;
// resolve ICVar quickly
uint64_t ci=0; // use CreateInterface from tier0 via simple export walk skip - hardcode from probe
// re-read createinterface
// simpler: use known singleton from previous run is ASLR so recompute
auto resolve=[&](uint64_t base,size_t size,const char*name)->uint64_t{
  int32_t el=0;rfn(base+0x3C,&el,4);uint32_t er=0,es=0;rfn(base+el+24+0x70,&er,4);rfn(base+el+24+0x74,&es,4);
  uint32_t nn=0,af=0,an=0,ao=0;rfn(base+er+0x18,&nn,4);rfn(base+er+0x1C,&af,4);rfn(base+er+0x20,&an,4);rfn(base+er+0x24,&ao,4);
  size_t nl=strlen(name);for(uint32_t i=0;i<nn;++i){uint32_t nr=0;rfn(base+an+i*4u,&nr,4);char nm[64]{};rfn(base+nr,nm,63);if(strcmp(nm,name)==0){uint16_t o=0;rfn(base+ao+i*2u,&o,2);uint32_t fr=0;rfn(base+af+o*4u,&fr,4);return base+fr;}}return 0;};
uint64_t cifn=resolve(tier0,tsz,"CreateInterface");
int32_t d=0;if(!cifn||!rfn(cifn+3,&d,sizeof(d))){r.detach();real::cs2::detach_from_cs2(att.handle);return 1;}uint64_t slot=cifn+7+d;uint64_t head=0;rfn(slot,&head,8);
uint64_t reg=head,icvar=0;for(int s=0;s<16&&reg;++s){uint64_t np=0,cf=0,nx=0;rfn(reg+8,&np,8);rfn(reg,&cf,8);rfn(reg+16,&nx,8);char nm[40]{};if(np)rfn(np,nm,39);if(strncmp(nm,"VEngineCvar",11)==0){int32_t d2=0;if(!rfn(cf+3,&d2,sizeof(d2))){reg=nx;continue;}icvar=cf+7+d2;break;}reg=nx;}
printf("icvar=0x%llx\n",(unsigned long long)icvar);
// find strings
const char* names[]={"cl_radar_scale","hud_scaling","cl_hud_radar_scale","fps_max","sv_cheats","name"};
for(auto*nm:names){uint64_t sc=find_str(client,csz,nm);uint64_t st=find_str(tier0,tsz,nm);printf("%s client=0x%llx tier0=0x%llx\n",nm,(unsigned long long)sc,(unsigned long long)st);}
// Scan a wide range of heap from ICVar pointer fields for any name string match by content
std::vector<uint64_t> strs;for(auto*nm:names){auto a=find_str(client,csz,nm);if(a)strs.push_back(a);a=find_str(tier0,tsz,nm);if(a)strs.push_back(a);}
for(int off=0;off<0x200;off+=8){uint64_t p=0;rfn(icvar+off,&p,8);if(p<0x100000||p>0x7FFFFFFFFFFFULL)continue;
// if p looks like heap, scan 1MB
for(uint64_t o=0;o<0x100000;o+=0x800){uint64_t buf[256];if(!rfn(p+o,buf,sizeof(buf)))break;for(size_t i=0;i<256;++i){for(uint64_t s:strs){if(buf[i]==s){printf("HIT icvar+0x%X block=0x%llx off=0x%llx str=0x%llx\n",off,(unsigned long long)p,(unsigned long long)(o+i*8),(unsigned long long)s);}}}}}
r.detach();real::cs2::detach_from_cs2(att.handle);return 0;}
