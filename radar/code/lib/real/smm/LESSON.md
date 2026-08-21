# SMM / Firmware Residual (lib/real/smm)

## What this stack models

System Management Mode is the highest-privilege x86 execution environment.
A planted SMM handler can read process memory without OS handles, without
hypervisor cooperation, and below VBS/HVCI. Every operation still leaves
**scars** that blue can measure.

| Module | Role |
|--------|------|
| `smm_protocol` | Pure protocol math (checksums, communicate frames, PCR extend, TSEG decode) |
| `port_io` | x86 I/O port access (0xB2 SMI, 0x70/0x71 CMOS) |
| `smi` | SW-SMI trigger, CMOS dump, SMM communicate, MSR_SMI_COUNT, latency |
| `smram` | TSEGMB / SMRR discovery and lock state |
| `smm_channel` | Educational phys-read channel over communicate buffer |
| `acpi` | RSDP, table read/list, FADT SMI_CMD, DSDT override path |
| `efi` | Runtime table pointers, Get/SetVariable, variable list |
| `tpm` | PCR read/extend/event + local PCR-extend prediction |

## Red residual

1. Pre-boot or flash-stage SMM handler install (outside this usermode tree).
2. Shared buffer + `smm_communicate` / `smm_phys_read` at runtime.
3. Optional ACPI DSDT / EFI variable persistence hooks.
4. TPM PCR mutation to confuse naive attestation (cannot forge quotes without keys).

## Blue detection / mitigation

- **SMI count** (MSR 0x34) delta vs expected platform SMIs.
- **SMI latency** anomalies (`classify_smi_latency_us`).
- **TSEG locked** + `SMM_Code_Chk_En`.
- **Trust aggregator** `smm_residual` + attestation failure.
- **Secure Boot / measured boot** covering SMM and ACPI.
- **Block** WinRing0 / InpOut class lab drivers in production.

## Build

```text
cmake -DLR_ENABLE_REAL_SMM=ON ...
cmake --build . --target ac_real_smm
```

## Strategy map

| Pair | Real entry |
|------|------------|
| `62_smm_read_channel` | `smm_communicate` / `smm_phys_read` |
| `68_acpi_pm_mem_read` | `read_acpi_table` / FADT SMI_CMD |
| `22_input_provenance` | `tpm_read_pcr` |
| `110_hwid_spoof` | `efi_get_variable` |
