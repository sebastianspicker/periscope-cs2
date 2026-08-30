/// desktop_dup_overlay.cpp — Desktop duplication prototype (educational).
/// Closure for Gap 12: provides a functional desktop duplication demo.
///
/// Uses DXGI Output Duplication to capture the desktop.
/// This is a BLUE technique: detecting external overlay windows by
/// monitoring desktop composition and looking for layered/transparent
/// windows rendered on top of the game.
///
/// Educational: anti-cheats at T4 can detect radar overlays by
/// capturing the desktop and analyzing pixels rendered above the game.
/// This demo shows the capture side; the detection side is in
/// t4_blue/dma_defense.cpp and the desktop_duplication strategy pair.

#include "real/gpu/render_pipeline.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(_WIN32)
// This demo is a console tool; force the console subsystem so the CRT uses
// main() instead of WinMain, regardless of any transitively-linked GUI libs.
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

int main() {
  std::printf("=== Desktop Duplication Demo (Educational) ===\n");
  std::printf("\n");
#if defined(LR_ENABLE_EDUCATIONAL_MARKERS)
  std::printf("TECHNIQUE: DXGI Output Duplication captures the desktop\n");
  std::printf("frame buffer. Anti-cheats use this to detect overlay\n");
  std::printf("windows rendered on top of the game (ESP, radar, etc.).\n");
  std::printf("\n");
  std::printf("This is a BLUE technique. See:\n");
  std::printf("  - teams/t4_blue/dma_defense.cpp\n");
  std::printf("  - strategies/t4/desktop_duplication/\n");
  std::printf("\n");
#endif

  // Check if desktop duplication is supported
  auto supported = real::gpu::desktop_duplication_supported();
  if (supported) {
    std::printf("Desktop duplication: %s\n", *supported ? "supported" : "not supported");
  } else {
    std::printf("Desktop duplication check: %s\n", supported.error_msg.c_str());
  }

  // Attempt a capture
  int w = 0, h = 0;
  auto frame = real::gpu::capture_desktop_frame(w, h);
  if (!frame) {
    std::printf("\nCapture failed: %s\n", frame.error_msg.c_str());
    std::printf("\nThis is expected on most systems. Full DXGI desktop\n");
    std::printf("duplication requires:\n");
    std::printf("  1. A D3D11 device created with D3D11_CREATE_DEVICE_BGRA_SUPPORT\n");
    std::printf("  2. An IDXGIOutput1::DuplicateOutput call\n");
    std::printf("  3. Running on Windows 8+ with WDDM 1.2+ driver\n");
    std::printf("\nThe educational value is in understanding the detection\n");
    std::printf("technique, not the specific API call.\n");
    return 1;
  }

  std::printf("\nCapture succeeded: %dx%d pixels, %zu bytes (BGRA)\n",
              w, h, frame->size());
  std::printf("\nSCAR: DXGI Output Duplication is visible to the game via\n");
  std::printf("IDXGIAdapter::EnumOutputs — anti-cheats check for active\n");
  std::printf("duplication sessions on their output adapter.\n");
  return 0;
}
