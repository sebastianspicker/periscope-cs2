// bridge_surface.cpp — T3 red HV path using World.trust posture flags.
// Simulated personal HV / attestation fields

#include "t3_red/bridge_surface.hpp"

namespace t3_red {

// BridgeSurface::open_lab: Open lab HV bridge surface residuals.
ac::Status BridgeSurface::open_lab() {
  open_ = true;
  last_.open = true;
  last_.detail = "lab_mark_only";
  return ac::Status::Ok;
}

// BridgeSurface::close: Close bridge surface residual on World.
void BridgeSurface::close() {
  open_ = false;
  last_.open = false;
}

// BridgeSurface::open_on_world: Open bridge surface on a specific World.
BridgeReport BridgeSurface::open_on_world(sim::World& w,
                                          const std::string& driver,
                                          const std::string& device) {
  last_ = {};
  last_.driver_name = driver;
  last_.device_name = device;
  device_ = device;

  w.load_driver(sim::Driver{driver, "bridge_hash_lab", "unknown",
                            /*boot_start=*/true,
                            /*byovd_known_bad=*/false,
                            /*is_ac=*/false,
                            /*is_bridge=*/true,
                            /*provides_mem_rw=*/false,
                            /*load_order=*/5});
  last_.driver_loaded = true;

  w.create_device(sim::Device{device, driver, /*mem_rw_ioctl=*/true});
  last_.device_created = true;

  open_ = true;
  last_.open = true;
  last_.detail = "bridge driver=" + driver + " device=" + device;
  w.note("t3 BridgeSurface " + last_.detail);
  return last_;
}

}  // namespace t3_red
