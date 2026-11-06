# Lab-safe residual research (VMX / BYOVD / DMA / SMM)

Educational red/blue narratives.

## Entry points

| Family | API | Related strategies / teams |
|--------|-----|----------------------------|
| VMX / HV | depth::run_vmx_research_example | 05_hypervisor, T3 HvRadar / PlatformAc, TrustAggregator |
| BYOVD | depth::run_byovd_research_example | 04_byovd, T2 KernelAc / blocklist |
| DMA | depth::run_dma_research_example | 06_dma_hardware, T4 DmaRadar / DmaDefense |
| SMM residual | depth::run_smm_research_example | Trust timeline + attest/EFI dual-view scars |
| All four | depth::run_all_residual_research_examples | Combined residual example |

## Multi-step scar to detect to mitigate (sim)

### VMX / personal HV

1. Red: clear VBS/HVCI, try_start_personal_hv, bridge driver/device, hv_read.
2. Blue: TrustAggregator multi-tick + bridge hunt to block ranked / high risk.

### BYOVD

1. Red: load byovd_known_bad, mem-rw device, device_ioctl_read without game handle.
2. Blue: hash blocklist + known-bad + device surface to session risk / mitigate.

### DMA

1. Red: dma_device_present + IOMMU off, dma_read (no local process).
2. Blue: enable IOMMU + interest management + stream crypto (LeakageScorer::mitigate_world).

### SMM (firmware-class residual)

1. Red: smm_residual, attest/PCR fail, unexpected EFI, SK dirty view.
2. Blue: multi-tick trust aggregate with require_no_smm_residual to ranked deny.

## Relation to tiers

T3/T4 team libraries are the deep educational implementations. Residual research helpers are compact four-family examples for lectures.
