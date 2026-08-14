# Tiered counter strategies

Defensive summary: **what red does** → **what blue counters with**, per tier.  
DMA is residual only (T4). Software-first userbase assumed.

---

## Quick matrix

| Tier | Red core strategy | Blue primary counters | Blue structural / residual |
|------|-------------------|----------------------|----------------------------|
| **T0** | Usermode RPM + external UI | Handle graph, process co-occurrence | Fog-of-war, info-advantage |
| **T1** | Syscall RPM / packed soft client | Same handles; don’t trust API hooks | Same + light UI/network intel |
| **T2** | Kernel or BYOVD read (no handle) | Driver load, blocklist, device/IOCTL | Callback integrity; fallbacks T0–T1 |
| **T3** | Personal HV + thin bridge | Trust policy (VBS/HVCI), HV probe, bridge as driver | Attestation; server authority |
| **T4*** | DMA / 2nd PC hardware | IOMMU / device policy (limited) | Fog-of-war, info-advantage |

\*Out of primary scope; listed for completeness.

---

## T0 — Free / public usermode external

### What they do
- Separate process: `OpenProcess` + `ReadProcessMemory`
- Walk entity list with public/static offsets
- Draw radar in SFML/ImGui/Win32 (or simple overlay-adjacent window)
- No inject, no game write

### Strategies to counter

| Priority | Strategy | How |
|----------|----------|-----|
| **1** | **Handle graph** | Flag foreign processes with `PROCESS_VM_READ` (or equivalent) on the game PID |
| **2** | **Process co-occurrence** | Correlate game session with reader process names/paths/parents |
| **3** | **Window heuristics** | Secondary always-on-top / map-like windows (weak; easy to hide) |
| **4** | **Reputation / allowlist** | Reduce FPs from debuggers, rec tools, RGB utilities |
| **5** | **Server info-advantage** | Pre-aim / track without vision or sound on demos |
| **6** | **Interest management** | Don’t replicate unobservable enemy positions to client |

### Do not rely on
- Game `.text` integrity alone  
- Aimbot ML alone  
- YARA of public free radars alone  

### Success criteria
Naive RPM radar detected **within one match** with manageable FPs.

---

## T1 — Paid soft external (median adversary)

### What they do
- Same as T0, plus:
  - Indirect / direct **syscalls** (skip hooked ntdll)
  - Encrypted offsets + C2 update after patches
  - Packer / string encrypt / per-buyer builds
  - Phone or browser radar (PC only reads + streams)
  - Loader stub → staged payload

### Strategies to counter

| Priority | Strategy | How |
|----------|----------|-----|
| **1** | **Handle graph still primary** | Syscalls do **not** remove the open handle — enum handles, not API detours |
| **2** | **Stop trusting usermode hooks** | Detours on `ReadProcessMemory` / ntdll are bypassable; kernel/object callbacks preferred |
| **3** | **Session correlation** | Packed unique binaries → track behavior + handles + lineage, not file hash |
| **4** | **Staging / reflective load** | Watch short-lived stubs, RWX private regions, parent/child graphs |
| **5** | **Network (supportive)** | Local WebSocket listeners; known radar SaaS domains (policy permitting) |
| **6** | **Threat intel (supportive)** | Seller C2 / offset CDNs for ops, not sole ban evidence |
| **7** | **Info-advantage + fog-of-war** | Same as T0 — still works when client looks “clean” |

### Do not rely on
- Hooking only `kernel32!ReadProcessMemory`  
- Static signatures of paid builds  
- Overlay/swapchain detection (legit radar often avoids game overlay)  

### Success criteria
Syscall + packed T1 caught by **same handle pipeline** as T0; phone-UI variants still leave a **PC reader** signal.

---

## T2 — Kernel driver / BYOVD (“no handle” software)

### What they do
- UI process requests reads via **IOCTL** to:
  - Custom mem R/W driver, or  
  - **BYOVD** (signed vulnerable driver)
- Often **no** usermode handle to the game
- Optional callback stripping / device name obfuscation
- Fallback to T1 RPM if driver fails

### Strategies to counter

| Priority | Strategy | How |
|----------|----------|-----|
| **1** | **Image / driver load telemetry** | Every kernel module load: hash, cert, path, signer, boot-start vs demand |
| **2** | **BYOVD cloud blocklist** | Fast-updated deny of known-vulnerable drivers; block or hard-flag session |
| **3** | **Device / IOCTL watch** | Suspicious devices; usermode process opening them during matches |
| **4** | **Driver allowlist (strict modes)** | Competitive: only known-good third-party drivers where product allows |
| **5** | **Callback / notify integrity** | Detect tampering with process/image/object notify chains |
| **6** | **Keep T0–T1 detectors live** | Degradation path when HV/driver fails or isn’t used |
| **7** | **Service / SCM events** | New kernel services adjacent to first cheat use |
| **8** | **Server residual** | Info-advantage + interest management (client may look handle-clean) |

### Do not rely on
- Handle graph alone (intentionally empty)  
- “Unsigned driver only” (BYOVD is signed)  
- One-time boot scan (drivers can load mid-session)  

### Success criteria
Custom R/W driver or known BYOVD load → **session risk or block** before or during match; blocklist ships without full client release.

---

## T3 — Hypervisor-assisted (rare software)

### What they do
- Personal hypervisor (VMX/SVM) under/beside Windows  
- Guest VA → GPA walks for entity data  
- **Thin bridge** (small `.sys` / VMCALL / shared page) to radar UI  
- Instruct or force **VBS / Hyper-V / Core Isolation off**  
- Stealth: CPUID hide, timing tricks, EPT page hide  
- Fallbacks: T2 then T1  

### Strategies to counter

| Priority | Strategy | How |
|----------|----------|-----|
| **1** | **Trust policy gate** | Ranked requires **VBS + HVCI** (+ Secure Boot if product allows); refuse or heavy-score if off |
| **2** | **Attestation** | TPM / Secure Launch / measured boot where available — known-good state only |
| **3** | **Hypervisor presence probes** | CPUID leaves, vendor strings, multi-invariant **timing** (CPUID/RDTSC/exception latency) |
| **4** | **Bridge = T2 driver problem** | Hunt `hvcomm`-class modules, devices, IOCTL/VMCALL channels like any cheat driver |
| **5** | **Boot / early load control** | Boot-start allowlist, ELAM, unexpected EFI boot entries |
| **6** | **Nested virt policy** | Prefer only trusted virt stack (e.g. platform HV); treat foreign HV as hostile |
| **7** | **Full lower-tier stack** | Empty handles under pure HV → still catch fallbacks and bridge |
| **8** | **Server authority** | Interest management + info-advantage + delayed bans when client is fully hostile |

### Do not rely on
- Handle graph alone  
- Single CPUID “hypervisor bit” check (spoofable)  
- Pure client integrity if red owns VMX root without attestation  

### Success criteria
Competitive play **hard without** disabling platform trust; residual T3 on unlocked machines still limited by **no useful client entity data** and **demo scoring**.

### Honest limit
True hostile HV on a fully unlocked client can lie to guest AC. Blue’s durable remaining tools are **policy/attestation**, **server fog-of-war**, and **behavioral/forensic bans**.

---

## T4 — Hardware DMA / multi-PC (residual)

### What they do
- PCIe DMA / FPGA / second machine reads RAM; game PC may have no cheat process  

### Strategies to counter (brief)

| Priority | Strategy | How |
|----------|----------|-----|
| **1** | **Interest management** | Best ROI — data never on client in useful form |
| **2** | **Info-advantage demos** | Same behavioral layer as software radar |
| **3** | **IOMMU / device policy** | Where OEM and product support allow |
| **4** | **Input / capture anomalies** | Hardware mouse / capture-card class (different product surface) |

Not the focus of this research tree; do not over-invest until T0–T3 software controls exist.

---

## Controls that apply to **every** tier

Build these once; they stack under all red backends.

| Control | Role |
|---------|------|
| **Interest management / fog-of-war** | Shrinks or kills radar value regardless of read path |
| **Server demos + info-advantage scoring** | Catches “legit” human use of stolen info |
| **Delayed ban correlator** | Combines weak signals without instant FP bans |
| **Account / HWID / social graph** | Seller clusters, repeat offenders |
| **Degradation awareness** | Always run T0–T2 detectors even when hunting T3 |

---

## Recommended build order (engineering)

```text
1. T0/T1  handle graph + object callbacks + FP allowlist
2. T2     driver load telemetry + BYOVD blocklist + device watch
3. All    basic info-advantage features on demos
4. T3     ranked VBS/HVCI policy + HV probes + bridge intel
5. All    interest management (largest design cost, largest payoff)
6. T3+    attestation / Secure Launch as platform matures
7. T4     IOMMU only if threat model demands
```

---

## One-line cheatsheet

| Tier | Counter in one line |
|------|---------------------|
| **T0** | Who has a read handle on the game? |
| **T1** | Same — ignore their syscalls and packing. |
| **T2** | What driver/device is reading for them? Block BYOVD. |
| **T3** | Are they in a trusted virt state? Who owns HV + bridge? |
| **All** | Don’t send free enemy XY; score impossible knowledge on demos. |
