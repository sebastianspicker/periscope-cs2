# Lab-safe residual research examples: VMX, BYOVD, DMA, SMM

Educational red↔blue narratives only. **No** real VMX rootkits, loadable vulnerable
drivers, physical DMA tooling, or SMM/firmware shellcode.

Entry points (shipped):

| Family | API | Strategy / team surface |
|--------|-----|-------------------------|
| **VMX/HV** | `depth::run_vmx_research_example` | `05_hypervisor`, T3 team libs, `TrustAggregator` |
| **BYOVD** | `depth::run_byovd_research_example` | `04_byovd`, T2 driver guard |
| **DMA** | `depth::run_dma_research_example` | `06_dma_hardware`, `t4::blue::DmaDefense` |
| **SMM** | `depth::run_smm_research_example` | Trust timeline + attest/EFI dual-view residual |

Run all four:

```text
./build/depth_tests          # includes residual section
# or call depth::run_all_residual_research_examples() from code
```

## Multi-step scar → detect → mitigate (sim)

### VMX / personal HV
1. **Red:** clear VBS/HVCI → `try_start_personal_hv` → bridge driver/device → `hv_read`.
2. **Blue:** `TrustAggregator` over ≥3 ticks + bridge hunt → **block ranked**.

### BYOVD
1. **Red:** load `byovd_known_bad` driver → mem-rw device → `device_ioctl_read` **without** game handle.
2. **Blue:** hash blocklist + known-bad flag + device surface → session risk.

### DMA
1. **Red:** `dma_device_present` + IOMMU off → `dma_read` (no local cheat process).
2. **Blue:** enable IOMMU + interest management + stream crypto (`LeakageScorer::mitigate_world`).

### SMM (firmware-class residual)
1. **Red:** `smm_residual=true`, attest/PCR fail, unexpected EFI entry, secure-kernel dirty view.
2. **Blue:** multi-tick `TrustAggregator` with `require_no_smm_residual` → ranked deny.

## Explicit non-goals
- Real VT-x/AMD-V payloads, InfinityHook kernels, physical FPGA DMA, SMM implants.
- Anything that leaves the `sim::World` battlefield.

## Why these exist
Primary research focus remains **software** T0–T3 legit radar. These four
residuals teach the **outer bounds** of the threat model and the same durable
blue tools: **trust policy aggregation**, **driver/device graphs**, **IOMMU**,
and **server fog-of-war**.
