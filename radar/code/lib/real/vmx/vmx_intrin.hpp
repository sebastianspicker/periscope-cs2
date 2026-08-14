// vmx_intrin.hpp — Full Intel VT-x / Hyper-V research stack for x64.
//
// Unprivileged surfaces (CPUID, EPT entry math, hierarchy software bookkeeping,
// Hyper-V leaf spoofing) run correctly in ordinary usermode. Privileged
// sequences (VMXON/OFF, VMCS, MSR, INVEPT/INVVPID, VMCALL) are real instruction
// paths gated by CPL=0 / SEH pre-checks so CI hosts get structured errors
// instead of unhandled #UD.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#if LR_ARCH_X64

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace real::vmx {

// ── VMX Basic Types ────────────────────────────────────────────────

/// VMCS field encodings (Intel SDM Vol 3C, Appendix B).
enum class VmcsField : std::uint64_t {
  // 16-bit control fields
  Vpid                     = 0x00000000,
  PostedIntrNotifyVector   = 0x00000002,
  EptpIndex                = 0x00000004,

  // 16-bit guest-state fields
  GuestEsSelector          = 0x00000800,
  GuestCsSelector          = 0x00000802,
  GuestSsSelector          = 0x00000804,
  GuestDsSelector          = 0x00000806,
  GuestFsSelector          = 0x00000808,
  GuestGsSelector          = 0x0000080A,
  GuestLdtrSelector        = 0x0000080C,
  GuestTrSelector          = 0x0000080E,

  // 16-bit host-state fields
  HostEsSelector           = 0x00000C00,
  HostCsSelector           = 0x00000C02,
  HostSsSelector           = 0x00000C04,
  HostDsSelector           = 0x00000C06,
  HostFsSelector           = 0x00000C08,
  HostGsSelector           = 0x00000C0A,
  HostTrSelector           = 0x00000C0C,

  // 64-bit control fields
  IoBitmapA                = 0x00002000,
  IoBitmapB                = 0x00002002,
  MsrBitmap                = 0x00002004,
  VmExitMsrStoreAddr       = 0x00002006,
  VmExitMsrLoadAddr        = 0x00002008,
  VmEntryMsrLoadAddr       = 0x0000200A,
  ExecutiveVmcsPointer     = 0x0000200C,
  TscOffset                = 0x00002010,
  VirtualApicPageAddr      = 0x00002012,
  ApicAccessAddr           = 0x00002014,
  Eptp                     = 0x0000201A,
  EptpList                 = 0x0000201C,

  // 64-bit guest-state fields
  VmcsLinkPointer          = 0x00002800,
  GuestIa32Debugctl        = 0x00002802,
  GuestIa32Efer            = 0x00002806,

  // 64-bit host-state fields
  HostIa32Efer             = 0x00002C02,

  // 32-bit control fields
  PinBasedExecControls     = 0x00004000,
  CpuBasedExecControls     = 0x00004002,
  ExceptionBitmap          = 0x00004004,
  PageFaultErrorCodeMask   = 0x00004006,
  PageFaultErrorCodeMatch  = 0x00004008,
  Cr3TargetCount           = 0x0000400A,
  VmExitControls           = 0x0000400C,
  VmExitMsrStoreCount      = 0x0000400E,
  VmExitMsrLoadCount       = 0x00004010,
  VmEntryControls          = 0x00004012,
  VmEntryMsrLoadCount      = 0x00004014,
  VmEntryIntrInfo          = 0x00004016,
  VmEntryExceptionErrorCode = 0x00004018,
  VmEntryInstructionLen    = 0x0000401A,
  TprThreshold             = 0x0000401C,
  SecondaryExecControls    = 0x0000401E,

  // 32-bit guest-state fields
  GuestEsLimit             = 0x00004800,
  GuestCsLimit             = 0x00004802,
  GuestSsLimit             = 0x00004804,
  GuestDsLimit             = 0x00004806,
  GuestFsLimit             = 0x00004808,
  GuestGsLimit             = 0x0000480A,
  GuestLdtrLimit           = 0x0000480C,
  GuestTrLimit             = 0x0000480E,
  GuestGdtrLimit           = 0x00004810,
  GuestIdtrLimit           = 0x00004812,
  GuestEsAccessRights      = 0x00004814,
  GuestCsAccessRights      = 0x00004816,
  GuestSsAccessRights      = 0x00004818,
  GuestDsAccessRights      = 0x0000481A,
  GuestFsAccessRights      = 0x0000481C,
  GuestGsAccessRights      = 0x0000481E,
  GuestLdtrAccessRights    = 0x00004820,
  GuestTrAccessRights      = 0x00004822,
  GuestInterruptibility    = 0x00004824,
  GuestActivityState       = 0x00004826,
  GuestSysenterCs          = 0x0000482A,
  VmxPreemptionTimerValue  = 0x0000482E,

  // 32-bit host-state fields
  HostIa32SysenterCs       = 0x00004C00,

  // Natural-width control fields
  Cr0GuestHostMask         = 0x00006000,
  Cr4GuestHostMask         = 0x00006002,
  Cr0ReadShadow            = 0x00006004,
  Cr4ReadShadow            = 0x00006006,

  // Natural-width guest-state fields
  GuestCr0                 = 0x00006800,
  GuestCr3                 = 0x00006802,
  GuestCr4                 = 0x00006804,
  GuestEsBase              = 0x00006806,
  GuestCsBase              = 0x00006808,
  GuestSsBase              = 0x0000680A,
  GuestDsBase              = 0x0000680C,
  GuestFsBase              = 0x0000680E,
  GuestGsBase              = 0x00006810,
  GuestLdtrBase            = 0x00006812,
  GuestTrBase              = 0x00006814,
  GuestGdtrBase            = 0x00006816,
  GuestIdtrBase            = 0x00006818,
  GuestDr7                 = 0x0000681A,
  GuestRsp                 = 0x0000681C,
  GuestRip                 = 0x0000681E,
  GuestRflags              = 0x00006820,
  GuestPendingDbgExcept    = 0x00006822,
  GuestSysenterEsp         = 0x00006824,
  GuestSysenterEip         = 0x00006826,

  // Natural-width host-state fields
  HostCr0                  = 0x00006C00,
  HostCr3                  = 0x00006C02,
  HostCr4                  = 0x00006C04,
  HostFsBase               = 0x00006C06,
  HostGsBase               = 0x00006C08,
  HostTrBase               = 0x00006C0A,
  HostGdtrBase             = 0x00006C0C,
  HostIdtrBase             = 0x00006C0E,
  HostIa32SysenterEsp      = 0x00006C10,
  HostIa32SysenterEip      = 0x00006C12,
  HostRsp                  = 0x00006C14,
  HostRip                  = 0x00006C16,
};

/// VMX instruction completion status.
enum class VmxStatus : std::uint64_t {
  Success = 0,
  FailWithStatus = 1,
  FailInvalid = 2,
};

/// One VMCS field write produced by pure field-programming logic.
struct VmcsFieldWrite {
  VmcsField field = VmcsField::GuestRip;
  std::uint64_t value = 0;
};

/// Inputs for composing a basic VMLAUNCH-ready field table (pure, no hardware).
struct VmcsSetupConfig {
  std::uint64_t guest_rip = 0;
  std::uint64_t guest_rsp = 0;
  std::uint64_t guest_cr3 = 0;
  std::uint64_t guest_cr0 = 0x80000031ULL;   // PE|MP|ET|NE|PG typical long-mode shadow
  std::uint64_t guest_cr4 = 0x00000020ULL;   // PAE
  std::uint64_t guest_rflags = 0x2;
  std::uint64_t guest_efer = 0x500;          // LME|LMA

  std::uint64_t host_rip = 0;
  std::uint64_t host_rsp = 0;
  std::uint64_t host_cr0 = 0;
  std::uint64_t host_cr3 = 0;
  std::uint64_t host_cr4 = 0;
  std::uint64_t host_efer = 0x500;

  std::uint64_t eptp = 0;
  std::uint16_t vpid = 1;

  // Pre-adjusted control fields (caller may use capability MSRs to adjust).
  std::uint32_t pin_based = 0x00000016;       // ext-int + NMI exiting defaults
  std::uint32_t cpu_based = 0x94006172;       // common primary + secondary activate
  std::uint32_t secondary = 0x0000002A;       // EPT + VPID + unrestricted guest bits
  std::uint32_t exit_controls = 0x00036FFB;   // host-addr-space-size etc.
  std::uint32_t entry_controls = 0x000013FF;  // IA-32e mode guest etc.
};

// ── CPUID-based VMX detection ──────────────────────────────────────

/// Check if VMX is supported by the CPU (CPUID.1:ECX.VMX).
Result<bool> vmx_supported();

/// Check if SVM is supported by the CPU (CPUID.80000001h:ECX.SVM).
Result<bool> svm_supported();

/// Check if the platform hypervisor is active (CPUID.1:ECX.hypervisor).
Result<bool> is_hypervisor_guest();

/// A non-invasive snapshot of the CPU and hypervisor CPUID interfaces.
struct CpuidDump {
  std::string vendor;
  std::uint32_t max_leaf = 0;
  bool vmx_support = false;
  bool hypervisor_present = false;
  std::string hv_vendor;

  /// Format the snapshot for diagnostics.
  std::string describe() const;
};

/// Collect CPUID data without executing privileged instructions.
CpuidDump dump_cpuid();

/// Calibrated CPUID execution latency (2x median of 100 samples).
/// Used for RDTSC timing spoof to hide VM exits.
std::uint64_t get_cpuid_latency();

/// Read an MSR when called by a trusted kernel-mode integration.
/// User-mode callers receive a structured error (no #GP).
Result<std::uint64_t> read_msr_usermode(std::uint32_t msr);

/// Attempt VMX enablement from the current privilege level.
/// On CPL=0 runs the real CR4.VMXE / FEATURE_CONTROL sequence; otherwise
/// returns a structured privilege error without touching privileged state.
Result<void> enable_vmx_from_usermode();

/// Translate a virtual address to its physical address.
/// Linux: /proc/self/pagemap. Windows: requires kernel mediation (structured error)
/// unless a research physical-address mapping was registered for the page.
Result<std::uint64_t> virt_to_phys(void* virt_addr);

// ── VMX Lifecycle ──────────────────────────────────────────────────

/// Enable VMX: unlock IA32_FEATURE_CONTROL if needed, set CR4.VMXE.
/// Real sequence at CPL=0; structured error otherwise.
Result<void> enable_vmx();

/// Allocate and initialize a 4 KiB-aligned VMXON region with revision ID.
Result<void*> vmxon_alloc();

/// Free a region allocated by vmxon_alloc / vmcs_alloc.
void vmx_region_free(void* region);

/// Execute VMXON with the physical address of the region.
Result<void> vmxon(void* region);

/// Execute VMXOFF and clear CR4.VMXE.
Result<void> vmxoff();

// ── VMCS Operations ────────────────────────────────────────────────

/// Allocate a 4 KiB-aligned VMCS region with revision ID.
Result<void*> vmcs_alloc();

/// Clear a VMCS (VMCLEAR) using its physical address.
Result<void> vmclear(void* vmcs);

/// Load a VMCS pointer (VMPTRLD) using its physical address.
Result<void> vmptrld(void* vmcs);

/// Read a VMCS field (VMREAD) from the current VMCS.
Result<std::uint64_t> vmread(VmcsField field);

/// Write a VMCS field (VMWRITE) in the current VMCS.
Result<void> vmwrite(VmcsField field, std::uint64_t value);

/// Pure field-programming: build the ordered list of VMCS writes for a basic
/// guest. Does not touch hardware; unit-testable without CPL=0.
std::vector<VmcsFieldWrite> compose_basic_vmcs_fields(const VmcsSetupConfig& cfg);

// ── VM Entry ───────────────────────────────────────────────────────

/// Launch a virtual machine (VMLAUNCH).
Result<VmxStatus> vmlaunch();

/// Resume a virtual machine (VMRESUME).
Result<VmxStatus> vmresume();

// ── EPT ────────────────────────────────────────────────────────────

/// EPT pointer structure (EPTP) plus software bookkeeping for teardown.
struct EptPointer {
  std::uint64_t value = 0;
  /// Virtual address of the PML4 table (software only; not in the EPTP MSR).
  void* pml4_virtual = nullptr;

  /// EPT paging-structure memory type (bits 2:0), e.g. 6 = write-back.
  std::uint8_t memory_type() const;
  /// Page-walk length in levels (bits 5:3 encode length-1).
  int page_walk_length() const;
  /// Accessed/dirty flags enable (bit 6).
  bool accessed_dirty() const;
  /// Physical address of the EPML4 table (bits 51:12).
  std::uint64_t pml4() const;
};

/// Build an EPT page table hierarchy mapping a guest-physical range to host pages.
/// Table entries store host physical addresses (real via virt_to_phys, or
/// research synthetic PAs with a maintained virt↔phys registry when PA is
/// unavailable in usermode). Supports 4 KiB leaves and 2 MiB large pages for
/// aligned ranges.
Result<EptPointer> build_ept_hierarchy(std::uint64_t guest_phys_base,
                                       std::size_t size,
                                       std::uint64_t host_phys_base);

/// Optional flags for hierarchy construction.
struct EptBuildOptions {
  bool use_large_pages = true;   // 2 MiB leaves when GPA/HPA/size allow
  bool write_back = true;        // memory type 6 vs 0
};

/// Build EPT hierarchy with explicit options (same registry semantics).
Result<EptPointer> build_ept_hierarchy_ex(std::uint64_t guest_phys_base,
                                          std::size_t size,
                                          std::uint64_t host_phys_base,
                                          const EptBuildOptions& opts);

/// Set the host physical page number of an EPT entry (bits 51:12).
void ept_entry_set_phys_addr(std::uint64_t& entry, std::uint64_t phys_addr);

/// Read the host physical address field from an EPT entry.
std::uint64_t ept_entry_get_phys_addr(std::uint64_t entry);

/// Set read, write, and execute permissions on an EPT entry (bits 2:0).
void ept_entry_set_access(std::uint64_t& entry, bool read, bool write, bool execute);

/// Read permission bits (bit0=R, bit1=W, bit2=X).
void ept_entry_get_access(std::uint64_t entry, bool& read, bool& write, bool& execute);

/// Set the EPT memory type (bits 5:3), e.g. 6 for write-back. Valid on leaf entries.
void ept_entry_set_memory_type(std::uint64_t& entry, std::uint8_t memory_type);

/// Read the EPT memory type field (bits 5:3).
std::uint8_t ept_entry_get_memory_type(std::uint64_t entry);

/// True if the entry is a large-page leaf (bit 7 set on PDE/PDPTE).
bool ept_entry_is_large(std::uint64_t entry);

/// Software walk of a built hierarchy: translate GPA → HPA using the virt registry.
Result<std::uint64_t> ept_translate_gpa(const EptPointer& eptp, std::uint64_t gpa);

/// Tear down an EPT hierarchy; frees only pages allocated by build_ept_hierarchy.
Result<void> destroy_ept_hierarchy(const EptPointer& eptp, const void* pml4_virtual);

/// Invalidate EPT cached mappings (INVEPT). type=1 single-context, type=2 all-context.
Result<void> invept(int type, std::uint64_t eptp);

/// Invalidate VPID mappings (INVVPID).
Result<void> invvpid(int type, std::uint16_t vpid, std::uint64_t linear_addr);

// ── Physical Memory via VMX ────────────────────────────────────────

/// Read physical memory (Linux /dev/mem or research mapping). Structured error
/// when the platform cannot expose physical memory to this process.
Result<std::vector<std::uint8_t>> vmx_read_physical(std::uint64_t phys_addr,
                                                     std::size_t size);

// ── Hyper-V Hypercalls ─────────────────────────────────────────────

/// Issue a Hyper-V hypercall (VMCALL). Pre-checks hypercall availability and
/// catches #UD when the host is not a Hyper-V guest / not authorized.
Result<std::uint64_t> hyperv_hypercall(std::uint64_t input,
                                        std::uint64_t param1,
                                        std::uint64_t param2);

/// Check if Hyper-V hypercalls are available (CPUID leaves).
Result<bool> hyperv_hypercalls_available();

/// Query the platform hypervisor vendor string (CPUID leaf 0x40000000).
/// Returns empty string if no hypervisor is present.
std::string platform_hv_vendor();

/// Return a spoofed Hyper-V vendor string for guest CPUID leaf 0x40000000.
/// Relays the platform HV vendor if present, otherwise returns "Microsoft Hv".
std::string hyperv_spoofed_vendor();

/// Fill a/b/c/d with realistic Hyper-V feature-flags for leaf 0x40000001.
void hyperv_spoofed_leaf_40000001(std::uint32_t& a, std::uint32_t& b,
                                   std::uint32_t& c, std::uint32_t& d);

/// Fill a/b/c/d with spoofed values for Hyper-V leaves 0x40000002–0x4000000A.
void hyperv_spoofed_leaf(std::uint32_t leaf, std::uint32_t& a, std::uint32_t& b,
                          std::uint32_t& c, std::uint32_t& d);

/// Read guest physical memory through Hyper-V hypercall path when available.
Result<std::vector<std::uint8_t>> hyperv_read_virtual_memory(std::uint64_t guest_phys_addr,
                                                              std::size_t size);

/// Snapshot VMX capabilities. MSR-derived fields require CPL=0; CPUID-only
/// fallback populates vmx_enabled without crashing usermode hosts.
struct VmxCapabilities {
  bool vmx_enabled = false;
  bool ept_supported = false;
  bool vpid_supported = false;
  bool unrestricted_guest = false;
  bool invept_supported = false;
  bool invvpid_supported = false;
  int max_ept_levels = 0;
  bool msr_probe_ok = false;   // true if capability MSRs were readable

  /// Format the snapshot for diagnostics.
  std::string describe() const;
};

/// Collect VMX capabilities (CPUID always; MSRs when CPL=0).
Result<VmxCapabilities> get_vmx_capabilities();

/// Full VMCS setup for research: alloc, VMCLEAR, VMPTRLD, then program host
/// state, guest state, pin/CPU/secondary/exit/entry controls, and EPTP via
/// compose_basic_vmcs_fields + VMWRITE. On usermode, returns structured error
/// after preparing what pure software can (alloc + field table is still pure).
Result<void*> setup_vmcs(void* vmxon_region, std::uint64_t guest_rip,
                         std::uint64_t guest_rsp, std::uint64_t guest_cr3);

/// Extended setup accepting a full config (including EPTP and host state).
Result<void*> setup_vmcs_ex(void* vmxon_region, const VmcsSetupConfig& cfg);

}  // namespace real::vmx

#endif  // LR_ARCH_X64
