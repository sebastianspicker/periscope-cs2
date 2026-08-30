# Threat tiers (delivery)

Software ladder for external entity radar. T4 is residual (hardware-class). Primary pedagogy is T0 through T3.

## Capability matrix

| Capability | T0 | T1 | T2 | T3 | T4 |
|------------|----|----|----|----|-----|
| Entity radar from client memory | yes | yes | yes | yes | yes (off-box) |
| No game inject required | yes | yes | yes | yes | yes |
| No game write | yes | yes | yes | yes | yes |
| Evade simple usermode API hooks | | partial | yes | yes | n/a |
| Evade handle-graph detectors | | | often | yes | yes (no local process) |
| Evade "no unknown .sys" | | | BYOVD blurs | custom HV | n/a |
| Evade VBS/HVCI-required ranked | | | hard | hard | n/a |
| Worthless under strong fog | yes | yes | yes | yes | yes |

## T0: Usermode RPM

Path: src/lab_components/teams/t0_red/, src/lab_components/teams/t0_blue/

Team: t0_red::RpmBackend, CheatClient, EntityPipeline; t0_blue handle graph monitors.

Scar: foreign process with VM_READ on game.

Blue: handle graph, co-occurrence, weak UI heuristics.

Demos: duel_t0, proto_t0_*

Strategies: 01_external_rpm, handle hide, section map, GDI capture residuals, and related IDs.

## T1: Syscall-soft

Path: src/lab_components/teams/t1_red/, src/lab_components/teams/t1_blue/

Team: t1_red::SyscallBackend (attach_world sets via_syscall_path); t1_blue::SyscallAwareHandleMonitor.

Scar: still a handle; hooks on ntdll may be blind.

Blue: do not trust usermode hook presence; same graph and lineage.

Demos: duel_t1, proto_t1_*

Strategies: 02_indirect_syscall, manual map hide, stack spoof, ETW blind, hollow, SeDebug, and related IDs.

## T2: Kernel / BYOVD

Path: src/lab_components/teams/t2_red/, src/lab_components/teams/t2_blue/

Team: KernelRadar, ByovdSurface, IoctlReadBackend; KernelAc, driver guard, device watch, callback integrity.

Scar: memrw device, known-bad driver, or callback strip. Pure path has no usermode game handle.

Blue: blocklist, device open heuristics, callback baseline, SCM services.

Demos: duel_t2, proto_t2_*

Strategies: 03_kernel_ioctl, 04_byovd, callback strip/shadow, physmem, pool tag, ETW-TI, instr callback, and related IDs.

## T3: Personal hypervisor (sim)

Path: src/lab_components/teams/t3_red/, src/lab_components/teams/t3_blue/

Team: HvRadar::run_full_loop, HvReadBackend, BridgeSurface; PlatformAc::full, TrustPolicy, HvProbe, BridgeIntel, AttestationGate, depth::TrustAggregator.

Scar: VBS/HVCI cleared, personal_hv_active, bridge driver/device, optional EPT/timing/attest residual. Pure HV has no game handle.

Blue: ranked trust deny, multi-invariant HV probe, bridge intel, dual-view/EPT, residual flags (CI options, infinity hook, VTL1, EFI/ELAM, and related).

Demos: duel_t3, proto_t3_*

Strategies: 05_hypervisor, nested HV, attestation, EPT hide, timing spoof, EFI/ELAM, HVCI race, feature control MSR, and related IDs.

See also [RESIDUAL-RESEARCH.md](RESIDUAL-RESEARCH.md).

## T4: DMA / residual hardware (sim)

Path: src/lab_components/teams/t4_red/, src/lab_components/teams/t4_blue/

Team: DmaRadar::run_full_loop, DmaDefense::full.

Scar: dma_device_present, iommu_on=false, dma_read without local cheat process; optional capture/desktop/clone residuals.

Blue: platform IOMMU signal (weak), structural fog (LeakageScorer / interest), info-advantage residual.

Demos: duel_t4, proto_t4_*

Strategies: 06_dma_hardware, IOMMU policy, dual boot, capture CV/HID, lag/packet disambig, clipcursor, and related IDs.

## Engineering target for blue

1. T0 and T1 reliably detectable in-session via handles (and co-run).
2. T2 via driver/device/callback policy in-session or at boot.
3. T3 constrained by trust policy for competitive play plus residual HV/bridge sensors.
4. All tiers lose free data under interest management.
5. All tiers remain accountable via info-advantage and delayed multi-signal bans when scars are weak.
