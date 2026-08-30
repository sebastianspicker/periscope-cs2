# Vulnerable Driver + DMA — Final Prototype

## gdrv.sys Exact Match

Binary analysis from gdrv.sys v1.0.6.9:

| Address | gdrv.sys | vuln_driver.c |
|---------|----------|---------------|
| +0x2840 | `MmMapIoSpace` PHYS_READ | `GdrvPhysRead()` |
| +0x2900 | `MmMapIoSpace` PHYS_WRITE | `GdrvPhysWrite()` |
| +0x29C0 | `KeStackAttachProcess` VIRT_READ | `GdrvVirtRead()` |
| IOCTL 0xC3502000 | CTL_CODE(0x22,0x800,3,0) | same |
| IOCTL 0xC3502008 | CTL_CODE(0x22,0x802,3,0) | same |
| Method | METHOD_NEITHER | METHOD_NEITHER |

## Device path (client ↔ driver)

| Kernel | Usermode |
|--------|----------|
| `\Device\gdrv` | `\\.\gdrv` |
| `\DosDevices\gdrv` | (symlink) |

Shared layouts: `example_vulnerable/gdrv_abi.h`.

## Extended IOCTL surface (educational)

| Code | Handler | Real kernel path |
|------|---------|------------------|
| 0xC3502000–0C | phys/virt R/W | MmMapIoSpace / KeStackAttachProcess |
| 0xC3502010 | ENTITY_WALK | attach-once entity walk |
| 0xC3502014 | PROCESS_SCAN | ZwQuerySystemInformation full walk |
| 0xC3502018 | CALLBACK_STRIP | resolve Psp*Notify arrays / Cm list |
| 0xC350201C | MODULE_LIST | PEB LDR walk |

## PCILeech 2.0 B1 Exact Match

| Parameter | PCILeech 2.0 B1 | fpga_dma.h |
|-----------|------------------|------------|
| Vendor:Device | 0x10EE:0x9034 | same |
| Firmware version | 0x20220214 | same |
| Descriptor size | 32 bytes | same |
| Read latency (4KB) | ~2.5 us | PCILeech_READ_4KB_us |
| Descriptor fetch | ~0.8 us | PCILeech_DESC_FETCH_us |
| PCIe gen | 2 x4 @ 5.0 GT/s | PCILeech_GEN2_RATE/LANES |

## Prototype Demo

```bash
# Host ABI tests (no driver load)
cd radar/examples/drivers/example_vulnerable
cmake -S . -B build -DBUILD_DRIVER=OFF && cmake --build build
./build/bin/gdrv_host_tests
./build/bin/user_test --info
./build/bin/user_test --list

# T2 BYOVD client (requires gdrv.sys loaded)
user_test.exe --t2-cs2    # Read CS2 PE via IOCTL
user_test.exe --phys 0x0  # Read physical memory
```
