// radar_ring_test.cpp — R3: snapshot ring, scene watermark, freshness gate.

#include "radar/snapshot_ring.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

using radar_domain::SceneWatermark;
using radar_domain::SnapshotRing;
using radar_domain::fresh_by_age;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

struct Payload {
  int value = 0;
  int mirror = 0;  // ~value: correlated word exposes a torn copy
  char tag[16]{};
};

void test_publish_read_latest() {
  SnapshotRing<Payload, 8> ring;
  Payload out;
  check(!ring.read_latest(out), "empty ring reads false");

  Payload first;
  first.value = 1;
  std::snprintf(first.tag, sizeof(first.tag), "first");
  ring.publish(first);
  check(ring.read_latest(out), "published frame readable");
  check(out.value == 1, "payload value");
  check(std::string(out.tag) == "first", "payload tag");

  Payload second;
  second.value = 2;
  ring.publish(second);
  check(ring.read_latest(out) && out.value == 2, "latest wins");
  check(ring.published() == 2, "publication count");
}

void test_more_publications_than_slots() {
  SnapshotRing<Payload, 3> ring;
  for (int i = 0; i < 10; ++i) {
    Payload p;
    p.value = i;
    ring.publish(p);
  }
  Payload out;
  check(ring.read_latest(out) && out.value == 9, "lapping keeps newest");
}

void test_reader_laps_safely() {
  // A reader copying a slot that the writer rewrites must either get the old
  // or the new content, never a torn one; the retry loop covers it.
  SnapshotRing<Payload, 2> ring;
  std::atomic<bool> stop{false};
  std::atomic<int> reads{0};
  std::atomic<int> torn_failures{0};

  Payload writer_frame;
  std::snprintf(writer_frame.tag, sizeof(writer_frame.tag), "w");
  std::thread writer([&] {
    int value = 0;
    while (!stop.load(std::memory_order_relaxed)) {
      writer_frame.value = value;
      writer_frame.mirror = ~value;
      ++value;
      ring.publish(writer_frame);
    }
  });

  std::thread reader([&] {
    while (!stop.load(std::memory_order_relaxed)) {
      Payload out;
      if (ring.read_latest(out)) {
        if (out.mirror != ~out.value) {
          torn_failures.fetch_add(1, std::memory_order_relaxed);
        }
        reads.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  stop.store(true, std::memory_order_relaxed);
  writer.join();
  reader.join();

  check(torn_failures.load() == 0, "no torn reads observed");
  check(reads.load() > 0, "reader made progress");
}

void test_scene_watermark() {
  SceneWatermark watermark;
  const std::uint64_t scene = watermark.current();
  check(watermark.is_fresh(scene), "current serial fresh");
  check(!watermark.is_fresh(scene - 1), "older serial stale");

  const std::uint64_t next = watermark.invalidate();
  check(next == scene + 1, "invalidate bumps serial");
  check(!watermark.is_fresh(scene), "old scene invalid after flip");
  check(watermark.is_fresh(next), "new scene fresh");
}

void test_freshness_gate() {
  check(fresh_by_age(1000, 1100, 150), "inside window");
  check(!fresh_by_age(1000, 1151, 150), "outside window");
  check(!fresh_by_age(1100, 1000, 150), "future sample stale");
  check(fresh_by_age(1000, 1000, 150), "exact boundary fresh");
}

}  // namespace

int main() {
  test_publish_read_latest();
  test_more_publications_than_slots();
  test_reader_laps_safely();
  test_scene_watermark();
  test_freshness_gate();

  if (failures != 0) {
    std::printf("%d ring check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_ring_test: all checks passed\n");
  return 0;
}
