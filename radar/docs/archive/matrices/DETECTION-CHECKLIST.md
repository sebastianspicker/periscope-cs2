# Detection / red-team checklist

Use inside a **lab** with fixtures or authorized tests. Do not point this at production players without policy approval.

## T0 — Classic RPM

- [ ] Reader process opens game with VM_READ
- [ ] Handle appears in graph within N seconds
- [ ] Co-occurrence event emitted (game + reader)
- [ ] Secondary radar window optional detection (may miss)
- [ ] Allowlisted apps do not hard-ban (FP path)

## T1 — Syscall RPM

- [ ] Same as T0 with indirect syscalls (no hooked API)
- [ ] Detection does **not** depend on ntdll detour alone
- [ ] Packed / string-encrypted reader still caught via handle

## T2 — Custom driver

- [ ] Unknown driver load event
- [ ] Device create / symbolic link logged
- [ ] Usermode open of device correlated with game session
- [ ] Block or force-fail ranked when policy says so

## T2 — BYOVD

- [ ] Known-bad driver hash/cert on blocklist fails load or flags session
- [ ] Blocklist update path works without full client ship
- [ ] Newly observed vulnerable driver can be staged to blocklist

## T3 — Policy / HV

- [ ] Ranked refuses or scores VBS off
- [ ] Ranked refuses or scores HVCI off (as product allows)
- [ ] Unexpected hypervisor presence flagged
- [ ] Timing/CPUID probe suite records anomalies
- [ ] Bridge-class driver treated as T2
- [ ] When HV “succeeds,” T0 handle may be empty — policy/HV/bridge still fire
- [ ] Fallback: if red disables HV and uses RPM, T0 still catches

## Server / design

- [ ] Unobservable enemies not fully replicated (interest mgmt test)
- [ ] Demo contains vision + sound + eye angles for scoring
- [ ] Synthetic “perfect info” trajectory raises info-advantage score
- [ ] Delayed ban path dry-run with correlated signals

## Coverage matrix (fill in dates)

| Tier | Detector path | Lab pass | Prod telemetry live |
|------|---------------|----------|---------------------|
| T0 | handle_graph | ☐ | ☐ |
| T1 | handle_graph | ☐ | ☐ |
| T2 custom | driver_guard | ☐ | ☐ |
| T2 BYOVD | blocklist | ☐ | ☐ |
| T3 policy | attestation_gate | ☐ | ☐ |
| T3 HV | hv_probe | ☐ | ☐ |
| All | info_advantage | ☐ | ☐ |
| All | interest_mgmt | ☐ | ☐ |
