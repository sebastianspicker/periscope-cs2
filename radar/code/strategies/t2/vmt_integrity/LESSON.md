# vmt_integrity — CS2 VMT Integrity

Family: Detection. Tiers: T2. Area: Message 160 inventory.

## Battlefield
Red: Changes a simulated virtual-function target.
Blue: Collects 112 interface VMT entries and validates the target against expected
module-relative provenance, while the shared sensor also computes interface CRC data.

## Takeaway
Changing a dispatch target creates an integrity mismatch even when the surrounding
module is still present in the process.
