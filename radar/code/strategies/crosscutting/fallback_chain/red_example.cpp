#include "red_example.hpp"

namespace examples::fallback_chain {

// RED: try HV under VBS (fail) → kernel memrw → RPM handle (reliability over purity).
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("fallback-radar.exe");
  // HV fails under VBS.
  w.trust.vbs = true;
  w.trust.hvci = true;
  r.hv_failed = !w.try_start_personal_hv("lab-hv");
  // Kernel path.
  w.load_driver(sim::Driver{"fallback-mem.sys", "fbsha", "unknown", false, false,
                            false, false, true});
  w.create_device(sim::Device{"\\\\.\\FallbackMem", "fallback-mem.sys", true});
  r.kernel_ok = true;
  for (const auto& d : w.drivers) {
    if (d.provides_mem_rw && !d.is_ac) r.kernel_ok = true;
  }
  // RPM fallback handle.
  r.rpm_ok = w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  r.achieved = r.hv_failed && r.kernel_ok && r.rpm_ok;
  r.detail = "fallback_chain red hv_fail=" + std::to_string(r.hv_failed ? 1 : 0) +
             " kernel=" + std::to_string(r.kernel_ok ? 1 : 0) +
             " rpm=" + std::to_string(r.rpm_ok ? 1 : 0);
  w.note(r.detail);
  return r;
}

}  // namespace examples::fallback_chain
