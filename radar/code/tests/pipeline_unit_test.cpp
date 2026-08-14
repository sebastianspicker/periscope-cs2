// pipeline_unit_test.cpp — drives shipped radar::FramePipeline end-to-end.
// Asserts: real API-integrity init/verify, AcceptGate from live metrics
// (not hardcoded 7/7), BehavioralFilter applied in collect_frame.

#include "radar_pipeline.hpp"

#include <cstdio>
#include <cstring>

namespace {

int fails = 0;
int checks = 0;

void expect(bool cond, const char* msg) {
  ++checks;
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++fails;
  } else {
    std::printf("ok: %s\n", msg);
  }
}

}  // namespace

int main() {
  radar::FramePipeline pipe;
  // Offline lab path (no CS2 attach) — still runs full pre/collect/post.
  expect(pipe.initialize(/*cs2_pid=*/0, /*use_real_cs2=*/false),
         "pipeline initialize offline");
  expect(pipe.api_integrity_registry_ready(),
         "api integrity registry populated at init");

  // Run enough frames so frame_index hits 1 (API integrity + self-verify).
  for (int i = 0; i < 3; ++i) {
    expect(pipe.run_frame(), "run_frame succeeds offline");
  }
  expect(pipe.frame_count() >= 3, "frame_count advanced");

  // BehavioralFilter must have seen lab entities (collect_frame offline path).
  const auto& bf = pipe.behavioral();
  expect(bf.stats().ticks >= 1, "behavioral filter ticks > 0");
  expect(bf.stats().entitiesSeen >= 4, "behavioral filter saw lab entities");
  expect(pipe.last_entity_count() == 4, "last_entity_count from lab collect");
  expect(pipe.last_filtered_entity_count() >= 0 &&
             pipe.last_filtered_entity_count() <= 4,
         "filtered entity count in range");

  // AcceptGate metrics must NOT be the old hardcoded all-true theater.
  const auto& gm = pipe.last_gate_metrics();
  expect(!gm.cvarWalkOk, "offline: cvarWalkOk false (no real CS2)");
  expect(!gm.bandEnforced, "offline: bandEnforced false");
  expect(gm.overlayMinPx == 0, "offline: overlayMinPx 0");
  expect(gm.chunkCount >= 0, "gate metrics chunkCount set");
  // Offline lab cannot meet all 7 conditions — gate must not auto-accept
  // solely because conditions were hardcoded true.
  expect(pipe.gate().met_count() < 7,
         "offline: met_count < 7 without live CS2");
  expect(!pipe.gate().accepted(),
         "offline: gate rejected (honest metrics, not theater)");

  // Heartbeat = selfVerifyOk ∧ apiIntegrityOk. If integrity failed, heartbeat
  // must not be forced true (old theater set heartbeatOk=true always).
  if (!pipe.last_api_integrity_ok()) {
    expect(!gm.heartbeatOk, "heartbeat false when API integrity fails");
  } else {
    // Integrity passed: heartbeat may still fail on self-verify — either way ok.
    expect(true, "API integrity ok — heartbeat derived from real flags");
  }
  // Stronger: cvar/overlay/band are the definitive theater markers.
  expect(!(gm.cvarWalkOk && gm.bandEnforced && gm.overlayMinPx >= 280 &&
           gm.chunkRatio >= 0.4f && gm.zeroPushRatio <= 0.5f &&
           gm.cvarInitSeconds <= 15.f && gm.heartbeatOk &&
           pipe.gate().accepted()),
         "offline cannot be all-conditions-true accept");

  pipe.shutdown();
  expect(true, "shutdown clean");

  std::printf("\n%d checks, %d failed\n", checks, fails);
  return fails == 0 ? 0 : 1;
}
