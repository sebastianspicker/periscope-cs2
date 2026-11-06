// T2 BLUE prototype — catch no-handle red via driver/BYOVD/device signals.

#include "ac/proto_log.hpp"
#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "t0_blue/handle_graph_monitor.hpp"
#include "t2_blue/byovd_blocklist.hpp"
#include "t2_blue/callback_integrity.hpp"
#include "t2_blue/device_watch.hpp"
#include "t2_blue/driver_guard.hpp"

#include <cstdio>

int main() {
  ac::proto_banner("BLUE", "T2", "Driver guard + BYOVD blocklist + device watch");

  constexpr std::uint32_t kGamePid = 1000;
  constexpr std::uint32_t kUiPid = 2200;

  ac::MemoryTelemetrySink sink;
  ac::RiskAggregator risk;

  // Handle graph is intentionally clean (T2 design).
  t0_blue::HandleGraphMonitor handles(sink);
  handles.ingest_edges(kGamePid, {});  // no VM_READ edges
  ac::proto_kv("handle_hits", handles.suspicious().size());

  t2_blue::DriverGuard drivers(sink);
  drivers.on_image_load({
      .path = "C:\\Windows\\Temp\\LabVulnDrv.sys",
      .sha256_hex = "lab_byovd_hash_001",
      .signer = "Contoso-Test",
      .boot_start = false,
  });

  t2_blue::ByovdBlocklist blocklist(sink);
  blocklist.add("lab_byovd_hash_001");
  const bool byovd_hit =
      blocklist.check_and_emit("lab_byovd_hash_001", "LabVulnDrv.sys");
  ac::proto_kv("byovd_blocked", byovd_hit);

  t2_blue::DeviceWatch devices(sink);
  devices.set_suspicious_names({"AcLabMemRw", "MemRw"});
  devices.on_device_open({kUiPid, "\\\\.\\AcLabMemRw"});

  t2_blue::CallbackIntegrity cbs(sink);
  cbs.set_baseline({4, 3, true});
  cbs.audit({2, 1, false});  // stripped

  for (const auto& ev : sink.events()) {
    risk.ingest(ev);
  }
  ac::proto_events(sink);
  ac::proto_kv("risk_score", risk.state().score);
  ac::proto_kv("block_ranked", risk.state().block_ranked);

  const bool detected = byovd_hit || risk.state().score >= 3.0;
  ac::proto_kv("T2_DETECTED", detected);
  std::puts(detected ? "prototype ok (t2 surface caught without handles)"
                     : "prototype FAIL");
  return detected ? 0 : 1;
}
