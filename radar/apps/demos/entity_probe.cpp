#include "real/real_fwd.hpp"
#include "real/win/api_table.hpp"
#include "real/win/xorstr.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>

// Tiny probe: dump entity list structure from live CS2
int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  auto& api = real::win::g_Api();
  if (!api.resolved) { puts("api fail"); return 1; }
  auto attach = real::cs2::attach_to_cs2();
  if (!attach.attached) { printf("attach fail: %s\n", attach.error_msg.c_str()); return 1; }
  printf("pid=%u base=0x%llx\n", attach.pid, (unsigned long long)attach.base_address);

  std::uint64_t client_base=0; std::size_t client_size=0;
  if (!real::cs2::find_client_module(attach.pid, attach.handle, client_base, client_size)) {
    puts("no client.dll"); return 1;
  }
  printf("client.dll base=0x%llx size=0x%zx\n", (unsigned long long)client_base, client_size);

  real::cs2::Cs2MemoryReader reader;
  if (reader.attach(ac::Tier::T0_UsermodeRpm, attach.pid) != ac::Status::Ok) {
    puts("reader attach fail"); return 1;
  }

  auto r64 = [&](std::uint64_t addr, const char* tag) -> std::uint64_t {
    auto rr = reader.read(addr, 8);
    if (rr.status != ac::Status::Ok || rr.bytes.size()!=8) {
      printf("  READ FAIL %s @0x%llx status=%d\n", tag, (unsigned long long)addr, (int)rr.status);
      return 0;
    }
    std::uint64_t v; std::memcpy(&v, rr.bytes.data(), 8);
    printf("  %s @0x%llx = 0x%llx\n", tag, (unsigned long long)addr, (unsigned long long)v);
    return v;
  };
  auto r32 = [&](std::uint64_t addr, const char* tag) -> std::uint32_t {
    auto rr = reader.read(addr, 4);
    if (rr.status != ac::Status::Ok || rr.bytes.size()!=4) {
      printf("  READ FAIL %s @0x%llx\n", tag, (unsigned long long)addr);
      return 0;
    }
    std::uint32_t v; std::memcpy(&v, rr.bytes.data(), 4);
    printf("  %s @0x%llx = 0x%x (%u)\n", tag, (unsigned long long)addr, v, v);
    return v;
  };

  using namespace real::cs2::snapshot;
  std::uint64_t el_addr = client_base + globals::dwEntityList;
  std::uint64_t entity_list = r64(el_addr, "dwEntityList ptr");
  if (!entity_list) return 1;

  // highest entity index (relative to GameEntitySystem)
  r32(entity_list + 0x2090, "highestEntityIndex@+0x2090");
  // also try absolute from client
  r32(client_base + globals::dwEntityList + 0x2090, "highest@client+dwEL+0x2090");

  // dump first few qwords of entity system object
  puts("entity_system[0..0x40):");
  for (int i=0;i<8;i++) {
    char tag[32]; snprintf(tag,sizeof(tag),"  +0x%X", i*8);
    r64(entity_list + i*8, tag);
  }

  // chunk table at +0x10
  std::uint64_t chunk0 = r64(entity_list + 0x10, "chunk0 @ +0x10");
  if (chunk0) {
    puts("chunk0 identities (index 0..8):");
    for (int i=0;i<=8;i++) {
      char tag[48];
      snprintf(tag,sizeof(tag),"  identity[%d] ent", i);
      std::uint64_t ent = r64(chunk0 + (std::uint64_t)i * 0x70, tag);
      if (ent && i>=1 && i<=4) {
        // try health/team at pawn-ish offsets - might be controller
        r32(ent + fields::m_iHealth, "    health");
        r32(ent + fields::m_iTeamNum, "    team");
        r32(ent + fields::m_hPlayerPawn, "    m_hPlayerPawn");
      }
    }
  }

  // local player pawn/controller
  std::uint64_t local_pawn = r64(client_base + globals::dwLocalPlayerPawn, "local pawn ptr");
  std::uint64_t local_ctrl = r64(client_base + globals::dwLocalPlayerController, "local controller ptr");
  if (local_pawn) {
    r32(local_pawn + fields::m_iHealth, "local.health");
    r32(local_pawn + fields::m_iTeamNum, "local.team");
  }
  if (local_ctrl) {
    r32(local_ctrl + fields::m_hPlayerPawn, "ctrl.m_hPlayerPawn");
    r32(local_ctrl + fields::m_iTeamNum, "ctrl.team");
  }

  // Also try: entity_list used WITHOUT first deref (some cheats do client+RVA as base)
  puts("alt: treat client+dwEntityList as system base (no deref):");
  std::uint64_t alt = el_addr;
  r32(alt + 0x2090, "alt highest");
  r64(alt + 0x10, "alt chunk0");

  reader.detach();
  real::cs2::detach_from_cs2(attach.handle);
  return 0;
}
