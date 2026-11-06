# 166_section_entropy_probe - High-entropy section vs patch-free decode

Family: Evasion. Tiers: T1. Area: t1. Sim only.

## Battlefield

Red: Ship an offline-resolved address registry in an encrypted non-image
section and decode it into registers at use. The data section is never
patched, so a decode-time patch heuristic sees nothing.

Blue: Measure entropy of non-image sections and sample it repeatedly. High,
stable entropy is the remaining scar; a manifest is the mitigation.

Arena: sim::World only.

## RED

1. Spawn a loader.
2. Arm the encrypted address registry (`encrypted_addr_registry_active`,
   `addr_registry_section_encrypted`) with a high section entropy score.
3. Open a VM_READ handle.
4. Decode register-only (`decode_register_only`), leaving
   `registry_data_section_patched` false and `registry_decode_ops` zero.
5. Read through the registry.

Achieved when: register-only decode works with no data-section patch.

## BLUE

1. Arm the entropy probe; flag scores >= 70.
2. Flag the encrypted registry section.
3. Flag that the patch heuristic is blind here (register-only, no patch).
4. Require the entropy to stay high across samples.
5. Corroborate with read telemetry.
6. Mitigation: require a manifest for high-entropy non-image sections.

Win: `detected := signals >= 2 && probe && entropy >= 70 && encrypted`,
`mitigated := detected && risk >= 0.70`.

## Takeaway

"Patch-free" is not "artifact-free". Removing the data-section write moves the
scar from *mutation* to *shape*: an encrypted, high-entropy, non-image
section. Detection follows the artifact that remains, not the one red removed.
