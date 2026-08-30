#pragma once

// Sim educational red↔blue residual narratives for:
//   VMX/HV, BYOVD, DMA, SMM
// Multi-step red → detect → mitigate over sim::World scars (no hardware).

#include "sim/world.hpp"
#include "sim/narrative.hpp"

#include <string>

namespace depth {

// Aggregate outcome fields for `ResidualExampleResult` (lab narrative / tests).
struct ResidualExampleResult {
  bool red_achieved = false;
  bool blue_detected = false;
  bool blue_mitigated = false;
  std::string red_detail;
  std::string blue_detail;
  std::string family;  // "vmx" | "byovd" | "dma" | "smm"
};

/// VMX/HV residual: personal HV + bridge + trust off → policy/probe/bridge hunt.
ResidualExampleResult run_vmx_research_example(sim::World& w,
                                               sim::Narrator* n = nullptr);

/// BYOVD residual: known-bad signed driver + device IOCTL read, no game handle.
ResidualExampleResult run_byovd_research_example(sim::World& w,
                                                 sim::Narrator* n = nullptr);

/// DMA residual: off-box read when IOMMU off; blue enforces IOMMU + fog.
ResidualExampleResult run_dma_research_example(sim::World& w,
                                               sim::Narrator* n = nullptr);

/// SMM residual: firmware-class sim scar interacting with attest/trust aggregate.
ResidualExampleResult run_smm_research_example(sim::World& w,
                                               sim::Narrator* n = nullptr);

/// Run all four residual research examples on fresh arena clones; returns summary.
std::string run_all_residual_research_examples();

}  // namespace depth
