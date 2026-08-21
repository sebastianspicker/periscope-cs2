# Crosscutting strategies

Apply on top of any delivery tier. These are product, evasion, ops, and server-design lessons, not a sixth way to read memory.

Path: code/strategies/

Each strategy folder:

```text
name/
├── LESSON.md
├── red_example.hpp / .cpp    examples::<name>::apply
├── blue_example.hpp / .cpp   detect / mitigate
└── pair.cpp                  strategy_lab registration
```

## Evasion family — 9 pairs

| Theme | Example IDs | Multi-step idea |
|-------|-------------|-----------------|
| Staged loader | 12_staged_loader | Stub to payload to manual map to CDN to mapper |
| Crypto / poly | 13_obfuscation_crypto, 75_polymorphic_build, 87_build_watermark | Unique build + handle; YARA miss |
| Offset C2 | 14_offset_c2 | Handle and offset CDN intel |
| HWID | 15_hwid_spoof | Spoof + payment/IP cluster accounts |
| Anti-RE | 52_anti_re_canary | Analysis host + canary + debug posture |
| VEH CF | 63_veh_exception_cf | Dirty VEH chain + handlers + correlate |
| Clipboard | 99_clipboard_token | Leak + loader + net + handle |

Demo: ./build/evasion_lab

## Features family — 5 pairs

| Theme | Example IDs | Multi-step idea |
|-------|-------------|-----------------|
| Soft aim | 11_aim_humanization | Rage snaps then soft inputs; snap slips; radar residual |
| Silent aim | 62_silent_aim_desync | Camera != server aim multi-sample + challenges |
| Input synth | 19_input_synthesis | Memory-clean + multi arduino/kmbox + mixed |
| Overlay ESP | 09_overlay_esp | Overlay and handle (data path) |
| Phone radar | 10_web_phone_radar | PC reader and SaaS net; no overlay |

Demo: ./build/features_lab

## Ops family — 8 pairs

| Theme | Example IDs | Multi-step idea |
|-------|-------------|-----------------|
| Account graph | 23_account_graph | SellerFusion + smurf/banned + VPN/C2 |
| Report velocity | 51_report_velocity | Multi seats same payment high reports |
| VPN graph | 86_vpn_proxy_graph | vpn flag and shared vpn_* ip_class |
| Network C2 | 28_network_c2_intel | Intel nets and technical handle (never C2 alone) |
| AC integrity | 41_ac_self_integrity | text_hash and hook residual; fail-closed |
| Input provenance | 22_input_provenance | Inject + raw mix multi-sample |
| Overwatch queue | 74_overwatch_queue | Weak handle and delayed queue |
| Multi-signal score | 98_overwatch_multisignal | Handle+vpn+poly score >= 2 |

Demo: ./build/ops_lab

Shared helpers: depth::SellerFusionCorrelator, depth::MultiInvariantScorer

## Structural family — 5 pairs

| Theme | Example IDs | Multi-step idea |
|-------|-------------|-----------------|
| Interest / fog | 24_interest_mgmt | Full-repl want to fog + crypto strip key |
| Info-advantage | 25_info_advantage | Pre-aim frames + multi-invariant residual |
| Delayed ban | 26_delayed_ban | Weak multi-scars to BanCorrelator delayed action |
| Stream crypto | 40_entity_stream_crypto | Client key exfil vs keyless + fog |
| Fallback chain | 20_fallback_chain | HV fail to driver to RPM; blue stacks sensors |

Demo: ./build/structural_lab

## Family counts (live)

From ./build/strategy_lab stats (includes tier strategies):

| Family | Count |
|--------|------:|
| Delivery | 24 |
| Feature | 12 |
| Evasion | 44 |
| Detection | 10 |
| Structural | 9 |
| Total | 99 |

All strategies live under `code/strategies/`. Family is metadata in the strategy catalog (Delivery / Feature / Evasion / Detection / Structural), not a second directory tree.
