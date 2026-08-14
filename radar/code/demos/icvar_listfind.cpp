#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/cs2/offsets.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
static real::cs2::Cs2MemoryReader* g_r;
static bool rfn(uint64_t a,void*b,size_t n){auto rr=g_r->read(a,n);if(rr.status!=ac::Status::Ok||rr.bytes.size()!=n)return false;memcpy(b,rr.bytes.data(),n);return true;}
static uint64_t resolve(uint64_t base,size_t size,const char*name){
  int32_t el=0;rfn(base+0x3C,&el,4);uint32_t er=0;rfn(base+(uint64_t)el+24+0x70,&er,4);
  uint32_t nn=0,af=0,an=0,ao=0;rfn(base+er+0x18,&nn,4);rfn(base+er+0x1C,&af,4);rfn(base+er+0x20,&an,4);rfn(base+er+0x24,&ao,4);
  for(uint32_t i=0;i<nn;++i){uint32_t nr=0;rfn(base+an+(uint64_t)i*4,&nr,4);char nm[80]{};rfn(base+nr,nm,79);if(strcmp(nm,name)==0){uint16_t o=0;rfn(base+ao+(uint64_t)i*2,&o,2);uint32_t fr=0;rfn(base+af+(uint64_t)o*4,&fr,4);return base+fr;}}return 0;}
int main(){
 setvbuf(stdout,0,_IONBF,0);if(!real::win::g_Api().resolved)return 1;auto att=real::cs2::attach_to_cs2();if(!att.attached)return 1;
 uint64_t tier0=0;size_t tsz=0;real::cs2::find_module_by_basename(att.pid,att.handle,"tier0.dll",tier0,tsz);
 real::cs2::Cs2MemoryReader r;r.attach(ac::Tier::T0_UsermodeRpm,att.pid);g_r=&r;
 uint64_t cifn=resolve(tier0,tsz,"CreateInterface");
 uint8_t code[16];rfn(cifn,code,16);int32_t d=0;memcpy(&d,code+3,4);uint64_t head=0;rfn(cifn+7+d,&head,8);
 uint64_t reg=head,icvar=0;
 for(int s=0;s<16&&reg;++s){uint64_t np=0,cf=0,nx=0;rfn(reg+8,&np,8);rfn(reg,&cf,8);rfn(reg+16,&nx,8);char nm[40]{};if(np)rfn(np,nm,39);
  if(strncmp(nm,"VEngineCvar",11)==0){uint8_t st[16];rfn(cf,st,16);int32_t d2=0;memcpy(&d2,st+3,4);icvar=cf+7+d2;break;}reg=nx;}
 printf("icvar=0x%llx\n",(unsigned long long)icvar);
 // For each ptr in icvar, try as list head; print first 5 good names
 for(int off=0;off<=0x1F0;off+=8){
  uint64_t headp=0;rfn(icvar+off,&headp,8);
  if(headp<0x10000||headp>0x00007FFFFFFFFFFFULL) continue;
  for(int nameOff:{0x18,0x10,0x20,0x28,0x08,0x00,0x30,0x40}){
   for(int nextOff:{0x08,0x10,0x18,0x00}){
    if(nextOff==nameOff)continue;
    uint64_t node=headp;int good=0;char first[5][48]={{0}};
    for(int step=0;step<64&&node;++step){
     uint64_t np=0,next=0;rfn(node+nameOff,&np,8);rfn(node+nextOff,&next,8);
     char nm[48]{};if(np>0x10000)rfn(np,nm,47);
     bool ok=nm[0]=='_'||(nm[0]>='a'&&nm[0]<='z')||(nm[0]>='A'&&nm[0]<='Z');
     if(ok){int al=0;for(int k=0;nm[k];++k){char c=nm[k];if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.')++al;else{ok=false;break;}}if(ok&&al>=3){if(good<5)strncpy(first[good],nm,47);++good;}}
     if(!next||next==node||next<0x10000)break;node=next;
    }
    if(good>=3){printf("LIST icvar+0x%X name@+0x%X next@+0x%X good=%d:",off,nameOff,nextOff,good);for(int i=0;i<good&&i<5;++i)printf(" %s",first[i]);printf("\n");}
   }
  }
  // also: headp as array of 64 ptrs
  int arrGood=0;char an[5][48]={{0}};
  for(int i=0;i<128;++i){uint64_t p=0;rfn(headp+(uint64_t)i*8,&p,8);if(p<0x10000)continue;
   for(int no:{0x18,0x10,0x20,0x00,0x28,0x40}){uint64_t np=0;rfn(p+no,&np,8);char nm[48]{};if(np>0x10000)rfn(np,nm,47);
    bool ok=nm[0]=='_'||(nm[0]>='a'&&nm[0]<='z');if(!ok)continue;int al=0;for(int k=0;nm[k];++k){char c=nm[k];if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')++al;else{ok=false;break;}}
    if(ok&&al>=4){if(arrGood<5)strncpy(an[arrGood],nm,47);++arrGood;break;}}
  }
  if(arrGood>=3){printf("ARRAY icvar+0x%X -> 0x%llx good=%d:",off,(unsigned long long)headp,arrGood);for(int i=0;i<arrGood&&i<5;++i)printf(" %s",an[i]);printf("\n");}
 }
 r.detach();real::cs2::detach_from_cs2(att.handle);return 0;
}
