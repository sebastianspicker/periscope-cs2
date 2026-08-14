# Blue project tree (anti-cheat / integrity)

Suggested structure for a research-to-product codebase. Names are conventional; adapt to engine and platform.

```text
blue-anticheat/
│
├── README.md
├── docs/
│   ├── threat-model.md              # link/copy from research folder
│   ├── tier-coverage.md             # T0–T3 matrix ownership
│   └── privacy-legal.md             # what telemetry is allowed
│
├── policy/                          # competitive trust requirements
│   ├── ranked_requirements.json     # VBS, HVCI, Secure Boot, IOMMU later
│   ├── driver_allowlist.json
│   ├── driver_blocklist.json        # BYOVD + known cheat bridges
│   └── risk_scores.yaml             # VBS-off = elevated, etc.
│
├── client/                          # runs on player machine
│   ├── usermode/
│   │   ├── ac_agent/                # service / anti-cheat user process
│   │   │   ├── main.cpp
│   │   │   ├── process_watch.cpp    # co-occurrence with game
│   │   │   ├── window_watch.cpp     # secondary radar windows (weak)
│   │   │   ├── network_watch.cpp    # optional: local WS / known SaaS
│   │   │   ├── handle_graph/        # ★ T0–T1 primary
│   │   │   │   ├── enum_handles.cpp
│   │   │   │   ├── classify_access.cpp  # VM_READ on game PID
│   │   │   │   └── report.cpp
│   │   │   ├── hv_probe/            # ★ T3 signals from usermode where possible
│   │   │   │   ├── cpuid_probe.cpp
│   │   │   │   ├── timing_probe.cpp
│   │   │   │   └── nested_virt.cpp
│   │   │   └── telemetry_pipe.cpp   # to kernel + cloud
│   │   └── game_sdk_hooks/          # optional engine integration
│   │       └── match_session.cpp
│   │
│   └── kernel/                      # ★ T2 + hard T3 bridge
│       ├── ac_driver/
│       │   ├── entry.c
│       │   ├── callbacks/
│       │   │   ├── image_notify.c   # driver/dll loads
│       │   │   ├── process_notify.c
│       │   │   └── object_callbacks.c  # handle create to game
│       │   ├── integrity/
│       │   │   ├── callback_audit.c # own notify chain health
│       │   │   └── module_hash.c
│       │   ├── driver_guard/
│       │   │   ├── blocklist.c
│       │   │   ├── allowlist.c
│       │   │   └── device_watch.c   # suspicious devices / IOCTL openers
│       │   ├── trust/
│       │   │   ├── vbs_status.c
│       │   │   └── secure_boot.c
│       │   └── comm/
│       │       └── to_usermode.c
│       └── elam/                    # optional early launch anti-malware
│           └── elam_driver/
│
├── server/                          # ★ durable controls
│   ├── interest_mgmt/               # fog-of-war / replication filter
│   │   ├── visibility.cpp
│   │   ├── audio_interest.cpp
│   │   └── replication_budget.cpp
│   ├── demo/
│   │   ├── recorder.cpp
│   │   └── storage/
│   ├── info_advantage/              # behavioral vs server truth
│   │   ├── features.cpp             # pre-aim w/o vision/sound
│   │   ├── scorer.cpp
│   │   └── overwatch_queue.cpp
│   ├── ban/
│   │   ├── correlator.cpp           # client signals + scores + graph
│   │   ├── delayed_ban.cpp
│   │   └── appeals_hooks.cpp
│   └── session/
│       └── attestation_gate.cpp     # reject ranked if policy fail
│
├── intel/                           # ops / threat intel
│   ├── byovd_catalog/               # vulnerable driver hashes/certs
│   ├── bridge_catalog/              # known HV bridge names/hashes
│   ├── c2_domains/                  # radar SaaS (policy permitting)
│   └── seller_clusters/             # HWID/payment/IP graphs
│
├── redteam_lab/                     # internal only — fixtures, not public cheats
│   ├── README.md                    # safety + legal boundary
│   ├── fixtures/
│   │   ├── fake_rpm_reader/         # benign test: open+read test process
│   │   ├── fake_ioctl_device/       # mock device for agent tests
│   │   └── policy_matrix/           # VBS on/off harness notes
│   └── scenarios/
│       ├── t0_handle.yaml
│       ├── t1_syscall_notes.md
│       ├── t2_blocklist.yaml
│       └── t3_policy_gate.yaml
│
├── common/
│   ├── proto/                       # telemetry schema
│   │   ├── handle_event.proto
│   │   ├── driver_event.proto
│   │   ├── trust_event.proto
│   │   ├── hv_probe.proto
│   │   └── info_adv_feature.proto
│   └── lib/
│       └── risk_score.cpp
│
├── cloud/
│   ├── ingest/
│   ├── rules_engine/
│   ├── blocklist_publish/           # push BYOVD updates to clients
│   └── ml/                          # optional later; not primary for radar
│       └── info_advantage/
│
└── ops/
    ├── dashboards/
    ├── runbooks/
    │   ├── banwave_t2_byovd.md
    │   ├── ranked_vbs_enforcement.md
    │   └── false_positive_handles.md
    └── metrics/
        ├── detection_latency_slo.md
        └── tier_coverage.md
```

## Minimum viable blue (MVP) vs full

### MVP (covers assumed userbase T0–T2)

```text
blue-mvp/
├── client/usermode/handle_graph/
├── client/kernel/driver_guard/ + image_notify
├── policy/driver_blocklist.json
├── server/info_advantage/ (basic features)
└── redteam_lab/scenarios/t0_handle.yaml, t2_blocklist.yaml
```

### Full (adds T3 pressure + design)

```text
+ policy/ranked_requirements (VBS/HVCI)
+ client hv_probe + trust status
+ server interest_mgmt (fog-of-war)
+ attestation_gate for ranked
+ intel/bridge_catalog
```

## Ownership by red tier

| Red tier | Blue paths that must fire |
|----------|---------------------------|
| T0 | `handle_graph`, `process_watch` |
| T1 | same + less reliance on usermode API hooks |
| T2 | `driver_guard`, `image_notify`, `blocklist`, object callbacks |
| T3 | `policy`, `trust`, `hv_probe`, `bridge` intel, fallbacks T0–T2 |
| All | `interest_mgmt`, `info_advantage`, `ban/correlator` |
