# T4 PCIe DMA — FPGA-Based CS2 Memory Reader

**Educational prototype** demonstrating **T4 hardware-level memory access** via PCIe DMA using an FPGA (hardened PCILeech-class pattern). Reads CS2 entity data with **zero OS involvement** — no process handle, no kernel driver, no system calls.

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│  Separate Machine (Radar)                                            │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  host_sim / Cs2DmaReader                                      │   │
│  │  - PageTableWalker::translate(va, cr3) -> pa                  │   │
│  │  - Cs2DmaReader::read_entities() -> DmaEntityData[]            │   │
│  │  - NO OS calls, NO process handle                             │   │
│  └──────────────┬───────────────────────────────────────────────┘   │
└─────────────────┼───────────────────────────────────────────────────┘
                  │ PCIe gen2 x4 cable
                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│  FPGA Board (Xilinx Artix-7 XC7A35T)                                │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  Hardened DMA firmware                                        │   │
│  │  - PCIe endpoint: Vendor 0x1B73, Device 0xA0B0 (spoofed)     │   │
│  │  - BAR0: 128KB register map (offsets + REG_BASE_SHIFT 0x80)   │   │
│  │  - DMA descriptor ring (36-byte descriptors)                  │   │
│  │  - Data buffer at REG_DATA_BUFFER                             │   │
│  │  - Perform PCIe memory read TLPs to host physical addresses   │   │
│  └──────────────┬───────────────────────────────────────────────┘   │
└─────────────────┼───────────────────────────────────────────────────┘
                  │ PCIe bus / NTB
                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Target Machine (CS2)                                                │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  Physical Memory                                               │   │
│  │  - cs2.exe at some physical page                               │   │
│  │  - EPROCESS, page tables, code, heap all in physical RAM       │   │
│  │  - DMA reads bypass CPU, bypass OS, bypass all security        │   │
│  │  - Handle table: ZERO handles from the radar machine           │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  ⚠ Requires IOMMU/VT-d for protection from DMA attacks             │
└──────────────────────────────────────────────────────────────────────┘
```

## Key Difference: T4 DMA vs All Lower Tiers

| Tier | Method | OS Artifact | Detection |
|------|--------|-------------|-----------|
| T0 | OpenProcess + RPM | VM_READ handle in radar's table | Handle graph scan |
| T1 | Direct syscall | VM_READ handle in radar's table | Handle graph scan |
| T2 | BYOVD IOCTL | Device handle to gdrv.sys | Device watch, IOCTL monitor |
| T3 | Hypervisor EPT | VMX root mode in MSRs | CPUID timing, VBS checks |
| **T4** | **PCIe DMA FPGA** | **NOTHING** | **IOMMU only** |

## FPGA Register Map (BAR0, 128KB, REG_BASE_SHIFT=0x80)

| Offset | Name | Access | Description |
|--------|------|--------|-------------|
| 0x0080 | VERSION | RO | Firmware version (0x20260726) |
| 0x0084 | SCRATCH | RW | Scratch register |
| 0x0088 | STATUS | RO | Status flags |
| 0x008C | CTRL | RW | Control flags |
| 0x0090 | DESC_ADDR_LO | RW | Descriptor ring PA (low 32) |
| 0x0094 | DESC_ADDR_HI | RW | Descriptor ring PA (high 32) |
| 0x0098 | DESC_COUNT | RW | Number of descriptors |
| 0x009C | DESC_COMPLETED | RO | Completed descriptors |
| 0x00A0 | DMA_CTRL | WO | Doorbell — start DMA |
| 0x00A4 | DMA_ABORT | WO | Abort DMA |
| 0x00A8 | DMA_STATUS | RO | DMA engine status |
| 0x00AC | DMA_ERROR | RO | Error code |
| 0x00B0 | ACS_BYPASS | RW | ACS bypass control |
| 0x00B4 | IOMMU_STATE | RO | IOMMU detection |
| 0x00B8 | DMA_LATENCY | RO | Last DMA latency (us) |
| 0x00C0 | BYTES_XFER | RO | Total bytes transferred |
| 0x1080 | DATA_BUFFER | RW | 64KB data buffer |

## DMA Descriptor (36 bytes, scatter-gather)

```
Offset  Field     Description
0x00    src_addr  Source physical address
0x08    dst_addr  Destination (FPGA buffer)
0x10    size      Transfer size (bytes, max 4096)
0x14    flags     Control flags
0x18    user_data User-defined
0x20    padding   Anti-fingerprint (descriptor size 32→36)
```

Flags: 0x01=READ, 0x02=WRITE, 0x04=CHAIN, 0x08=LAST, 0x10=NULL, 0x20=INT_ON_COMP, 0x40=SCRAMBLE

## CS2 Entity Read Flow (T4 DMA)

```
1. Scan physical memory for EPROCESS structure
   - Search for "cs2.exe" string at +0x2B8 (Win10) or +0x3E0 (Win11)
   - Extract DirectoryTableBase (CR3) from KPROCESS at +0x28

2. Walk page tables to find client.dll
   - PML4[va>>39] -> PDP[va>>30] -> PD[va>>21] -> PT[va>>12]
   - Handle 2MB and 1GB large pages (PS bit)
   - Client.dll VA is typically around 0x180000000

3. Read entity list pointer from client.dll
   - At RVA 0x254EE60 (dwEntityList): read the pointer
   - Translate entity list VA -> PA via page walk

4. Walk 64 entity slots at stride 0x70
   - For each slot: read 8-byte controller address
   - Read m_hPlayerPawn (+0x914) from controller
   - Resolve pawn: (handle & 0x7FFF) * 0x70 + entity_list_base
   - Read m_iHealth (+0x34C), m_iTeamNum (+0x3E7), m_vOldOrigin (+0x13B8)
   - Read m_angEyeAngles (+0x3340) for yaw
   - Read m_vecVelocity (+0x1170)
```

## CS2 Offsets (Periscope-Validated, XOR-encrypted in fpga_dma.h)

```cpp
dwEntityList            = 0x254EE60    m_iHealth     = 0x34C
dwLocalPlayerPawn       = 0x23A4238    m_iTeamNum    = 0x3E7 (uint8)
dwLocalPlayerController = 0x237EBA0    m_vOldOrigin  = 0x13B8
dwViewAngles            = 0x23B9C78    m_angEyeAngles= 0x3340
dwCSGOInput             = 0x23B95F0    m_hPlayerPawn = 0x914
dwPlantedC4             = 0x236E678    kIdentityStride = 0x70
                                       m_vecVelocity = 0x1170
```

## Build

### With CMake (recommended)
```bash
cmake -S . -B build
cmake --build build
./build/bin/host_sim
./build/bin/dma_tests
```

### With Make (Clang++/G++)
```bash
make
./host_sim
./dma_tests
```

### With MSVC (Windows)
```cmd
cl /EHsc /std:c++20 /I. host_sim.cpp page_table_walker.cpp cs2_dma_reader.cpp /Fe:host_sim.exe
cl /EHsc /std:c++20 /I. tests\dma_tests.cpp page_table_walker.cpp cs2_dma_reader.cpp /Fe:dma_tests.exe
```

## CLI

```
host_sim [--iommu] [--iterations N] [--quiet] [--check] [--help]
```

- Default: full pipeline → prints entity table (5 synthetic players). Exit 0 on match.
- `--check`: self-check of walker / EPROCESS / entity fields; exit 0 on pass.
- `--iommu`: simulate IOMMU blocking DMA (error path).

## Sample Output
```
[  0.000] [HOST]  T4 PCIe DMA: CS2 Entity Read via FPGA
[  0.001] [MEM]   client.dll PA = 0x02A01000
[  0.001] [MEM]   CS2 CR3 = 0x01000000
[  2.003] [PCIe]  Link up: width x4, speed 5.0 GT/s
[  2.003] [FPGA]  Running DMA...
[  2.010] [T4]    Found 5 valid entities

  Slot  Health Team  Origin.X     Origin.Y     Origin.Z     Yaw
  ----  ------ ----- ------------ ------------ ------------ ------
  0     100    3     123.4        56.7         1.2          45.0
  1     85     3     456.7        123.4        0.5          120.0
  2     67     3     789.1        234.5        2.1          200.0
  3     42     2     111.2        333.4        1.8          300.0
  4     91     2     555.6        777.8        0.9          15.0
```

## Blue Team Detection & Mitigation

| Vector | What Blue Sees |
|--------|---------------|
| **IOMMU/VT-d** | DMA remapping blocks reads to unmapped pages |
| **ACS** | Access Control Services in PCIe switch |
| **PCIe Vendor ID** | Spoofed/unknown device on bus (0x1B73+0xA0B0) |
| **Bandwidth Anomaly** | Unexpected PCIe traffic pattern |
| **Physical Access** | FPGA requires PCIe slot access |
| **DMA Firmware** | Custom bitstream detectable via JTAG/SPI flash |

## Files

| File | Description |
|------|-------------|
| `fpga_dma.h` | Hardened BAR map, DmaDescriptor (36B), Cs2Offsets, PageTableWalker, Cs2DmaReader |
| `host_sim.cpp` | Complete T4 simulation: memory image, FPGA device, entity read pipeline |
| `page_table_walker.cpp` | x86-64 4-level page table walk (4KB/2MB/1GB) + EPROCESS scanner |
| `cs2_dma_reader.cpp` | CS2 entity list walk via physical reads |
| `obf.hpp` | Standalone compile-time string obfuscation |
| `tests/dma_tests.cpp` | Unit tests for walker / entity reader / register map |
| `CMakeLists.txt` | CMake build system |
| `Makefile` | Clang/GCC build |

## References

- Periscope entity adapter: [`entity.hpp`](../../../adapters/real/cs2/entity.hpp)
- PCILeech FPGA: https://github.com/ufrisk/pcileech-fpga
- PCILeech: https://github.com/ufrisk/pcileech
- Intel SDM Vol 3A Chapter 4: Paging Structures
- Periscope T4 red-team components: `radar/src/lab_components/teams/t4_red/`
