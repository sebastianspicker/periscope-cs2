// ac_unit_test.cpp — drives the shipped code/lib/ac public surface end-to-end.
// No re-implementation of RiskAggregator / to_string; asserts real library behavior.

#include "ac/memory_backend.hpp"
#include "ac/proto_log.hpp"
#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "ac/types.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++fails;
  } else {
    std::printf("ok: %s\n", msg);
  }
}

bool non_empty_stable(std::string_view s) {
  if (s.empty()) return false;
  // Reject unknown/fallback labels for valid enumerators.
  if (s == "T?" || s == "unknown") return false;
  if (s.find("unknown_") == 0) return false;
  return true;
}

// Minimal concrete backend: proves IMemoryBackend is a usable non-stub contract.
class LabMockBackend final : public ac::IMemoryBackend {
 public:
  ac::Tier tier() const override { return ac::Tier::T0_UsermodeRpm; }
  std::string_view name() const override { return "lab-mock"; }

  ac::Status attach(std::uint32_t target_id) override {
    if (target_id == 0) return ac::Status::InvalidArgument;
    attached_ = true;
    target_id_ = target_id;
    return ac::Status::Ok;
  }

  void detach() override {
    attached_ = false;
    target_id_ = 0;
  }

  bool is_attached() const override { return attached_; }

  ac::ReadResult read(const ac::ReadRequest& req) override {
    ac::ReadResult out;
    if (!attached_) {
      out.status = ac::Status::Unavailable;
      return out;
    }
    if (req.size == 0 || req.address == 0) {
      out.status = ac::Status::InvalidArgument;
      return out;
    }
    // Lab payload: echo address low bytes then a fixed pattern of length size.
    out.bytes.resize(req.size);
    for (std::size_t i = 0; i < req.size; ++i) {
      out.bytes[i] = static_cast<std::uint8_t>((req.address + i) & 0xFFu);
    }
    out.status = ac::Status::Ok;
    return out;
  }

  std::uint32_t target_id() const { return target_id_; }

 private:
  bool attached_ = false;
  std::uint32_t target_id_ = 0;
};

void test_enum_to_string() {
  // Tier — all defined enumerators.
  expect(non_empty_stable(ac::to_string(ac::Tier::T0_UsermodeRpm)), "to_string Tier T0");
  expect(non_empty_stable(ac::to_string(ac::Tier::T1_SyscallSoft)), "to_string Tier T1");
  expect(non_empty_stable(ac::to_string(ac::Tier::T2_KernelByovd)), "to_string Tier T2");
  expect(non_empty_stable(ac::to_string(ac::Tier::T3_Hypervisor)), "to_string Tier T3");
  expect(ac::to_string(ac::Tier::T0_UsermodeRpm) == "T0", "to_string Tier T0 label");
  expect(ac::to_string(ac::Tier::T3_Hypervisor) == "T3", "to_string Tier T3 label");

  // Status — all defined enumerators.
  expect(non_empty_stable(ac::to_string(ac::Status::Ok)), "to_string Status Ok");
  expect(non_empty_stable(ac::to_string(ac::Status::NotImplemented)),
         "to_string Status NotImplemented");
  expect(non_empty_stable(ac::to_string(ac::Status::Denied)), "to_string Status Denied");
  expect(non_empty_stable(ac::to_string(ac::Status::Unavailable)),
         "to_string Status Unavailable");
  expect(non_empty_stable(ac::to_string(ac::Status::InvalidArgument)),
         "to_string Status InvalidArgument");
  expect(non_empty_stable(ac::to_string(ac::Status::LabOnly)), "to_string Status LabOnly");
  expect(ac::to_string(ac::Status::Ok) == "ok", "to_string Status ok label");

  // HandleAcquisitionModel
  const ac::HandleAcquisitionModel handle_vals[] = {
      ac::HandleAcquisitionModel::None,
      ac::HandleAcquisitionModel::DirectOpenProcess,
      ac::HandleAcquisitionModel::NtOpenProcess,
      ac::HandleAcquisitionModel::DirectSyscall,
      ac::HandleAcquisitionModel::HandleDuplicate,
      ac::HandleAcquisitionModel::KernelDriver,
      ac::HandleAcquisitionModel::HijackProxy,
      ac::HandleAcquisitionModel::DmaPhysical,
  };
  for (auto v : handle_vals) {
    expect(non_empty_stable(ac::to_string(v)), "to_string HandleAcquisitionModel");
  }
  expect(ac::to_string(ac::HandleAcquisitionModel::DirectOpenProcess) ==
             "direct_open_process",
         "to_string HandleAcquisitionModel DirectOpenProcess");

  // MemoryAcquisitionModel
  const ac::MemoryAcquisitionModel mem_vals[] = {
      ac::MemoryAcquisitionModel::DirectRpm,   ac::MemoryAcquisitionModel::Syscall,
      ac::MemoryAcquisitionModel::KernelIoctl, ac::MemoryAcquisitionModel::HvHypercall,
      ac::MemoryAcquisitionModel::DmaPhysical, ac::MemoryAcquisitionModel::HijackProxy,
  };
  for (auto v : mem_vals) {
    expect(non_empty_stable(ac::to_string(v)), "to_string MemoryAcquisitionModel");
  }

  // DisguiseProfile
  const ac::DisguiseProfile disguise_vals[] = {
      ac::DisguiseProfile::None,           ac::DisguiseProfile::SteamOverlay,
      ac::DisguiseProfile::DiscordOverlay, ac::DisguiseProfile::RivaTuner,
      ac::DisguiseProfile::ObsStudio,      ac::DisguiseProfile::NvidiaShadowplay,
      ac::DisguiseProfile::GenericMonitor,
  };
  for (auto v : disguise_vals) {
    expect(non_empty_stable(ac::to_string(v)), "to_string DisguiseProfile");
  }

  // DefenseLayer
  const ac::DefenseLayer defense_vals[] = {
      ac::DefenseLayer::ProcessIsolation,        ac::DefenseLayer::ProxyMemoryAccess,
      ac::DefenseLayer::HardwareMonitorDisguise, ac::DefenseLayer::ForensicTraceRemoval,
      ac::DefenseLayer::PeLegitimacy,            ac::DefenseLayer::SystemNormalization,
      ac::DefenseLayer::BehavioralJitter,
  };
  for (auto v : defense_vals) {
    expect(non_empty_stable(ac::to_string(v)), "to_string DefenseLayer");
  }

  // ObservationView
  const ac::ObservationView obs_vals[] = {
      ac::ObservationView::HandleTable, ac::ObservationView::ModuleList,
      ac::ObservationView::MemoryPattern, ac::ObservationView::InProcess,
      ac::ObservationView::Behavioral,  ac::ObservationView::PostExecution,
  };
  for (auto v : obs_vals) {
    expect(non_empty_stable(ac::to_string(v)), "to_string ObservationView");
  }

  // EventKind
  const ac::EventKind event_vals[] = {
      ac::EventKind::HandleToGame,     ac::EventKind::ProcessCoRun,
      ac::EventKind::DriverLoad,       ac::EventKind::ByovdBlocked,
      ac::EventKind::DeviceOpen,       ac::EventKind::TrustPolicyFail,
      ac::EventKind::HvProbeAnomaly,   ac::EventKind::BridgeSuspected,
      ac::EventKind::InfoAdvantageHit, ac::EventKind::Generic,
  };
  for (auto v : event_vals) {
    expect(non_empty_stable(ac::to_string(v)), "to_string EventKind");
  }
  expect(ac::to_string(ac::EventKind::ByovdBlocked) == "byovd_blocked",
         "to_string EventKind ByovdBlocked");
}

void test_telemetry_and_risk() {
  ac::MemoryTelemetrySink sink;
  ac::RiskAggregator risk;

  ac::TelemetryEvent e1{ac::EventKind::HandleToGame, ac::Tier::T0_UsermodeRpm, 100,
                        200, "handle", 2.0};
  ac::TelemetryEvent e2{ac::EventKind::DriverLoad, ac::Tier::T2_KernelByovd, 100, 0,
                        "byovd-ish", 2.5};
  ac::TelemetryEvent e3{ac::EventKind::InfoAdvantageHit, ac::Tier::T0_UsermodeRpm, 100,
                        0, "preaim", 3.0};
  ac::TelemetryEvent e4{ac::EventKind::ByovdBlocked, ac::Tier::T2_KernelByovd, 0, 0,
                        "blocked-hash", 1.5};
  // Empty detail exercises default reason path.
  ac::TelemetryEvent e5{ac::EventKind::TrustPolicyFail, ac::Tier::T3_Hypervisor, 0, 0,
                        "", 0.5};

  sink.emit(e1);
  sink.emit(e2);
  sink.emit(e3);
  sink.emit(e4);
  sink.emit(e5);

  risk.ingest(e1);
  risk.ingest(e2);
  risk.ingest(e3);
  risk.ingest(e4);
  risk.ingest(e5);

  const double expected_score =
      e1.risk_delta + e2.risk_delta + e3.risk_delta + e4.risk_delta + e5.risk_delta;

  expect(sink.events().size() == 5, "telemetry sink retains 5 events");
  expect(sink.events()[0].kind == ac::EventKind::HandleToGame,
         "telemetry order first is HandleToGame");
  expect(sink.events()[1].kind == ac::EventKind::DriverLoad,
         "telemetry order second is DriverLoad");
  expect(sink.events()[2].kind == ac::EventKind::InfoAdvantageHit,
         "telemetry order third is InfoAdvantageHit");
  expect(sink.events()[3].detail == "blocked-hash", "telemetry retains detail");
  expect(sink.events()[4].kind == ac::EventKind::TrustPolicyFail,
         "telemetry order fifth is TrustPolicyFail");

  const auto& st = risk.state();
  expect(std::fabs(st.score - expected_score) < 1e-9,
         "risk score equals sum of risk_deltas");
  expect(st.event_count == 5, "risk event_count == 5");
  expect(st.distinct_kinds == 5, "risk distinct_kinds == 5");
  expect(st.multi_signal, "risk multi_signal from multi-kind stream");
  expect(st.reasons.size() == 5, "risk retains one reason per event");
  expect(st.reasons[0] == "handle", "risk reason from detail");
  expect(st.reasons[4] == "trust_policy_fail", "risk default reason for empty detail");
  expect(st.flag_overwatch, "risk flag_overwatch from InfoAdvantageHit / score");
  expect(st.block_ranked, "risk block_ranked from ByovdBlocked/TrustPolicyFail");

  // Reset clears all aggregate fields.
  risk.reset();
  expect(risk.state().score == 0.0, "risk reset score");
  expect(risk.state().event_count == 0, "risk reset event_count");
  expect(risk.state().distinct_kinds == 0, "risk reset distinct_kinds");
  expect(!risk.state().multi_signal, "risk reset multi_signal");
  expect(!risk.state().block_ranked, "risk reset block_ranked");
  expect(!risk.state().flag_overwatch, "risk reset flag_overwatch");
  expect(risk.state().reasons.empty(), "risk reset reasons");

  sink.clear();
  expect(sink.events().empty(), "telemetry clear empties sink");

  // Two-event multi-signal without overwatch-forcing kinds until score threshold.
  ac::TelemetryEvent mild1{ac::EventKind::DeviceOpen, ac::Tier::T2_KernelByovd, 1, 0,
                           "dev", 1.0};
  ac::TelemetryEvent mild2{ac::EventKind::ProcessCoRun, ac::Tier::T0_UsermodeRpm, 1, 0,
                           "co", 1.0};
  risk.ingest(mild1);
  risk.ingest(mild2);
  expect(std::fabs(risk.state().score - 2.0) < 1e-9, "mild multi score sum");
  expect(risk.state().multi_signal, "mild multi_signal flag");
  expect(risk.state().distinct_kinds == 2, "mild distinct_kinds");
  expect(!risk.state().flag_overwatch, "mild no overwatch below threshold");
  expect(!risk.state().block_ranked, "mild no block_ranked");
}

void test_memory_backend_contract() {
  LabMockBackend be;
  expect(be.tier() == ac::Tier::T0_UsermodeRpm, "backend tier identity");
  expect(be.name() == "lab-mock", "backend name identity");
  expect(!be.is_attached(), "backend starts detached");

  expect(be.attach(0) == ac::Status::InvalidArgument, "backend reject target 0");
  expect(be.attach(42) == ac::Status::Ok, "backend attach ok");
  expect(be.is_attached(), "backend is_attached after attach");
  expect(be.target_id() == 42, "backend stores target_id");

  ac::ReadRequest bad{};
  bad.address = 0;
  bad.size = 4;
  auto rbad = be.read(bad);
  expect(rbad.status == ac::Status::InvalidArgument, "backend read rejects addr 0");

  ac::ReadRequest req{};
  req.address = 0x1000;
  req.size = 4;
  auto rr = be.read(req);
  expect(rr.status == ac::Status::Ok, "backend read ok");
  expect(rr.bytes.size() == 4, "backend read size");
  // Lab mock fills byte i with low 8 bits of (address + i).
  expect(rr.bytes[0] == 0x00 && rr.bytes[1] == 0x01 && rr.bytes[2] == 0x02 &&
             rr.bytes[3] == 0x03,
         "backend read payload");

  be.detach();
  expect(!be.is_attached(), "backend detach");
  auto rdet = be.read(req);
  expect(rdet.status == ac::Status::Unavailable, "backend read while detached");
}

void test_entity_and_read_types() {
  ac::EntitySnapshot e(7, ac::Vec3{1.f, 2.f, 3.f}, 2, true);
  expect(e.id == 7, "EntitySnapshot id");
  expect(e.origin.x == 1.f && e.origin.y == 2.f && e.origin.z == 3.f,
         "EntitySnapshot origin");
  expect(e.team == 2 && e.alive, "EntitySnapshot team/alive");

  ac::ReadResult def{};
  expect(def.status == ac::Status::NotImplemented, "ReadResult default status");
  expect(def.bytes.empty(), "ReadResult default empty bytes");

  ac::ObservationVerdict v{ac::ObservationView::HandleTable, true, 0.85,
                           "suspicious handle"};
  expect(v.anomaly_detected && v.confidence > 0.5, "ObservationVerdict fields");
  expect(ac::to_string(v.view) == "handle_table", "ObservationVerdict view label");
}

void test_proto_log_helpers() {
  // Smoke: helpers format without crashing; status/tier conversion used live.
  ac::proto_banner("BLUE", "T0", "ac_unit_test");
  ac::proto_status("attach", ac::Status::Ok);
  ac::proto_line("backend", "lab-mock");
  ac::proto_kv("score", 7.5);
  ac::proto_kv("events", static_cast<std::size_t>(3));
  ac::proto_kv("multi", true);

  ac::MemoryTelemetrySink sink;
  sink.emit({ac::EventKind::HandleToGame, ac::Tier::T0_UsermodeRpm, 1, 2, "h", 1.0});
  ac::proto_events(sink);

  ac::RiskAggregator risk;
  risk.ingest({ac::EventKind::HandleToGame, ac::Tier::T0_UsermodeRpm, 1, 2, "h", 2.0});
  risk.ingest({ac::EventKind::InfoAdvantageHit, ac::Tier::T0_UsermodeRpm, 1, 0, "i",
               3.5});
  ac::proto_risk(risk.state());
  expect(risk.state().multi_signal, "proto_log path still multi_signal");
}

// Run the full consumer suite once; used twice for consistency check.
int run_consumer_once(const char* pass_label) {
  std::printf("\n=== ac consumer pass: %s ===\n", pass_label);
  const int before = fails;
  test_enum_to_string();
  test_telemetry_and_risk();
  test_memory_backend_contract();
  test_entity_and_read_types();
  test_proto_log_helpers();
  return fails - before;
}

}  // namespace

int main() {
  const int fail_a = run_consumer_once("A");
  const int fail_b = run_consumer_once("B");
  expect(fail_a == 0, "consumer pass A zero failures");
  expect(fail_b == 0, "consumer pass B zero failures");
  expect(fail_a == fail_b, "consumer passes A/B consistent");

  if (fails != 0) {
    std::fprintf(stderr, "%d failure(s) in ac_unit_test\n", fails);
    return 1;
  }
  std::puts("all ac_unit_test checks passed");
  return 0;
}
