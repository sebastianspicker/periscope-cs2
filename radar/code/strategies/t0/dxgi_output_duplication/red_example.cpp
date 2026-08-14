#include "red_example.hpp"
#include <cstdio>

namespace examples::dxgi_output_duplication {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto capture = w.spawn("capture.exe");
  if (!w.proc(capture))
    return {false, steps, "capture spawn failed"};

  w.dxgi_output_dup_active = true;
  w.dxgi_output_frames_acquired = 5;
  w.dxgi_output_composite_rendered = true;
  w.desktop_duplication_active = true;
  w.desktop_duplication = true;

  const bool scar = w.dxgi_output_dup_active && w.dxgi_output_composite_rendered && w.dxgi_output_frames_acquired >= 5;
  if (!scar)
    return {false, steps, "DXGI output duplication scar failed"};

  std::printf("[T0 dxgi_output_duplication] step %d: %d frames acquired, composite rendered\n", ++steps, w.dxgi_output_frames_acquired);
  w.note("dxgi_output_duplication: output composite via IDXGIOutputDuplication");
  return {true, steps, "dxgi_output_duplication: IDXGIOutputDuplication active", w.dxgi_output_frames_acquired, true};
}

}  // namespace examples::dxgi_output_duplication
