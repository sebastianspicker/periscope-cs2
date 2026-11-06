#include "red_example.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace examples::cpuid_hypercall_channel {

namespace {
// Lab magic key returned by the covert CPUID leaf ('ACHV' little-endian).
constexpr std::uint32_t kChannelMagic = 0x41434856u;
constexpr int kProbeLeaves = 3;
}  // namespace

RedResult apply(sim::World& w) {
  RedResult result{false, 0, "", "cpuid channel precondition failed"};
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game_pid || !game || game->memory.empty() || w.ranked_access_denied) {
    result.detail = "game process or entity table unavailable";
    return result;
  }
  ++result.steps;

  const auto actor = w.spawn("cpuid-channel-actor.exe");
  if (!w.proc(actor)) {
    result.detail = "covert-channel consumer could not start";
    return result;
  }
  ++result.steps;

  // The platform hypervisor exposes the hypercall interface red abuses. VBS
  // would keep the leaf range locked, so the lab arena requires it off (the
  // pair.cpp precondition seeds the same posture the hypercall pair uses).
  if (!w.trust.platform_hv_active || w.trust.platform_hv_vendor.empty()) {
    result.detail = "no platform hypervisor hypercall interface available";
    return result;
  }
  result.magic = w.trust.platform_hv_vendor;
  ++result.steps;

  // Step: probe CPUID leaves until the magic key answers. Abnormal leaf
  // probing is itself a blue signal (cpuid_leaf_probes).
  w.cpuid_leaf_probes = kProbeLeaves;
  w.cpuid_channel_magic = kChannelMagic;
  if (w.cpuid_leaf_probes < kProbeLeaves || w.cpuid_channel_magic != kChannelMagic) {
    result.detail = "magic leaf probe failed";
    return result;
  }
  ++result.steps;

  // Step: open the unauthenticated covert channel. Any local process that
  // knows the magic can use it - that is the channel-abuse residual.
  w.cpuid_hypercall_channel_active = true;
  w.cpuid_channel_unauthenticated = true;
  w.cpuid_channel_consumer_pid = actor;
  ++result.steps;

  // Step: read the entity count through the covert hypercall channel. No
  // game handle is opened; the lab hypercall page carries entity bytes.
  const auto table_rel = static_cast<std::size_t>(w.lab_entity_table_rel);
  if (table_rel + sizeof(std::uint32_t) > game->memory.size()) {
    result.detail = "channel target page has no entity data";
    return result;
  }
  std::uint32_t entity_count = 0;
  std::memcpy(&entity_count, game->memory.data() + table_rel, sizeof(entity_count));
  ++w.hypercall_channel_reads;
  ++result.steps;

  const bool no_handle = std::none_of(w.handles.begin(), w.handles.end(),
                                      [&](const sim::Handle& handle) {
                                        return handle.owner_pid == actor &&
                                               handle.target_pid == game_pid &&
                                               sim::has(handle.access, sim::AccessMask::VmRead);
                                      });
  result.achieved = w.cpuid_hypercall_channel_active && w.hypercall_channel_reads > 0 &&
                    entity_count > 0 && no_handle;
  result.detail = "cpuid channel reads=" + std::to_string(w.hypercall_channel_reads) +
                  " entity_count=" + std::to_string(entity_count) +
                  " leaf_probes=" + std::to_string(w.cpuid_leaf_probes) +
                  " unauthenticated=" + std::to_string(w.cpuid_channel_unauthenticated) +
                  " no_vm_read_handle=" + std::to_string(no_handle);
  w.note(result.detail);
  return result;
}

}  // namespace examples::cpuid_hypercall_channel
