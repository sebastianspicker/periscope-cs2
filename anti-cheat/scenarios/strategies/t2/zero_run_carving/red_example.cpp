#include "red_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::zero_run_carving {

namespace {
constexpr std::size_t kCarveOffset = 0x400;  // past the planted entity table
constexpr std::size_t kCarveBytes = 0x100;   // one page of carved code
constexpr std::uint8_t kCarveByte = 0xC3;    // lab payload marker byte
}  // namespace

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game || game->memory.size() < kCarveOffset + kCarveBytes) {
    std::printf("[T2 zero_run_carving] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T2 zero_run_carving] step %d: game pid=%u validated\n", ++steps, game_pid);

  const auto actor = w.spawn("zero-carve-actor.exe");
  if (!w.proc(actor)) {
    std::printf("[T2 zero_run_carving] FAIL: lab actor could not start\n");
    return {false, steps, "actor spawn verification failed"};
  }
  std::printf("[T2 zero_run_carving] step %d: actor pid=%u created\n", ++steps, actor);

  // Step: verify a zero run. The arena's planted region ends well before
  // 0x400, so the page range at kCarveOffset must still be all-zero.
  const bool zero_run = std::all_of(game->memory.begin() + static_cast<std::ptrdiff_t>(kCarveOffset),
                                    game->memory.begin() + static_cast<std::ptrdiff_t>(kCarveOffset + kCarveBytes),
                                    [](std::uint8_t b) { return b == 0; });
  if (!zero_run) {
    std::printf("[T2 zero_run_carving] FAIL: target page range is not a verified zero run\n");
    return {false, steps, "zero-run verification failed"};
  }
  std::printf("[T2 zero_run_carving] step %d: zero run verified at offset 0x%zx (%zu bytes)\n",
              ++steps, kCarveOffset, kCarveBytes);

  // Step: place code in the verified zero pages. No allocation API is called
  // (VirtualAllocEx/NtMapViewOfSection class) - the pages already exist.
  w.zero_run_carve_active = true;
  w.zero_run_pages_carved = 1;
  w.zero_run_bytes_written = static_cast<int>(kCarveBytes);
  w.zero_run_allocation_api_used = false;
  std::vector<std::uint8_t> payload(kCarveBytes, kCarveByte);
  if (!w.write_mem(game_pid, game->base + kCarveOffset, payload.data(), payload.size())) {
    std::printf("[T2 zero_run_carving] FAIL: carve write failed\n");
    return {false, steps, "carve write into zero pages failed"};
  }
  std::printf("[T2 zero_run_carving] step %d: code carved into zero pages (no allocation API)\n", ++steps);

  // Step: verify the carve persisted and that the VAD manager knows nothing
  // about it - the pages are PTE-valid but absent from every VAD reservation.
  bool readback_ok = true;
  for (std::size_t i = 0; i < kCarveBytes; ++i) {
    if (game->memory[kCarveOffset + i] != kCarveByte) readback_ok = false;
  }
  w.zero_run_vad_reserved = false;  // placement not reserved in the VAD manager
  if (!readback_ok || w.zero_run_vad_reserved) {
    std::printf("[T2 zero_run_carving] FAIL: carve not retained or VAD state wrong\n");
    return {false, steps, "carve retention / VAD-gap verification failed"};
  }
  std::printf("[T2 zero_run_carving] step %d: carve verified; placement absent from VAD manager\n", ++steps);

  const std::string detail =
      "zero_run_carving: pages=" + std::to_string(w.zero_run_pages_carved) +
      " bytes=" + std::to_string(w.zero_run_bytes_written) +
      " allocation_api=0 vad_reserved=0";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::zero_run_carving
