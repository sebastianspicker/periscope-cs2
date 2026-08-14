#include "red_example.hpp"
#include <cstdio>

namespace examples::callback_shadow_kernel {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto driver = w.spawn("shadow-driver.sys");
  if (!w.proc(driver))
    return {false, steps, "driver spawn failed"};

  w.callback_kernel_shadow_active = true;
  w.callback_kernel_process_notify_restored = w.process_notify_true;
  w.callback_kernel_image_notify_restored = w.image_notify_true;
  w.callback_kernel_stripped_during_ioctl = true;

  w.process_notify = 0;
  w.image_notify = 0;
  w.ac_callback_present = false;
  w.callback_shadow_active = true;

  const bool scar = w.callback_kernel_shadow_active && w.callback_kernel_stripped_during_ioctl;
  if (!scar)
    return {false, steps, "callback shadow kernel scar failed"};

  std::printf("[T2 callback_shadow_kernel] step %d: kernel callbacks stripped during BYOVD IOCTL\n", ++steps);
  w.note("callback_shadow_kernel: kernel callback strip for BYOVD");
  return {true, steps, "callback_shadow_kernel: callbacks stripped", w.callback_kernel_process_notify_restored, w.callback_kernel_image_notify_restored, true};
}

}  // namespace examples::callback_shadow_kernel
