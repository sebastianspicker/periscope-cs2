// T0 BLUE prototype — handle graph + co-occurrence against a simulated T0 red.

#include "ac/proto_log.hpp"
#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "t0_blue/process_cooccurrence.hpp"

#include <cstdio>
#include <vector>

int main() {
  ac::proto_banner("BLUE", "T0", "Handle graph + process co-occurrence");

  constexpr std::uint32_t kGamePid = 1000;
  constexpr std::uint32_t kRadarPid = 2001;

  ac::MemoryTelemetrySink sink;
  ac::RiskAggregator risk;
  t0_blue::HandleGraphMonitor handles(sink);
  t0_blue::ProcessCooccurrence co(sink);

  // Simulated OS snapshot while "game" runs and T0 red holds VM_READ.
  std::vector<t0_blue::HandleEdge> edges = {
      {kRadarPid, kGamePid, t0_blue::ProcessAccess::VmRead, "lab-radar.exe"},
      {3000, kGamePid, t0_blue::ProcessAccess::QueryLimited, "overlay-helper"},
  };
  handles.ingest_edges(kGamePid, edges);

  co.on_game_session(kGamePid, {
                                   {kGamePid, "lab-game"},
                                   {kRadarPid, "lab-radar"},
                                   {4000, "discord"},
                               });

  ac::proto_kv("suspicious_handles", handles.suspicious().size());
  for (const auto& e : handles.suspicious()) {
    std::printf("    src_pid=%u name=%s access=VM_READ\n", e.source_pid,
                e.source_name.c_str());
  }

  for (const auto& ev : sink.events()) {
    risk.ingest(ev);
  }
  ac::proto_events(sink);
  ac::proto_kv("risk_score", risk.state().score);
  ac::proto_kv("flag_overwatch", risk.state().flag_overwatch);

  const bool detected = !handles.suspicious().empty();
  ac::proto_kv("T0_DETECTED", detected);
  std::puts(detected ? "prototype ok (t0 red surface caught)"
                     : "prototype FAIL");
  return detected ? 0 : 1;
}
