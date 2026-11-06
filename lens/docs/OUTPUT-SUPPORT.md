# Lens output-path support matrix (ledger L8)

How the computed assist reaches the PC. Read with
[`TOPOLOGY.md`](TOPOLOGY.md): actuation is lab-only opt-in;
nothing here is default-on, and hardware injectors (kmbox-class) are
documented as compatible hardware, never implemented.

## The decision ladder (implemented in [`../src/periscope_lens/output.py`](../src/periscope_lens/output.py))

```
hid_gadget available?  → OutputPath.HID_GADGET   (phone is the mouse; zero PC software)
else pc_agent reachable? → OutputPath.PC_AGENT   (opt-in lab receiver sends OS input events)
else                   → OutputPath.TARGETS_ONLY (phone publishes targets; PC owns motion)
```

Detection is per-device and *evidence-based*: a path is claimed available
only after its feature probe succeeded on this exact phone/PC pair. A
silent fallback to a worse path is forbidden - the chosen path is always
surfaced to the user (the L8 acceptance).

## Path 1 - phone-as-USB-HID gadget

- Mechanism: Android 10+ configfs USB gadget functions (`usb_gadget_tool`
  lineage; Arian04/android-hid-client is the reference implementation).
  The PC sees a standard relative mouse; no drivers, no agent.
- Kernel/configfs support varies per device - hence the matrix below and
  the mandatory feature probe (does `/config/usb_gadget` exist and accept a
  HID function?).
- Constraints encoded in `HidDeltaShaper`: relative deltas only, clamped to
  the signed-byte report range (−127..127), rate-shaped per report so a
  large correction lands as several smooth steps, never one jump.

### Device matrix (fill by testing; never assume)

| device | Android | HID gadget probe | transport used | notes |
|---|---|---|---|---|
| *(fill per tested phone)* | | | adb / rndis / serial | |

## Path 2 - PC receiver agent

- A small opt-in process translating L6 `Targets` frames to OS input
  events, mirroring the house posture for actuation: gated behind explicit
  lab flags, excluded from the ordinary review lane like the other
  real-backend surfaces.
- Never started by the pipeline by default; the run summary states whether
  an agent is attached.

## Path 3 - targets only

- Always available: the phone publishes L6 `Targets`; all motion policy
  stays on the PC side (which, in this project's default posture, means
  "none is applied").

## Licensing note (X5 lens half lands in the cross commit)

The configfs mechanism is standard Android; the reference client
(Arian04/android-hid-client) is GPL - verify before code reuse, or
reimplement the (small, standard) gadget setup directly.
